// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_PUBLIC_FLUTTER_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_PUBLIC_FLUTTER_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#ifdef FLUTTER_WINRT
#include <windows.ui.core.h>
#endif

#include "flutter_export.h"
#include "flutter_messenger.h"
#include "flutter_plugin_registrar.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Opaque reference to a Flutter window controller.
typedef struct FlutterDesktopViewControllerState*
    FlutterDesktopViewControllerRef;

// Opaque reference to a Flutter window.
struct FlutterDesktopView;
typedef struct FlutterDesktopView* FlutterDesktopViewRef;

// Opaque reference to a Flutter engine instance.
struct FlutterDesktopEngine;
typedef struct FlutterDesktopEngine* FlutterDesktopEngineRef;

// Properties for configuring a Flutter engine instance.
typedef struct {
  // The path to the flutter_assets folder for the application to be run.
  // This can either be an absolute path or a path relative to the directory
  // containing the executable.
  const wchar_t* assets_path;

  // The path to the icudtl.dat file for the version of Flutter you are using.
  // This can either be an absolute path or a path relative to the directory
  // containing the executable.
  const wchar_t* icu_data_path;

  // The path to the AOT libary file for your application, if any.
  // This can either be an absolute path or a path relative to the directory
  // containing the executable. This can be nullptr for a non-AOT build, as
  // it will be ignored in that case.
  const wchar_t* aot_library_path;

  // The switches to pass to the Flutter engine.
  //
  // See: https://github.com/flutter/engine/blob/master/shell/common/switches.h
  // for details. Not all arguments will apply to desktop.
  const char** switches;

  // The number of elements in |switches|.
  size_t switches_count;
} FlutterDesktopEngineProperties;

// ========== View Controller ==========

// Creates a view that hosts and displays the given engine instance.
//
// This takes ownership of |engine|, so FlutterDesktopEngineDestroy should no
// longer be called on it, as it will be called internally when the view
// controller is destroyed. If creating the view controller fails, the engine
// will be destroyed immediately.
//
// If |engine| is not already running, the view controller will start running
// it automatically before displaying the window.
//
// The caller owns the returned reference, and is responsible for calling
// FlutterDesktopViewControllerDestroy. Returns a null pointer in the event of
// an error.
#ifndef FLUTTER_WINRT
FLUTTER_EXPORT FlutterDesktopViewControllerRef
FlutterDesktopViewControllerCreate(int width,
                                   int height,
                                   FlutterDesktopEngineRef engine);
#endif

//TODO
#ifdef FLUTTER_WINRT
FLUTTER_EXPORT FlutterDesktopViewControllerRef
FlutterDesktopViewControllerCreateFromCoreWindow(
    ABI::Windows::UI::Core::CoreWindow* window,
                                   FlutterDesktopEngineRef engine);
#endif

// Shuts down the engine instance associated with |controller|, and cleans up
// associated state.
//
// |controller| is no longer valid after this call.
FLUTTER_EXPORT void FlutterDesktopViewControllerDestroy(
    FlutterDesktopViewControllerRef controller);

// Returns the handle for the engine running in FlutterDesktopViewControllerRef.
//
// Its lifetime is the same as the |controller|'s.
FLUTTER_EXPORT FlutterDesktopEngineRef FlutterDesktopViewControllerGetEngine(
    FlutterDesktopViewControllerRef controller);
// Returns the view managed by the given controller.

FLUTTER_EXPORT FlutterDesktopViewRef
FlutterDesktopViewControllerGetView(FlutterDesktopViewControllerRef controller);

// Allows the Flutter engine and any interested plugins an opportunity to
// handle the given message.
//
// If the WindowProc was handled and further handling should stop, this returns
// true and |result| will be populated. |result| is not set if returning false.
#ifndef FLUTTER_WINRT
FLUTTER_EXPORT bool FlutterDesktopViewControllerHandleTopLevelWindowProc(
    FlutterDesktopViewControllerRef controller,
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    LRESULT* result);
#endif

// ========== Engine ==========

// Creates a Flutter engine with the given properties.
//
// The caller owns the returned reference, and is responsible for calling
// FlutterDesktopEngineDestroy.
FLUTTER_EXPORT FlutterDesktopEngineRef FlutterDesktopEngineCreate(
    const FlutterDesktopEngineProperties& engine_properties);

