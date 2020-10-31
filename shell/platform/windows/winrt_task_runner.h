// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_UWP_TASK_RUNNER_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_UWP_TASK_RUNNER_H_

#include <windows.h>

#include <winrt/Windows.UI.Core.h>

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

// A custom task runner that integrates with user32 GetMessage semantics so that
// host app can own its own message loop and flutter still gets to process
// tasks on a timely basis.
class WinRTTaskRunner {
 public:
  using TaskExpiredCallback = std::function<void(const FlutterTask*)>;
  WinRTTaskRunner(DWORD main_thread_id,
                  const TaskExpiredCallback& on_task_expired);

  ~WinRTTaskRunner();

  // Returns if the current thread is the thread used by the win32 event loop.
  bool RunsTasksOnCurrentThread() const;

  // Post a Flutter engine tasks to the event loop for delayed execution.
  void PostTask(FlutterTask flutter_task, uint64_t flutter_target_time_nanos);

 private:
  using TaskTimePoint = std::chrono::steady_clock::time_point;
  struct Task {
    uint64_t order;
    TaskTimePoint fire_time;
    FlutterTask task;

    struct Comparer {
      bool operator()(const Task& a, const Task& b) {
        if (a.fire_time == b.fire_time) {
          return a.order > b.order;
        }
        return a.fire_time > b.fire_time;
      }
    };
  };
  DWORD main_thread_id_;
  TaskExpiredCallback on_task_expired_;

  WinRTTaskRunner(const WinRTTaskRunner&) = delete;

  WinRTTaskRunner& operator=(const WinRTTaskRunner&) = delete;

  winrt::Windows::UI::Core::CoreDispatcher dispatcher_{nullptr};

  static TaskTimePoint TimePointFromFlutterTime(
      uint64_t flutter_target_time_nanos);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_UWP_TASK_RUNNER_H_
