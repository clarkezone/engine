// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/system_utils.h"

#include <Windows.h>

#include <sstream>

#include "flutter/shell/platform/windows/string_conversion.h"

namespace flutter {

std::vector<LanguageInfo> GetPreferredLanguageInfo() {
  std::vector<std::wstring> languages = GetPreferredLanguages();
  std::vector<LanguageInfo> language_info;
  for (auto language : languages) {
    language_info.push_back(ParseLanguageName(language));
  }
  return language_info;
}

std::vector<std::wstring> GetPreferredLanguages() {
  std::vector<std::wstring> languages;
  // TODO populate via WinRT

  languages.push_back(L"en-US");
  languages.push_back(L"en");

  return languages;
}

LanguageInfo ParseLanguageName(std::wstring language_name) {
  LanguageInfo info;

  // Split by '-', discarding any suplemental language info (-x-foo).
  std::vector<std::string> components;
  std::istringstream stream(Utf8FromUtf16(language_name));
  std::string component;
  while (getline(stream, component, '-')) {
    if (component == "x") {
      break;
    }
    components.push_back(component);
  }

  // Determine which components are which.
  info.language = components[0];
  if (components.size() == 3) {
    info.script = components[1];
    info.region = components[2];
  } else if (components.size() == 2) {
    // A script code will always be four characters long.
    if (components[1].size() == 4) {
      info.script = components[1];
    } else {
      info.region = components[1];
    }
  }
  return info;
}

}  // namespace flutter
