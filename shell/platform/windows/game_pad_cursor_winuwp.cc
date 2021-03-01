// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/game_pad_cursor_winuwp.h"
#include "game_pad_cursor_winuwp.h"

namespace flutter {

GamepadCursorWinUWP::GamepadCursorWinUWP(
    WindowBindingHandlerDelegate* view,
    DisplayHelperWinUWP* displayhelper,
    winrt::Windows::UI::Core::CoreWindow const& window,
    winrt::Windows::UI::Composition::Compositor const& compositor,
    winrt::Windows::UI::Composition::VisualCollection const& rootcollection) {
  window_ = window;
  root_collection_ = rootcollection;
  binding_handler_delegate_ = view;
  display_helper_ = displayhelper;
  compositor_ = compositor;
  cursor_visual_ = CreateCursorVisual();
  ConfigureGamepad();
}

void GamepadCursorWinUWP::StartGamepadTimer() {
  winrt::Windows::UI::Core::CoreDispatcher dispatcher = window_.Dispatcher();

  dispatcher.RunAsync(
      winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
      [this, dispatcher]() {
        auto queue =
            winrt::Windows::System::DispatcherQueue::GetForCurrentThread();

        if (cursor_move_timer_ == nullptr) {
          cursor_move_timer_ = queue.CreateTimer();
          cursor_move_timer_.Interval(
              std::chrono::milliseconds(kNormalPollingFrequency));
          cursor_move_timer_.Tick(
              [=](winrt::Windows::System::DispatcherQueueTimer timer,
                  winrt::Windows::Foundation::IInspectable const&) {
                game_pad_->Process();
              });
          cursor_move_timer_.Start();
        }
      });
}

void GamepadCursorWinUWP::StopGamepadTimer() {
  winrt::Windows::UI::Core::CoreDispatcher dispatcher = window_.Dispatcher();

  dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                      [this, dispatcher]() {
                        if (cursor_move_timer_ != nullptr) {
                          cursor_move_timer_.Stop();
                          cursor_move_timer_ = nullptr;
                        }
                      });
}

void GamepadCursorWinUWP::ConfigureGamepad() {
  GamepadDualAxisCallback leftStick = [=](double x, double y) {
    OnGamepadLeftStickMoved(x, y);
  };

  GamepadDualAxisCallback rightStick = [=](double x, double y) {
    OnGamepadRightStickMoved(x, y);
  };

  GamepadButtonCallback pressedcallback =
      [=](winrt::Windows::Gaming::Input::GamepadButtons buttons) {
        OnGamepadButtonPressed(buttons);
      };

  GamepadButtonCallback releasedcallback =
      [=](winrt::Windows::Gaming::Input::GamepadButtons buttons) {
        OnGamepadButtonReleased(buttons);
      };
  GamepadAddedRemovedCallback changed = [=]() {
    OnGamepadControllersChanged();
  };

  game_pad_ = std::make_unique<GamepadWinUWP>(leftStick, rightStick, nullptr,
                                              nullptr, pressedcallback,
                                              releasedcallback, changed);
  game_pad_->Initialize();

  SetCursorTimeout();
}

void GamepadCursorWinUWP::SetCursorTimeout() {
  if (cursor_visual_ == nullptr) {
    return;
  }

  auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();

  // lazily create timer.
  if (emulated_cursor_hide_timer_ == nullptr) {
    emulated_cursor_hide_timer_ = queue.CreateTimer();
    emulated_cursor_hide_timer_.Interval(std::chrono::seconds(kInactivePeriod));
    emulated_cursor_hide_timer_.Tick(
        [=](winrt::Windows::System::DispatcherQueueTimer timer,
            winrt::Windows::Foundation::IInspectable const&) {
          // Timer fires when user has been inactive.  At this point stop the
          // timer.  It will be restarted next time controller interaction
          // happens triggering SetCursorTimeout
          timer.Stop();

          // If the timer fires, the user hasn't move the cursor for more than
          // the interval hence hide the cursor
          cursor_visual_.IsVisible(false);

          // Reduce mouse move poling frequency while user not interacting with
          // controller
          SetMouseMovePollingFrequency(kReducedPollingFrequency);
        });
  }

  // Ensure the cursor is visible and restart the timer as the user is
  // interacting.
  cursor_visual_.IsVisible(true);
  emulated_cursor_hide_timer_.Start();

  SetMouseMovePollingFrequency(kNormalPollingFrequency);
}

