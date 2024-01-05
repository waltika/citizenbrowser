// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/citizennotes_throttle_handle.h"

namespace content {

CitizenNotesThrottleHandle::CitizenNotesThrottleHandle(
    base::OnceCallback<void()> throttle_callback)
    : throttle_callback_(std::move(throttle_callback)) {}

CitizenNotesThrottleHandle::~CitizenNotesThrottleHandle() {
  std::move(throttle_callback_).Run();
}

}  // namespace content
