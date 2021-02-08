// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/flutter_window_winuwp.h"

namespace flutter {

// Multipler used to map controller velocity to an appropriate scroll input.
static constexpr double kControllerScrollMultiplier = 3;

FlutterWindowWinUWP::FlutterWindowWinUWP(
    ABI::Windows::UI::Core::CoreWindow* window) {
  winrt::Windows::UI::Core::CoreWindow cw{nullptr};
  winrt::copy_from_abi(cw, window);
  window_ = cw;

  SetEventHandlers();
  ConfigureGamePad();
  ConfigureXboxSpecific();

  current_display_info_ = winrt::Windows::Graphics::Display::
      DisplayInformation::GetForCurrentView();
}

WindowsRenderTarget FlutterWindowWinUWP::GetRenderTarget() {
#ifdef USECOREWINDOW
  return WindowsRenderTarget(window_);
#else
  compositor_ = winrt::Windows::UI::Composition::Compositor();
  target_ = compositor_.CreateTargetForCurrentView();
  visual_tree_root_ = compositor_.CreateContainerVisual();
  target_.Root(visual_tree_root_);

  cursor_visual_ = CreateCursorVisual();

  render_target_ = compositor_.CreateSpriteVisual();
  if (running_on_xbox_) {
    render_target_.Offset(
        {xbox_overscan_x_offset_, xbox_overscan_y_offset_, 1.0});
  } else {
    render_target_.Offset({1.0, 1.0, 1.0});
    ApplyInverseDpiScalingTransform();
  }
  visual_tree_root_.Children().InsertAtBottom(render_target_);

  WindowBoundsWinUWP bounds = GetBounds(current_display_info_, true);

  render_target_.Size({bounds.width, bounds.height});
  return WindowsRenderTarget(render_target_);
#endif
}

void FlutterWindowWinUWP::ApplyInverseDpiScalingTransform() {
  // Apply inverse transform to negate built in DPI scaling in order to render
  // at native scale.
  auto dpiScale = GetDpiScale();
  render_target_.Scale({1 / dpiScale, 1 / dpiScale, 1 / dpiScale});
}

PhysicalWindowBounds FlutterWindowWinUWP::GetPhysicalWindowBounds() {
  WindowBoundsWinUWP bounds = GetBounds(current_display_info_, true);
  return {static_cast<size_t>(bounds.width),
          static_cast<size_t>(bounds.height)};
}

void FlutterWindowWinUWP::UpdateFlutterCursor(const std::string& cursor_name) {
  // TODO(clarkezone): add support for Flutter cursors:
  // https://github.com/flutter/flutter/issues/70199
}

void FlutterWindowWinUWP::UpdateCursorRect(const Rect& rect) {
  // TODO(cbracken): Implement IMM candidate window positioning.
}

void FlutterWindowWinUWP::OnWindowResized() {}

float FlutterWindowWinUWP::GetDpiScale() {
  auto disp = winrt::Windows::Graphics::Display::DisplayInformation::
      GetForCurrentView();

  return GetDpiScale(disp);
}

WindowBoundsWinUWP FlutterWindowWinUWP::GetBounds(
    winrt::Windows::Graphics::Display::DisplayInformation const& disp,
    bool physical) {
  winrt::Windows::UI::ViewManagement::ApplicationView app_view =
      winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
  winrt::Windows::Foundation::Rect bounds = app_view.VisibleBounds();
  if (running_on_xbox_) {
    return {bounds.Width + (bounds.X), bounds.Height + (bounds.Y)};
  }

  if (physical) {
    // Return the height in physical pixels
    return {bounds.Width * static_cast<float>(disp.RawPixelsPerViewPixel()),
            bounds.Height * static_cast<float>(disp.RawPixelsPerViewPixel())};
  }

  return {bounds.Width, bounds.Height};
}

float FlutterWindowWinUWP::GetDpiScale(
    winrt::Windows::Graphics::Display::DisplayInformation const& disp) {
  double raw_per_view = disp.RawPixelsPerViewPixel();

  // TODO(clarkezone): ensure DPI handling is correct:
  // because XBOX has display scaling off, logicalDpi retuns 96 which is
  // incorrect check if raw_per_view is more acurate.
  // Also confirm if it is necessary to use this workaround on 10X
  // https://github.com/flutter/flutter/issues/70198

  if (running_on_xbox_) {
    return 1.5;
  }
  return static_cast<float>(raw_per_view);
}

FlutterWindowWinUWP::~FlutterWindowWinUWP() {}

void FlutterWindowWinUWP::SetView(WindowBindingHandlerDelegate* view) {
  binding_handler_delegate_ = view;
}

void FlutterWindowWinUWP::SetEventHandlers() {
  auto app_view =
      winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();

  app_view.SetDesiredBoundsMode(winrt::Windows::UI::ViewManagement::
                                    ApplicationViewBoundsMode::UseCoreWindow);

  app_view.VisibleBoundsChanged({this, &FlutterWindowWinUWP::OnBoundsChanged});

  window_.PointerPressed({this, &FlutterWindowWinUWP::OnPointerPressed});
  window_.PointerReleased({this, &FlutterWindowWinUWP::OnPointerReleased});
  window_.PointerMoved({this, &FlutterWindowWinUWP::OnPointerMoved});
  window_.PointerWheelChanged(
      {this, &FlutterWindowWinUWP::OnPointerWheelChanged});

  // TODO(clarkezone) support mouse leave handling
  // https://github.com/flutter/flutter/issues/70199

  // TODO(clarkezone) support system font changed
  // https://github.com/flutter/flutter/issues/70198

  window_.KeyUp({this, &FlutterWindowWinUWP::OnKeyUp});
  window_.KeyDown({this, &FlutterWindowWinUWP::OnKeyDown});
  window_.CharacterReceived({this, &FlutterWindowWinUWP::OnCharacterReceived});

  auto display = winrt::Windows::Graphics::Display::DisplayInformation::
      GetForCurrentView();
  display.DpiChanged({this, &FlutterWindowWinUWP::OnDpiChanged});
}

void FlutterWindowWinUWP::StartGamepadCursorThread() {
  if (worker_loop_ != nullptr &&
      worker_loop_.Status() ==
          winrt::Windows::Foundation::AsyncStatus::Started) {
    return;
  }

  winrt::Windows::UI::Core::CoreDispatcher dispatcher = window_.Dispatcher();

  auto workItemHandler = winrt::Windows::System::Threading::WorkItemHandler(
      [this, dispatcher](winrt::Windows::Foundation::IAsyncAction action) {
        while (action.Status() ==
               winrt::Windows::Foundation::AsyncStatus::Started) {
          dispatcher.RunAsync(
              winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
              [this, dispatcher]() { game_pad_->Process(); });
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      });

  worker_loop_ = winrt::Windows::System::Threading::ThreadPool::RunAsync(
      workItemHandler, winrt::Windows::System::Threading::WorkItemPriority::Low,
      winrt::Windows::System::Threading::WorkItemOptions::TimeSliced);
}

void FlutterWindowWinUWP::ConfigureGamePad() {
  DualAxisCallback leftStick = [=](double x, double y) {
    OnGamePadLeftStickMoved(x, y);
  };

  DualAxisCallback rightStick = [=](double x, double y) {
    OnGamePadRightStickMoved(x, y);
  };

  ButtonCallback pressedcallback =
      [=](winrt::Windows::Gaming::Input::GamepadButtons buttons) {
        OnGamePadButtonPressed(buttons);
      };

  ButtonCallback releasedcallback =
      [=](winrt::Windows::Gaming::Input::GamepadButtons buttons) {
        OnGamePadButtonReleased(buttons);
      };
  GamepadAddedRemovedCallback changed = [=]() {
    OnGamePadControllersChanged();
  };

  game_pad_ = std::make_unique<GamePadWinUWP>(leftStick, rightStick, nullptr,
                                              nullptr, pressedcallback,
                                              releasedcallback, changed);
  game_pad_->Initialize();
}

void FlutterWindowWinUWP::ConfigureXboxSpecific() {
  running_on_xbox_ =
      winrt::Windows::System::Profile::AnalyticsInfo::VersionInfo()
          .DeviceFamily() == L"Windows.Xbox";

  if (running_on_xbox_) {
    bool result =
        winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()
            .SetDesiredBoundsMode(winrt::Windows::UI::ViewManagement::
                                      ApplicationViewBoundsMode::UseCoreWindow);
    if (!result) {
      OutputDebugString(L"Couldn't set bounds mode.");
    }

    winrt::Windows::UI::ViewManagement::ApplicationView app_view = winrt::
        Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
    winrt::Windows::Foundation::Rect bounds = app_view.VisibleBounds();

    // the offset /2 represents how much off-screan the core window is
    // positioned unclear why disabling overscan doesn't correct this
    xbox_overscan_x_offset_ = bounds.X / 2;
    xbox_overscan_y_offset_ = bounds.Y / 2;
  }
}

void FlutterWindowWinUWP::OnDpiChanged(
    winrt::Windows::Graphics::Display::DisplayInformation const& args,
    winrt::Windows::Foundation::IInspectable const&) {
  ApplyInverseDpiScalingTransform();

  WindowBoundsWinUWP bounds = GetBounds(current_display_info_, true);

  binding_handler_delegate_->OnWindowSizeChanged(
      static_cast<size_t>(bounds.width), static_cast<size_t>(bounds.height));
}

void FlutterWindowWinUWP::OnPointerPressed(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::PointerEventArgs const& args) {
  double x = GetPosX(args);
  double y = GetPosY(args);

  binding_handler_delegate_->OnPointerDown(
      x, y, FlutterPointerMouseButtons::kFlutterPointerButtonMousePrimary);
}

void FlutterWindowWinUWP::OnPointerReleased(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::PointerEventArgs const& args) {
  double x = GetPosX(args);
  double y = GetPosY(args);

  binding_handler_delegate_->OnPointerUp(
      x, y, FlutterPointerMouseButtons::kFlutterPointerButtonMousePrimary);
}

void FlutterWindowWinUWP::OnPointerMoved(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::PointerEventArgs const& args) {
  double x = GetPosX(args);
  double y = GetPosY(args);

  binding_handler_delegate_->OnPointerMove(x, y);
}

void FlutterWindowWinUWP::OnPointerWheelChanged(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::PointerEventArgs const& args) {
  double x = GetPosX(args);
  double y = GetPosY(args);
  int delta = args.CurrentPoint().Properties().MouseWheelDelta();
  binding_handler_delegate_->OnScroll(x, y, 0, -delta, 1);
}

double FlutterWindowWinUWP::GetPosX(
    winrt::Windows::UI::Core::PointerEventArgs const& args) {
  const double inverse_dpi_scale = GetDpiScale();

  return (args.CurrentPoint().Position().X - xbox_overscan_x_offset_) *
         inverse_dpi_scale;
}

double FlutterWindowWinUWP::GetPosY(
    winrt::Windows::UI::Core::PointerEventArgs const& args) {
  const double inverse_dpi_scale = GetDpiScale();
  return static_cast<double>(
      (args.CurrentPoint().Position().Y - xbox_overscan_y_offset_) *
      inverse_dpi_scale);
}

void FlutterWindowWinUWP::OnBoundsChanged(
    winrt::Windows::UI::ViewManagement::ApplicationView const& app_view,
    winrt::Windows::Foundation::IInspectable const&) {
#ifndef USECOREWINDOW
  if (binding_handler_delegate_) {
    auto bounds = GetBounds(current_display_info_, true);

    binding_handler_delegate_->OnWindowSizeChanged(
        static_cast<size_t>(bounds.width), static_cast<size_t>(bounds.height));
    render_target_.Size({bounds.width, bounds.height});
  }
#endif
}

void FlutterWindowWinUWP::OnKeyUp(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::KeyEventArgs const& args) {
  // TODO(clarkezone) complete keyboard handling including
  // system key (back), unicode handling, shortcut support,
  // handling defered delivery, remove the need for action value.
  // https://github.com/flutter/flutter/issues/70202
  auto status = args.KeyStatus();
  unsigned int scancode = status.ScanCode;
  int key = static_cast<int>(args.VirtualKey());
  char32_t chararacter = static_cast<char32_t>(key | 32);
  int action = 0x0101;
  binding_handler_delegate_->OnKey(key, scancode, action, chararacter, false);
}

void FlutterWindowWinUWP::OnKeyDown(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::KeyEventArgs const& args) {
  // TODO(clarkezone) complete keyboard handling including
  // system key (back), unicode handling, shortcut support
  // handling defered delivery, remove the need for action value.
  // https://github.com/flutter/flutter/issues/70202
  auto status = args.KeyStatus();
  unsigned int scancode = status.ScanCode;
  int key = static_cast<int>(args.VirtualKey());
  char32_t chararacter = static_cast<char32_t>(key | 32);
  int action = 0x0100;
  binding_handler_delegate_->OnKey(key, scancode, action, chararacter, false);
}

void FlutterWindowWinUWP::OnCharacterReceived(
    winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Core::CharacterReceivedEventArgs const& args) {
  auto key = args.KeyCode();
  wchar_t keycode = static_cast<wchar_t>(key);
  if (keycode >= u' ') {
    std::u16string text({keycode});
    binding_handler_delegate_->OnText(text);
  }
}

void FlutterWindowWinUWP::OnGamePadLeftStickMoved(double x, double y) {
  float new_x =
      cursor_visual_.Offset().x + (kCursorScale * static_cast<float>(x));

  float new_y =
      cursor_visual_.Offset().y + (kCursorScale * -static_cast<float>(y));

  WindowBoundsWinUWP logical_bounds = GetBounds(current_display_info_, false);

  if (new_x > 0 && new_y > 0 && new_x < logical_bounds.width &&
      new_y < logical_bounds.height) {
    cursor_visual_.Offset({new_x, new_y, 0});
    if (!running_on_xbox_) {
      const double inverse_dpi_scale = GetDpiScale();
      binding_handler_delegate_->OnPointerMove(
          cursor_visual_.Offset().x * inverse_dpi_scale,
          cursor_visual_.Offset().y * inverse_dpi_scale);
    } else {
      binding_handler_delegate_->OnPointerMove(cursor_visual_.Offset().x,
                                               cursor_visual_.Offset().y);
    }
  }
}

void FlutterWindowWinUWP::OnGamePadRightStickMoved(double x, double y) {
  if (!running_on_xbox_) {
    const double inverse_dpi_scale = GetDpiScale();

    binding_handler_delegate_->OnScroll(
        cursor_visual_.Offset().x * inverse_dpi_scale,
        cursor_visual_.Offset().y * inverse_dpi_scale,
        x * kControllerScrollMultiplier, y * kControllerScrollMultiplier, 1);
  } else {
    binding_handler_delegate_->OnScroll(
        cursor_visual_.Offset().x, cursor_visual_.Offset().y,
        x * kControllerScrollMultiplier, y * kControllerScrollMultiplier, 1);
  }
}

void FlutterWindowWinUWP::OnGamePadButtonPressed(
    winrt::Windows::Gaming::Input::GamepadButtons buttons) {
  if ((buttons & winrt::Windows::Gaming::Input::GamepadButtons::A) ==
      winrt::Windows::Gaming::Input::GamepadButtons::A) {
    if (!running_on_xbox_) {
      const double inverse_dpi_scale = GetDpiScale();
      binding_handler_delegate_->OnPointerDown(
          cursor_visual_.Offset().x * inverse_dpi_scale,
          cursor_visual_.Offset().y * inverse_dpi_scale,
          FlutterPointerMouseButtons::kFlutterPointerButtonMousePrimary);
    } else {
      binding_handler_delegate_->OnPointerDown(
          cursor_visual_.Offset().x, cursor_visual_.Offset().y,
          FlutterPointerMouseButtons::kFlutterPointerButtonMousePrimary);
    }
  }
}

void FlutterWindowWinUWP::OnGamePadButtonReleased(
    winrt::Windows::Gaming::Input::GamepadButtons buttons) {
  if ((buttons & winrt::Windows::Gaming::Input::GamepadButtons::A) ==
      winrt::Windows::Gaming::Input::GamepadButtons::A) {
    if (!running_on_xbox_) {
      const double inverse_dpi_scale = GetDpiScale();

      binding_handler_delegate_->OnPointerUp(
          cursor_visual_.Offset().x * inverse_dpi_scale,
          cursor_visual_.Offset().y * inverse_dpi_scale,
          FlutterPointerMouseButtons::kFlutterPointerButtonMousePrimary);
    } else {
      binding_handler_delegate_->OnPointerUp(
          cursor_visual_.Offset().x, cursor_visual_.Offset().y,
          FlutterPointerMouseButtons::kFlutterPointerButtonMousePrimary);
    }
  }
}

void FlutterWindowWinUWP::OnGamePadControllersChanged() {
  // TODO lock here
  if (game_pad_->HasController()) {
    if (!game_controller_thread_running_) {
      if (cursor_visual_ != nullptr) {
        visual_tree_root_.Children().InsertAtTop(cursor_visual_);
      }
      StartGamepadCursorThread();
      game_controller_thread_running_ = true;
    } else {
      if (cursor_visual_ != nullptr) {
        visual_tree_root_.Children().Remove(cursor_visual_);
      }
      game_controller_thread_running_ = false;
      // TODO stop game thread
    }
  }
}

winrt::Windows::UI::Composition::Visual
FlutterWindowWinUWP::CreateCursorVisual() {
  auto container = compositor_.CreateContainerVisual();
  container.Offset(
      {window_.Bounds().Width / 2, window_.Bounds().Height / 2, 1.0});

  // size of the simulated mouse cursor
  const float size = 30;
  auto cursor_visual = compositor_.CreateShapeVisual();
  cursor_visual.Size({size, size});

  // compensate for overscan in cursor visual
  cursor_visual.Offset({xbox_overscan_x_offset_, xbox_overscan_y_offset_, 1.0});

  winrt::Windows::UI::Composition::CompositionEllipseGeometry circle =
      compositor_.CreateEllipseGeometry();
  circle.Radius({size / 2, size / 2});

  auto circleshape = compositor_.CreateSpriteShape(circle);
  circleshape.FillBrush(
      compositor_.CreateColorBrush(winrt::Windows::UI::Colors::Black()));
  circleshape.Offset({size / 2, size / 2});

  cursor_visual.Shapes().Append(circleshape);

  winrt::Windows::UI::Composition::Visual visual =
      cursor_visual.as<winrt::Windows::UI::Composition::Visual>();

  visual.CompositeMode(winrt::Windows::UI::Composition::
                           CompositionCompositeMode::DestinationInvert);

  visual.AnchorPoint({0.5, 0.5});
  container.Children().InsertAtTop(visual);

  return container;
}

}  // namespace flutter
