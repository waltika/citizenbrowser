// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/chrome_citizennotes_session.h"

#include <memory>
#include <type_traits>
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/citizen_x/protocol/cnautofill_handler.h"
#include "chrome/browser/citizen_x/protocol/cnbrowser_handler.h"
#include "chrome/browser/citizen_x/protocol/cncast_handler.h"
#include "chrome/browser/citizen_x/protocol/cnemulation_handler.h"
#include "chrome/browser/citizen_x/protocol/cnpage_handler.h"
#include "chrome/browser/citizen_x/protocol/cnsecurity_handler.h"
#include "chrome/browser/citizen_x/protocol/cnstorage_handler.h"
#include "chrome/browser/citizen_x/protocol/cnsystem_info_handler.h"
#include "chrome/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/citizennotes_agent_host_client.h"
#include "content/public/browser/citizennotes_agent_host_client_channel.h"
#include "content/public/browser/citizennotes_manager_delegate.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/citizen_x/protocol/cnwindow_manager_handler.h"
#endif

namespace {

template <typename Handler>
bool IsDomainAvailableToUntrustedClient() {
  return std::disjunction_v<std::is_same<Handler, CNPageHandler>,
                            std::is_same<Handler, CNEmulationHandler>,
                            std::is_same<Handler, CNTargetHandler>>;
}

}  // namespace

ChromeCitizenNotesSession::ChromeCitizenNotesSession(
    content::CitizenNotesAgentHostClientChannel* channel)
    : dispatcher_(this), client_channel_(channel) {
  content::CitizenNotesAgentHost* agent_host = channel->GetAgentHost();
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::CitizenNotesAgentHost::kTypePage) {
    if (IsDomainAvailableToUntrustedClient<CNPageHandler>() ||
        channel->GetClient()->IsTrusted()) {
      page_handler_ = std::make_unique<CNPageHandler>(
          agent_host, agent_host->GetWebContents(), &dispatcher_);
    }
    if (IsDomainAvailableToUntrustedClient<CNSecurityHandler>() ||
        channel->GetClient()->IsTrusted()) {
      security_handler_ = std::make_unique<CNSecurityHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
    if (IsDomainAvailableToUntrustedClient<CNCastHandler>() ||
        channel->GetClient()->IsTrusted()) {
      cast_handler_ = std::make_unique<CNCastHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
    if (IsDomainAvailableToUntrustedClient<CNStorageHandler>() ||
        channel->GetClient()->IsTrusted()) {
      storage_handler_ = std::make_unique<CNStorageHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
  }
  if (agent_host->GetWebContents() &&
      (agent_host->GetType() == content::CitizenNotesAgentHost::kTypePage ||
       agent_host->GetType() == content::CitizenNotesAgentHost::kTypeFrame)) {
    if (IsDomainAvailableToUntrustedClient<CNAutofillHandler>() ||
        channel->GetClient()->IsTrusted()) {
      autofill_handler_ =
          std::make_unique<CNAutofillHandler>(&dispatcher_, agent_host->GetId());
    }
  }
  if (IsDomainAvailableToUntrustedClient<CNEmulationHandler>() ||
      channel->GetClient()->IsTrusted()) {
    emulation_handler_ =
        std::make_unique<CNEmulationHandler>(agent_host, &dispatcher_);
  }
  if (IsDomainAvailableToUntrustedClient<CNTargetHandler>() ||
      channel->GetClient()->IsTrusted()) {
    target_handler_ = std::make_unique<CNTargetHandler>(
        &dispatcher_, channel->GetClient()->IsTrusted());
  }
  if (IsDomainAvailableToUntrustedClient<CNBrowserHandler>() ||
      channel->GetClient()->IsTrusted()) {
    browser_handler_ =
        std::make_unique<CNBrowserHandler>(&dispatcher_, agent_host->GetId());
  }
  if (IsDomainAvailableToUntrustedClient<CNSystemInfoHandler>() ||
      channel->GetClient()->IsTrusted()) {
    system_info_handler_ = std::make_unique<CNSystemInfoHandler>(&dispatcher_);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  window_manager_handler_ =
      std::make_unique<CNWindowManagerHandler>(&dispatcher_);
#endif
}

ChromeCitizenNotesSession::~ChromeCitizenNotesSession() = default;

base::HistogramBase::Sample CNGetCommandUmaId(
    const base::StringPiece command_name) {
  return static_cast<base::HistogramBase::Sample>(
      base::HashMetricName(command_name));
}

void ChromeCitizenNotesSession::HandleCommand(
    base::span<const uint8_t> message,
    content::CitizenNotesManagerDelegate::NotHandledCallback callback) {
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  DCHECK(dispatchable.ok());  // Checked by content::CitizenNotesSession.
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_.Dispatch(dispatchable);

  auto command_uma_id = CNGetCommandUmaId(base::StringPiece(
      reinterpret_cast<const char*>(dispatchable.Method().begin()),
      dispatchable.Method().size()));
  std::string client_type = client_channel_->GetClient()->GetTypeForMetrics();
  DCHECK(client_type == "CitizenNotes" || client_type == "Extension" ||
         client_type == "RemoteDebugger" || client_type == "Other");
  base::UmaHistogramSparse("CitizenNotes.CDPCommandFrom" + client_type,
                           command_uma_id);

  if (!dispatched.MethodFound()) {
    std::move(callback).Run(message);
    return;
  }
  pending_commands_[dispatchable.CallId()] = std::move(callback);
  dispatched.Run();
}

// The following methods handle responses or notifications coming from
// the browser to the client.
void ChromeCitizenNotesSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  pending_commands_.erase(call_id);
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeCitizenNotesSession::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeCitizenNotesSession::FlushProtocolNotifications() {}

void ChromeCitizenNotesSession::FallThrough(int call_id,
                                        crdtp::span<uint8_t> method,
                                        crdtp::span<uint8_t> message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
