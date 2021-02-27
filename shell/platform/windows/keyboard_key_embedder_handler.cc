// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/keyboard_key_embedder_handler.h"

#include <assert.h>
#include <windows.h>

#include <chrono>
#include <codecvt>
#include <iostream>
#include <string>

#include "flutter/shell/platform/windows/string_conversion.h"

namespace flutter {

namespace {
// An arbitrary size for the character cache in bytes.
//
// It should hold a UTF-32 character encoded in UTF-8 as well as the trailing
// '\0'.
constexpr size_t kCharacterCacheSize = 8;

/**
 * The code prefix for keys which do not have a Unicode representation.
 *
 * This is used by platform-specific code to generate Flutter key codes using
 * HID Usage codes.
 */
constexpr uint64_t kHidPlane = 0x00100000000;

/**
 * The code prefix for keys which have a Unicode representation.
 *
 * This is used by platform-specific code to generate Flutter key codes.
 */
constexpr uint64_t kUnicodePlane = 0x00000000000;

/**
 */
constexpr uint64_t kWindowsKeyIdPlane = 0x00700000000;

/**
 * Mask for the auto-generated bit portion of the key code.
 *
 * This is used by platform-specific code to generate new Flutter key codes
 * for keys which are not recognized.
 */
constexpr uint64_t kAutogeneratedMask = 0x10000000000;

constexpr SHORT kStateMaskToggled = 0x01;
constexpr SHORT kStateMaskPressed = 0x80;

}  // namespace

KeyboardKeyEmbedderHandler::KeyboardKeyEmbedderHandler(
    std::function<void(const FlutterKeyEvent&,
                       FlutterKeyEventCallback,
                       void* user_data)> send_event,
    GetKeyStateHandler get_key_state)
    : sendEvent_(send_event), get_key_state_(get_key_state), response_id_(1) {
  InitCriticalKeys();
}

KeyboardKeyEmbedderHandler::~KeyboardKeyEmbedderHandler() = default;

static bool isAsciiPrintable(int codeUnit) {
  return codeUnit <= 0x7f && codeUnit >= 0x20;
}

static bool isControlCharacter(int codeUnit) {
  return (codeUnit <= 0x1f && codeUnit >= 0x00) ||
         (codeUnit >= 0x7f && codeUnit <= 0x9f);
}

// Transform scancodes sent by windows to scancodes written in Chromium spec.
static uint16_t normalizeScancode(int windowsScanCode, bool extended) {
  // In Chromium spec the extended bit is shown as 0xe000 bit,
  // e.g. PageUp is represented as 0xe049.
  return (windowsScanCode & 0xff) | (extended ? 0xe000 : 0);
}

uint64_t KeyboardKeyEmbedderHandler::getPhysicalKey(int scancode,
                                                    bool extended) {
  int chromiumScancode = normalizeScancode(scancode, extended);
  auto resultIt = windowsToPhysicalMap_.find(chromiumScancode);
  if (resultIt != windowsToPhysicalMap_.end())
    return resultIt->second;
  return scancode | kHidPlane;
}

uint64_t KeyboardKeyEmbedderHandler::getLogicalKey(int key,
                                                   bool extended,
                                                   int scancode) {
  // Normally logical keys should only be derived from key codes, but since some
  // key codes are either 0 or ambiguous (multiple keys using the same key
  // code), these keys are resolved by scan codes.
  auto numpadIter =
      scanCodeToLogicalMap_.find(normalizeScancode(scancode, extended));
  if (numpadIter != scanCodeToLogicalMap_.cend())
    return numpadIter->second;

  // Check if the keyCode is one we know about and have a mapping for.
  auto logicalIt = windowsToLogicalMap_.find(key);
  if (logicalIt != windowsToLogicalMap_.cend())
    return logicalIt->second;

  // Upper case letters should be normalized into lower case letters.
  if (isAsciiPrintable(key)) {
    if (isupper(key)) {
      return tolower(key);
    }
    return key;
  }

  // For keys that do not exist in the map, if it has a non-control-character
  // label, then construct a new Unicode-based key from it. Don't mark it as
  // autogenerated, since the label uniquely identifies an ID from the Unicode
  // plane.
  if (!isControlCharacter(key)) {
    return key | kUnicodePlane;
  } else {
    // This is a non-printable key that we don't know about, so we mint a new
    // code with the autogenerated bit set.
    return key | kWindowsKeyIdPlane | kAutogeneratedMask;
  }
}

void KeyboardKeyEmbedderHandler::KeyboardHook(
    int key,
    int scancode,
    int action,
    char32_t character,
    bool extended,
    bool was_down,
    std::function<void(bool)> callback) {
  const uint64_t physical_key = getPhysicalKey(scancode, extended);
  const uint64_t logical_key = getLogicalKey(key, extended, scancode);
  assert(action == WM_KEYDOWN || action == WM_KEYUP);
  const bool is_physical_down = action == WM_KEYDOWN;

  auto last_logical_record_iter = pressingRecords_.find(physical_key);
  const bool had_record = last_logical_record_iter != pressingRecords_.end();
  const uint64_t last_logical_record =
      had_record ? last_logical_record_iter->second : 0;

  // The resulting event's `type`.
  FlutterKeyEventType type;
  // The resulting event's `logical_key`.
  uint64_t result_logical_key;
  // The next value of pressingRecords_[physical_key] (or to remove it).
  uint64_t next_logical_record;
  bool next_has_record = true;
  char character_bytes[kCharacterCacheSize];

  if (is_physical_down) {
    if (had_record) {
      if (was_down) {
        // A normal repeated key.
        type = kFlutterKeyEventTypeRepeat;
        assert(had_record);
        ConvertUtf32ToUtf8_(character_bytes, character);
        next_logical_record = last_logical_record;
        result_logical_key = last_logical_record;
      } else {
        // A non-repeated key has been pressed that has the exact physical key
        // as a currently pressed one, usually indicating multiple keyboards are
        // pressing keys with the same physical key, or the up event was lost
        // during a loss of focus. The down event is ignored.
        callback(true);
        return;
      }
    } else {
      // A normal down event (whether the system event is a repeat or not).
      type = kFlutterKeyEventTypeDown;
      assert(!had_record);
      ConvertUtf32ToUtf8_(character_bytes, character);
      next_logical_record = logical_key;
      result_logical_key = logical_key;
    }
  } else {  // isPhysicalDown is false
    if (last_logical_record == 0) {
      // The physical key has been released before. It might indicate a missed
      // event due to loss of focus, or multiple keyboards pressed keys with the
      // same physical key. Ignore the up event.
      callback(true);
      return;
    } else {
      // A normal up event.
      type = kFlutterKeyEventTypeUp;
      assert(had_record);
      // Up events never have character.
      character_bytes[0] = '\0';
      next_has_record = false;
      result_logical_key = last_logical_record;
    }
  }

  UpdateLastSeenCritialKey(key, physical_key, result_logical_key);
  SynchronizeCritialToggledStates(type == kFlutterKeyEventTypeDown ? key : 0);

  if (next_has_record) {
    pressingRecords_[physical_key] = next_logical_record;
  } else {
    pressingRecords_.erase(last_logical_record_iter);
  }

  SynchronizeCritialPressedStates();

  if (result_logical_key == VK_PROCESSKEY) {
    // VK_PROCESSKEY means that the key press is used by an IME. These key
    // presses are considered handled and not sent to Flutter. These events must
    // be filtered by result_logical_key because the key up event of such
    // presses uses the "original" logical key.
    callback(true);
    return;
  }

  FlutterKeyEvent key_data{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count()),
      .type = type,
      .physical = physical_key,
      .logical = result_logical_key,
      .character = character_bytes,
      .synthesized = false,
  };

