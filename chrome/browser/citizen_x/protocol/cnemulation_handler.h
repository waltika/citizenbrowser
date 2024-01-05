// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_PROTOCOL_EMULATION_HANDLER_H_
#define CHROME_BROWSER_CITIZENNOTES_PROTOCOL_EMULATION_HANDLER_H_

#include "chrome/browser/devtools/protocol/emulation.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/citizennotes_agent_host.h"

class CNEmulationHandler : public protocol::Emulation::Backend,
                         public infobars::InfoBarManager::Observer {
 public:
  CNEmulationHandler(content::CitizenNotesAgentHost* agent_host,
                   protocol::UberDispatcher* dispatcher);
  ~CNEmulationHandler() override;

  CNEmulationHandler(const CNEmulationHandler&) = delete;
  CNEmulationHandler& operator=(const CNEmulationHandler&) = delete;

  // Emulation::Backend:
  protocol::Response Disable() override;
  protocol::Response SetAutomationOverride(bool enabled) override;

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

 private:
  infobars::ContentInfoBarManager* GetContentInfoBarManager();

  RAW_PTR_EXCLUSION content::CitizenNotesAgentHost* agent_host_;
  RAW_PTR_EXCLUSION infobars::InfoBar* automation_info_bar_ = nullptr;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_PROTOCOL_EMULATION_HANDLER_H_