void GamepadCursorWinUWP::SetMouseMovePollingFrequency(int ms) {
  if (cursor_move_timer_ != nullptr) {
    cursor_move_timer_.Interval(std::chrono::milliseconds(ms));
  }
}

void GamepadCursorWinUWP::OnGamepadLeftStickMoved(double x, double y) {
  SetCursorTimeout();

  float new_x =
      cursor_visual_.Offset().x + (kCursorScale * static_cast<float>(x));

  float new_y =
      cursor_visual_.Offset().y + (kCursorScale * -static_cast<float>(y));

  WindowBoundsWinUWP logical_bounds = display_helper_->GetBounds(false);

  if (new_x > 0 && new_y > 0 && new_x < logical_bounds.width &&
      new_y < logical_bounds.height) {
    cursor_visual_.Offset({new_x, new_y, 0});
    if (!display_helper_->IsRunningOnXbox()) {
      const double inverse_dpi_scale = display_helper_->GetDpiScale();
      binding_handler_delegate_->OnPointerMove(
          cursor_visual_.Offset().x * inverse_dpi_scale,
          cursor_visual_.Offset().y * inverse_dpi_scale);
    } else {
      binding_handler_delegate_->OnPointerMove(cursor_visual_.Offset().x,
                                               cursor_visual_.Offset().y);
    }
  }
}

void GamepadCursorWinUWP::OnGamepadRightStickMoved(double x, double y) {
  if (!display_helper_->IsRunningOnXbox()) {
    const double inverse_dpi_scale = display_helper_->GetDpiScale();

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

void GamepadCursorWinUWP::OnGamepadButtonPressed(
    winrt::Windows::Gaming::Input::GamepadButtons buttons) {
  if ((buttons & winrt::Windows::Gaming::Input::GamepadButtons::A) ==
      winrt::Windows::Gaming::Input::GamepadButtons::A) {
    if (!display_helper_->IsRunningOnXbox()) {
      const double inverse_dpi_scale = display_helper_->GetDpiScale();
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

void GamepadCursorWinUWP::OnGamepadButtonReleased(
    winrt::Windows::Gaming::Input::GamepadButtons buttons) {
  if ((buttons & winrt::Windows::Gaming::Input::GamepadButtons::A) ==
      winrt::Windows::Gaming::Input::GamepadButtons::A) {
    if (!display_helper_->IsRunningOnXbox()) {
      const double inverse_dpi_scale = display_helper_->GetDpiScale();

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

void GamepadCursorWinUWP::OnGamepadControllersChanged() {
  if (game_pad_->HasController()) {
    if (!cursor_move_timer_) {
      if (cursor_visual_ != nullptr) {
        root_collection_.InsertAtTop(cursor_visual_);
      }
      StartGamepadTimer();
    } else {
      if (cursor_visual_ != nullptr) {
        root_collection_.Remove(cursor_visual_);
      }
      StopGamepadTimer();
    }
  }
}

winrt::Windows::UI::Composition::Visual
GamepadCursorWinUWP::CreateCursorVisual() {
  auto container = compositor_.CreateContainerVisual();
  container.Offset(
      {window_.Bounds().Width / 2, window_.Bounds().Height / 2, 1.0});

  // size of the simulated mouse cursor
  const float size = 20;
  const float container_size = size + 10;
  auto cursor_visual = compositor_.CreateShapeVisual();
  cursor_visual.Size({container_size, container_size});

  // compensate for overscan in cursor visual
  cursor_visual.Offset({display_helper_->GetXboxOverscanXOffset(),
                        display_helper_->GetXboxOverscanYOffset(), 1.0});

  winrt::Windows::UI::Composition::CompositionEllipseGeometry circle =
      compositor_.CreateEllipseGeometry();
  circle.Radius({size / 2, size / 2});

  auto circleshape = compositor_.CreateSpriteShape(circle);
  circleshape.FillBrush(
      compositor_.CreateColorBrush(winrt::Windows::UI::Colors::Black()));
  circleshape.StrokeBrush(
      compositor_.CreateColorBrush(winrt::Windows::UI::Colors::White()));
  circleshape.StrokeThickness(5.0);
  circleshape.Offset({container_size / 2, container_size / 2});

  cursor_visual.Shapes().Append(circleshape);

  winrt::Windows::UI::Composition::Visual visual =
      cursor_visual.as<winrt::Windows::UI::Composition::Visual>();

  visual.AnchorPoint({0.5, 0.5});
  container.Children().InsertAtTop(visual);

  return container;
}

}  // namespace flutter