  response_id_ += 1;
  uint64_t response_id = response_id_;
  PendingResponse pending{
      .callback =
          [this, callback = std::move(callback)](bool handled,
                                                 uint64_t reponse_id) {
            auto found = pending_responses_.find(reponse_id);
            if (found != pending_responses_.end()) {
              pending_responses_.erase(found);
            }
            callback(handled);
          },
      .response_id = response_id,
  };
  auto pending_ptr = std::make_unique<PendingResponse>(std::move(pending));
  pending_responses_[response_id] = std::move(pending_ptr);
  sendEvent_(key_data, KeyboardKeyEmbedderHandler::HandleResponse,
             reinterpret_cast<void*>(pending_responses_[response_id].get()));
}

void KeyboardKeyEmbedderHandler::UpdateLastSeenCritialKey(
    int virtual_key,
    uint64_t physical_key,
    uint64_t logical_key) {
  auto found = critical_keys_.find(virtual_key);
  if (found != critical_keys_.end()) {
    found->second.physical_key = physical_key;
    found->second.logical_key = logical_key;
  }
}

void KeyboardKeyEmbedderHandler::SynchronizeCritialToggledStates(
    int toggle_virtual_key) {
  for (auto& kv : critical_keys_) {
    UINT virtual_key = kv.first;
    CriticalKey& key_info = kv.second;
    if (key_info.physical_key == 0) {
      // Never seen this key.
      continue;
    }
    assert(key_info.logical_key != 0);
    SHORT state = get_key_state_(virtual_key);

    // Check toggling state first, because it might alter pressing state.
    if (key_info.check_toggled) {
      bool should_toggled = state & kStateMaskToggled;
      if (virtual_key == toggle_virtual_key) {
        key_info.toggled_on = !key_info.toggled_on;
      }
      if (key_info.toggled_on != should_toggled) {
        const char* empty_character = "";
        // If the key is pressed, release it first.
        if (pressingRecords_.find(key_info.physical_key) !=
            pressingRecords_.end()) {
          sendEvent_(SynthesizeSimpleEvent(
                         kFlutterKeyEventTypeUp, key_info.physical_key,
                         key_info.logical_key, empty_character),
                     nullptr, nullptr);
        } else {
          // This key will always be pressed in the following synthesized event.
          pressingRecords_[key_info.physical_key] = key_info.logical_key;
        }
        sendEvent_(SynthesizeSimpleEvent(kFlutterKeyEventTypeDown,
                                         key_info.physical_key,
                                         key_info.logical_key, empty_character),
                   nullptr, nullptr);
      }
      key_info.toggled_on = should_toggled;
    }
  }
}

