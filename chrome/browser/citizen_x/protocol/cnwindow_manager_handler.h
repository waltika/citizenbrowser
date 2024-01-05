// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_PROTOCOL_WINDOW_MANAGER_HANDLER_H_
#define CHROME_BROWSER_CITIZENNOTES_PROTOCOL_WINDOW_MANAGER_HANDLER_H_

#include "chrome/browser/devtools/protocol/window_manager.h"

class CNWindowManagerHandler : public protocol::WindowManager::Backend {
 public:
  explicit CNWindowManagerHandler(protocol::UberDispatcher* dispatcher);

  CNWindowManagerHandler(const CNWindowManagerHandler&) = delete;
  CNWindowManagerHandler& operator=(const CNWindowManagerHandler&) = delete;

  ~CNWindowManagerHandler() override;

  // WindowManager::Backend:
  protocol::Response EnterOverviewMode() override;
  protocol::Response ExitOverviewMode() override;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_PROTOCOL_WINDOW_MANAGER_HANDLER_H_