// Shuts down and destroys the given engine instance. Returns true if the
// shutdown was successful, or if the engine was not running.
//
// |engine| is no longer valid after this call.
FLUTTER_EXPORT bool FlutterDesktopEngineDestroy(FlutterDesktopEngineRef engine);

// Starts running the given engine instance and optional entry point in the Dart
// project. If the entry point is null, defaults to main().
//
// If provided, entry_point must be the name of a top-level function from the
// same Dart library that contains the app's main() function, and must be
// decorated with `@pragma(vm:entry-point)` to ensure the method is not
// tree-shaken by the Dart compiler.
//
// Returns false if running the engine failed.
FLUTTER_EXPORT bool FlutterDesktopEngineRun(FlutterDesktopEngineRef engine,
                                            const char* entry_point);

// Processes any pending events in the Flutter engine, and returns the
// number of nanoseconds until the next scheduled event (or max, if none).
//
// This should be called on every run of the application-level runloop, and
// a wait for native events in the runloop should never be longer than the
// last return value from this function.
FLUTTER_EXPORT uint64_t
FlutterDesktopEngineProcessMessages(FlutterDesktopEngineRef engine);

FLUTTER_EXPORT void FlutterDesktopEngineReloadSystemFonts(
    FlutterDesktopEngineRef engine);

// Returns the plugin registrar handle for the plugin with the given name.
//
// The name must be unique across the application.
FLUTTER_EXPORT FlutterDesktopPluginRegistrarRef
FlutterDesktopEngineGetPluginRegistrar(FlutterDesktopEngineRef engine,
                                       const char* plugin_name);

// Returns the messenger associated with the engine.
FLUTTER_EXPORT FlutterDesktopMessengerRef
FlutterDesktopEngineGetMessenger(FlutterDesktopEngineRef engine);

// ========== View ==========

// Return backing HWND for manipulation in host application.
FLUTTER_EXPORT HWND FlutterDesktopViewGetHWND(FlutterDesktopViewRef view);

// ========== Plugin Registrar (extensions) ==========
// These are Windows-specific extensions to flutter_plugin_registrar.h

// Function pointer type for top level WindowProc delegate registration.
//
// The user data will be whatever was passed to
// FlutterDesktopRegisterTopLevelWindowProcHandler.
//
// Implementations should populate |result| and return true if the WindowProc
// was handled and further handling should stop. |result| is ignored if the
// function returns false.
typedef bool (*FlutterDesktopWindowProcCallback)(HWND /* hwnd */,
                                                 UINT /* uMsg */,
                                                 WPARAM /*wParam*/,
                                                 LPARAM /* lParam*/,
                                                 void* /* user data */,
                                                 LRESULT* result);

// Returns the view associated with this registrar's engine instance.
FLUTTER_EXPORT FlutterDesktopViewRef FlutterDesktopPluginRegistrarGetView(
    FlutterDesktopPluginRegistrarRef registrar);

FLUTTER_EXPORT void
FlutterDesktopPluginRegistrarRegisterTopLevelWindowProcDelegate(
    FlutterDesktopPluginRegistrarRef registrar,
    FlutterDesktopWindowProcCallback delegate,
    void* user_data);

FLUTTER_EXPORT void
FlutterDesktopPluginRegistrarUnregisterTopLevelWindowProcDelegate(
    FlutterDesktopPluginRegistrarRef registrar,
    FlutterDesktopWindowProcCallback delegate);

// ========== Freestanding Utilities ==========

// Gets the DPI for a given |hwnd|, depending on the supported APIs per
// windows version and DPI awareness mode. If nullptr is passed, returns the DPI
// of the primary monitor.
//
// This uses the same logic and fallback for older Windows versions that is used
// internally by Flutter to determine the DPI to use for displaying Flutter
// content, so should be used by any code (e.g., in plugins) that translates
// between Windows and Dart sizes/offsets.
FLUTTER_EXPORT UINT FlutterDesktopGetDpiForHWND(HWND hwnd);

// Gets the DPI for a given |monitor|. If the API is not available, a default
// DPI of 96 is returned.
//
// See FlutterDesktopGetDpiForHWND for more information.
FLUTTER_EXPORT UINT FlutterDesktopGetDpiForMonitor(HMONITOR monitor);

// Reopens stdout and stderr and resysncs the standard library output streams.
// Should be called if output is being directed somewhere in the runner process
// (e.g., after an AllocConsole call).
FLUTTER_EXPORT void FlutterDesktopResyncOutputStreams();

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_PUBLIC_FLUTTER_WINDOWS_H_
