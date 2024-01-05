// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/mojom_citizennotes_agent_host.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/public/browser/mojom_citizennotes_agent_host_delegate.h"

namespace content {

// static
void MojomCitizenNotesAgentHost::GetAll(CitizenNotesAgentHost::List* out_list) {
  for (auto& id : host_ids()) {
    auto host = GetForId(id);
    if (host) {
      out_list->emplace_back(host);
    }
  }
}

MojomCitizenNotesAgentHost::MojomCitizenNotesAgentHost(
    const std::string& id,
    std::unique_ptr<MojomCitizenNotesAgentHostDelegate> delegate)
    : CitizenNotesAgentHostImpl(id), delegate_(std::move(delegate)) {
  mojo::PendingAssociatedRemote<blink::mojom::CitizenNotesAgent> agent;
  delegate_->ConnectCitizenNotesAgent(agent.InitWithNewEndpointAndPassReceiver());
  associated_agent_remote_.Bind(std::move(agent));
  NotifyCreated();
  host_ids().emplace_back(GetId());
}

MojomCitizenNotesAgentHost::~MojomCitizenNotesAgentHost() {
  associated_agent_remote_.reset();
  delegate_.reset();
  std::erase(host_ids(), GetId());
}

// Devtools Agent host overrides:

std::string MojomCitizenNotesAgentHost::GetType() {
  return delegate_->GetType();
}

std::string MojomCitizenNotesAgentHost::GetTitle() {
  return delegate_->GetTitle();
}

GURL MojomCitizenNotesAgentHost::GetURL() {
  return delegate_->GetURL();
}

bool MojomCitizenNotesAgentHost::Activate() {
  return delegate_->Activate();
}

bool MojomCitizenNotesAgentHost::Close() {
  return delegate_->Close();
}

void MojomCitizenNotesAgentHost::Reload() {
  delegate_->Reload();
}

// Devtools agent host impl overrides:

bool MojomCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                           bool aquire_wake_lock) {
  session->AttachToAgent(associated_agent_remote_.get(),
                         delegate_->ForceIOSession());
  return true;
}

// static
std::vector<std::string>& MojomCitizenNotesAgentHost::host_ids() {
  static base::NoDestructor<std::vector<std::string>> host_ids_{};
  return *host_ids_;
}

}  // namespace content
