// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_CLIENT_WRAPPER_INCLUDE_FLUTTER_FLUTTER_ENGINE_WINRT_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_CLIENT_WRAPPER_INCLUDE_FLUTTER_FLUTTER_ENGINE_WINRT_H_

#define NOMINMAX
#include <flutter_windows.h>

#include <chrono>
#include <memory>
#include <string>

#include "binary_messenger.h"
#include "dart_project_winrt.h"
#include "plugin_registrar.h"
#include "plugin_registry.h"

namespace flutter {

// An instance of a Flutter engine.
//
// In the future, this will be the API surface used for all interactions with
// the engine, rather than having them duplicated on FlutterViewController.
// For now it is only used in the rare where you need a headless Flutter engine.
class FlutterEngineWinRT : public PluginRegistry {
 public:
  // Creates a new engine for running the given project.
  explicit FlutterEngineWinRT(const DartProjectWinRT& project);

  virtual ~FlutterEngineWinRT();

  // Prevent copying.
  FlutterEngineWinRT(FlutterEngineWinRT const&) = delete;
  FlutterEngineWinRT& operator=(FlutterEngineWinRT const&) = delete;

  // Starts running the engine, with an optional entry point.
  //
  // If provided, entry_point must be the name of a top-level function from the
  // same Dart library that contains the app's main() function, and must be
  // decorated with `@pragma(vm:entry-point)` to ensure the method is not
  // tree-shaken by the Dart compiler. If not provided, defaults to main().
  bool Run(const char* entry_point = nullptr);

  // Terminates the running engine.
  void ShutDown();

  // Processes any pending events in the Flutter engine, and returns the
  // nanosecond delay until the next scheduled event (or  max, if none).
  //
  // This should be called on every run of the application-level runloop, and
  // a wait for native events in the runloop should never be longer than the
  // last return value from this function.
  std::chrono::nanoseconds ProcessMessages();

  // flutter::PluginRegistry:
  FlutterDesktopPluginRegistrarRef GetRegistrarForPlugin(
      const std::string& plugin_name) override;

  // Returns the messenger to use for creating channels to communicate with the
  // Flutter engine.
  //
  // This pointer will remain valid for the lifetime of this instance.
  BinaryMessenger* messenger() { return messenger_.get(); }

 private:
  // For access to RelinquishEngine.
  friend class FlutterViewControllerWinRT;

  // Gives up ownership of |engine_|, but keeps a weak reference to it.
  //
  // This is intended to be used by FlutterViewController, since the underlying
  // C API for view controllers takes over engine ownership.
  FlutterDesktopEngineRef RelinquishEngine();

  // Handle for interacting with the C API's engine reference.
  FlutterDesktopEngineRef engine_ = nullptr;

  // Messenger for communicating with the engine.
  std::unique_ptr<BinaryMessenger> messenger_;

  // Whether or not this wrapper owns |engine_|.
  bool owns_engine_ = true;

  // Whether the engine has been run. This will be true if Run has been called,
  // or if RelinquishEngine has been called (since the view controller will
  // run the engine if it hasn't already been run).
  bool has_been_run_ = false;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_CLIENT_WRAPPER_INCLUDE_FLUTTER_FLUTTER_ENGINE_WINRT_H_
