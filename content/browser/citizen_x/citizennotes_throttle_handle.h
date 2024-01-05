// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_THROTTLE_HANDLE_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_THROTTLE_HANDLE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace content {

// A simple class that holds the callback that unthrottles either a SW or a
// navigation. It is refcounted and runs `throttle_callback` when destroyed.
class CitizenNotesThrottleHandle : public base::RefCounted<CitizenNotesThrottleHandle> {
 public:
  explicit CitizenNotesThrottleHandle(base::OnceCallback<void()> throttle_callback);

  CitizenNotesThrottleHandle(const CitizenNotesThrottleHandle&) = delete;
  CitizenNotesThrottleHandle& operator=(const CitizenNotesThrottleHandle&) = delete;

 private:
  friend class base::RefCounted<CitizenNotesThrottleHandle>;

  ~CitizenNotesThrottleHandle();

  base::OnceCallback<void()> throttle_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_THROTTLE_HANDLE_H_
