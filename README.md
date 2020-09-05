Flutter Engine - Windows UWP experiment
==============

This fork of the Flutter Engine repo contains an experimental implementation of a Windows UWP embedder.  It is a work in progress that is not ready for production.
- All work here constitutes a spare time, best effort project that is not sponsored or endorsed by my employer.
- For a working Flutter UWP example that leverages this fork, please see https://github.com/clarkezone/fluttergalleryuwp

## September 2020 Progress Update
- ### The following capabilites are working as of September 2020:
    - Builds side-by-side with the win32 target:
        - Requires Windows SDK 10.0.18362.0
        - Configure target with `python .\flutter\tools\gn --runtime-mode=debug --unoptimized --winrt`
    - Rendering via LibAngle running in `angle_is_winuwp` mode
    - UWP compatible windowing (`CoreWindow`) and input
    - Mouse, basic keyboard and gamepad input
    - Basic XBOX mouse cursor
    - A proof-of-concept test runner implementation based on `IFrameworkViewSource` and `IFrameworkView`
    - AppContainer sandbox support
        - Windows Application Compatibility toolkit clean for Windows Store verified by successful store submission
        - Verified on UWP target platforms: XBOX and Windows 10X
- ### Remaining big rocks for parity with Flutter win32
    - Complete plugin support.  This has been working but is temporarily disabled
    - Full featured production quality runner implementation with support for UWP app lifecycle, clipboard, correct support for UWP navigation etc
    - Tests fully enabled and UWP coverage added
    - Soft / Touch keyboard support
    - Keyboard modifiers, correct unicode handling
    - Submit updates for UWP compatibility changes to dependent projects
        - Angle
        - SKIA
        - Dart
        - Buildroot
    - Secondary mouse buttons, font changes
    - ARM64 target
    - Tooling support
        - Integrate with VS Code console
        - flutter create, flutter run etc
        - observatory and friends
- ### Known Issues
    - Text input on XBOX requires a physical keyboard, text pad
    - Some touch gestures don't work
    - No support for up, down, left, right style controller navigation

## Build instructions
- Setup a Flutter engine development environment: 
    - https://github.com/flutter/flutter/wiki/Setting-up-the-Engine-development-environment
    - https://github.com/flutter/flutter/wiki/Compiling-the-engine#compiling-for-windows
- Recommended: use `c:\src\f` as your root directory
    - This will make it easier to get the Flutter Gallery example working
- Configure the winrt target
    - `python .\flutter\tools\gn --runtime-mode=debug --unoptimized --winrt`
- Compile it
    - `ninja -C out\winuwp_debug_unopt`
- The resulting `flutter_windows_winrt.dll` binary can be used in conjunction with  https://github.com/clarkezone/fluttergalleryuwp



Flutter Engine README.md
========================

[![Build Status - Cirrus][]][Build status]

Flutter is Google's mobile app SDK for crafting high-quality native interfaces
in record time. Flutter works with existing code, is used by developers and
organizations around the world, and is free and open source.

The Flutter Engine is a portable runtime for hosting
[Flutter](https://flutter.dev) applications.  It implements Flutter's core
libraries, including animation and graphics, file and network I/O,
accessibility support, plugin architecture, and a Dart runtime and compile
toolchain. Most developers will interact with Flutter via the [Flutter
Framework](https://github.com/flutter/flutter), which provides a modern,
reactive framework, and a rich set of platform, layout and foundation widgets.

If you want to run/contribute to Flutter Web engine, more tooling can be
found at [felt](https://github.com/flutter/engine/tree/master/lib/web_ui/dev#whats-felt).
This is a tool written to make web engine development experience easy.

If you are new to Flutter, then you will find more general information
on the Flutter project, including tutorials and samples, on our Web
site at [Flutter.dev](https://flutter.dev). For specific information
about Flutter's APIs, consider our API reference which can be found at
the [docs.flutter.dev](https://docs.flutter.dev/).

Flutter is a fully open source project, and we welcome contributions.
Information on how to get started can be found at our
[contributor guide](CONTRIBUTING.md).

[Build Status - Cirrus]: https://api.cirrus-ci.com/github/flutter/engine.svg?branch=master
[Build status]: https://cirrus-ci.com/github/flutter/engine

