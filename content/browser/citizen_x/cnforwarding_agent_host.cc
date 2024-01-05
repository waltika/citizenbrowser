// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/cnforwarding_agent_host.h"

#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/public/browser/citizennotes_external_agent_proxy_delegate.h"

namespace content {

CNForwardingAgentHost::CNForwardingAgentHost(
    const std::string& id,
    std::unique_ptr<CitizenNotesExternalAgentProxyDelegate> delegate)
      : CitizenNotesAgentHostImpl(id),
        delegate_(std::move(delegate)) {
  NotifyCreated();
}

CNForwardingAgentHost::~CNForwardingAgentHost() = default;

bool CNForwardingAgentHost::AttachSession(CitizenNotesSession* session,
                                        bool acquire_wake_lock) {
  session->TurnIntoExternalProxy(delegate_.get());
  return true;
}

void CNForwardingAgentHost::DetachSession(CitizenNotesSession* session) {}

std::string CNForwardingAgentHost::GetType() {
  return delegate_->GetType();
}

std::string CNForwardingAgentHost::GetTitle() {
  return delegate_->GetTitle();
}

GURL CNForwardingAgentHost::GetURL() {
  return delegate_->GetURL();
}

GURL CNForwardingAgentHost::GetFaviconURL() {
  return delegate_->GetFaviconURL();
}

std::string CNForwardingAgentHost::GetFrontendURL() {
  return delegate_->GetFrontendURL();
}

bool CNForwardingAgentHost::Activate() {
  return delegate_->Activate();
}

void CNForwardingAgentHost::Reload() {
  delegate_->Reload();
}

bool CNForwardingAgentHost::Close() {
  return delegate_->Close();
}

base::TimeTicks CNForwardingAgentHost::GetLastActivityTime() {
  return delegate_->GetLastActivityTime();
}

std::string CNForwardingAgentHost::GetDescription() {
  return delegate_->GetDescription();
}

}  // content