void KeyboardKeyEmbedderHandler::SynchronizeCritialPressedStates() {
  for (auto& kv : critical_keys_) {
    UINT virtual_key = kv.first;
    CriticalKey& key_info = kv.second;
    if (key_info.physical_key == 0) {
      // Never seen this key.
      continue;
    }
    assert(key_info.logical_key != 0);
    SHORT state = get_key_state_(virtual_key);
    if (key_info.check_pressed) {
      auto recorded_pressed_iter = pressingRecords_.find(key_info.physical_key);
      bool recorded_pressed = recorded_pressed_iter != pressingRecords_.end();
      bool should_pressed = state & kStateMaskPressed;
      if (recorded_pressed != should_pressed) {
        if (should_pressed) {
          pressingRecords_[key_info.physical_key] = key_info.logical_key;
        } else {
          pressingRecords_.erase(recorded_pressed_iter);
        }
        const char* empty_character = "";
        sendEvent_(
            SynthesizeSimpleEvent(should_pressed ? kFlutterKeyEventTypeDown
                                                 : kFlutterKeyEventTypeUp,
                                  key_info.physical_key, key_info.logical_key,
                                  empty_character),
            nullptr, nullptr);
      }
    }
  }
}

void KeyboardKeyEmbedderHandler::HandleResponse(bool handled, void* user_data) {
  PendingResponse* pending = reinterpret_cast<PendingResponse*>(user_data);
  auto callback = std::move(pending->callback);
  callback(handled, pending->response_id);
}

void KeyboardKeyEmbedderHandler::InitCriticalKeys() {
  auto createCheckedKey = [this](UINT virtual_key, bool extended,
                                 bool check_pressed,
                                 bool check_toggled) -> CriticalKey {
    #if defined(WINUWP)
return CriticalKey{};
    #else
    UINT scan_code = MapVirtualKey(virtual_key, MAPVK_VK_TO_VSC);
    return CriticalKey{
        .physical_key = getPhysicalKey(scan_code, extended),
        .logical_key = getLogicalKey(virtual_key, extended, scan_code),
        .check_pressed = check_pressed || check_toggled,
        .check_toggled = check_toggled,
        .toggled_on = check_toggled
                          ? !!(get_key_state_(virtual_key) & kStateMaskToggled)
                          : false,
    };
    #endif
  };

  // TODO(dkwingsmt): Consider adding more critical keys here.
  // https://github.com/flutter/flutter/issues/76736
  critical_keys_.emplace(VK_LSHIFT,
                         createCheckedKey(VK_LSHIFT, false, true, false));
  critical_keys_.emplace(VK_RSHIFT,
                         createCheckedKey(VK_RSHIFT, false, true, false));
  critical_keys_.emplace(VK_LCONTROL,
                         createCheckedKey(VK_LCONTROL, false, true, false));
  critical_keys_.emplace(VK_RCONTROL,
                         createCheckedKey(VK_RCONTROL, true, true, false));

  critical_keys_.emplace(VK_CAPITAL,
                         createCheckedKey(VK_CAPITAL, false, true, true));
  critical_keys_.emplace(VK_SCROLL,
                         createCheckedKey(VK_SCROLL, false, true, true));
  critical_keys_.emplace(VK_NUMLOCK,
                         createCheckedKey(VK_NUMLOCK, true, true, true));
}

void KeyboardKeyEmbedderHandler::ConvertUtf32ToUtf8_(char* out, char32_t ch) {
  if (ch == 0) {
    out[0] = '\0';
    return;
  }
  // TODO: Correctly handle UTF-32
  std::wstring text({static_cast<wchar_t>(ch)});
  strcpy_s(out, kCharacterCacheSize, Utf8FromUtf16(text).c_str());
}

FlutterKeyEvent KeyboardKeyEmbedderHandler::SynthesizeSimpleEvent(
    FlutterKeyEventType type,
    uint64_t physical,
    uint64_t logical,
    const char* character) {
  return FlutterKeyEvent{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count()),
      .type = type,
      .physical = physical,
      .logical = logical,
      .character = character,
      .synthesized = true,
  };
}

}  // namespace flutter
