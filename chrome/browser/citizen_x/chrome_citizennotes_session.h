// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_CHROME_CITIZENNOTES_SESSION_H_
#define CHROME_BROWSER_CITIZENNOTES_CHROME_CITIZENNOTES_SESSION_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/devtools/protocol/storage_handler.h"
#include "content/public/browser/citizennotes_manager_delegate.h"

namespace content {
class CitizenNotesAgentHostClientChannel;
}  // namespace content

class CNAutofillHandler;
class CNEmulationHandler;
class CNBrowserHandler;
class CNCastHandler;
class CNPageHandler;
class CNSecurityHandler;
class CNStorageHandler;
class CNSystemInfoHandler;
class CNTargetHandler;
class CNWindowManagerHandler;

class ChromeCitizenNotesSession : public protocol::FrontendChannel {
 public:
  explicit ChromeCitizenNotesSession(
      content::CitizenNotesAgentHostClientChannel* channel);

  ChromeCitizenNotesSession(const ChromeCitizenNotesSession&) = delete;
  ChromeCitizenNotesSession& operator=(const ChromeCitizenNotesSession&) = delete;

  ~ChromeCitizenNotesSession() override;

  void HandleCommand(
      base::span<const uint8_t> message,
      content::CitizenNotesManagerDelegate::NotHandledCallback callback);

  CNTargetHandler* target_handler() { return target_handler_.get(); }

 private:
  // protocol::FrontendChannel:
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FlushProtocolNotifications() override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  base::flat_map<int, content::CitizenNotesManagerDelegate::NotHandledCallback>
      pending_commands_;

  protocol::UberDispatcher dispatcher_;
  std::unique_ptr<CNAutofillHandler> autofill_handler_;
  std::unique_ptr<CNBrowserHandler> browser_handler_;
  std::unique_ptr<CNCastHandler> cast_handler_;
  std::unique_ptr<CNEmulationHandler> emulation_handler_;
  std::unique_ptr<CNPageHandler> page_handler_;
  std::unique_ptr<CNSecurityHandler> security_handler_;
  std::unique_ptr<CNStorageHandler> storage_handler_;
  std::unique_ptr<CNSystemInfoHandler> system_info_handler_;
  std::unique_ptr<CNTargetHandler> target_handler_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<CNWindowManagerHandler> window_manager_handler_;
#endif
  RAW_PTR_EXCLUSION content::CitizenNotesAgentHostClientChannel* client_channel_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_CHROME_CITIZENNOTES_SESSION_H_
