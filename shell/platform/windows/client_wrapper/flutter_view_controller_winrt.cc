// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/flutter/flutter_view_controller_winrt.h"

#include <algorithm>
#include <iostream>

namespace flutter {
FlutterViewControllerWinRT::FlutterViewControllerWinRT(
    ABI::Windows::UI::Core::CoreWindow* window,
                                             const DartProjectWinRT& project) {
  engine_ = std::make_unique<FlutterEngineWinRT>(project);
  controller_ = FlutterDesktopViewControllerCreateFromCoreWindow(
      window,
                                                   engine_->RelinquishEngine());
  if (!controller_) {
    std::cerr << "Failed to create view controller." << std::endl;
    return;
  }
  view_ = std::make_unique<FlutterView>(
      FlutterDesktopViewControllerGetView(controller_));
}

FlutterViewControllerWinRT::~FlutterViewControllerWinRT() {
  if (controller_) {
    FlutterDesktopViewControllerDestroy(controller_);
  }
}

std::chrono::nanoseconds FlutterViewControllerWinRT::ProcessMessages() {
  return engine_->ProcessMessages();
}

FlutterDesktopPluginRegistrarRef FlutterViewControllerWinRT::GetRegistrarForPlugin(
    const std::string& plugin_name) {
  return engine_->GetRegistrarForPlugin(plugin_name);
}

}  // namespace flutter
