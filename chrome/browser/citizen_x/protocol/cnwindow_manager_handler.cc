// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/protocol/cnwindow_manager_handler.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "content/public/browser/browser_thread.h"

CNWindowManagerHandler::CNWindowManagerHandler(
    protocol::UberDispatcher* dispatcher) {
  protocol::WindowManager::Dispatcher::wire(dispatcher, this);
}

CNWindowManagerHandler::~CNWindowManagerHandler() = default;

protocol::Response CNWindowManagerHandler::EnterOverviewMode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool success = ash::Shell::Get()->overview_controller()->StartOverview(
      ash::OverviewStartAction::kDevTools);
  return success ? protocol::Response::Success()
                 : protocol::Response::ServerError("Overview failed");
}

protocol::Response CNWindowManagerHandler::ExitOverviewMode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool success = ash::Shell::Get()->overview_controller()->EndOverview(
      ash::OverviewEndAction::kDevTools);
  return success ? protocol::Response::Success()
                 : protocol::Response::ServerError("Overview failed");
}
