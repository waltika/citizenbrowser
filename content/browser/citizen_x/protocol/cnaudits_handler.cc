// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnaudits_handler.h"

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/citizennotes_issue_storage.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace protocol {

CNAuditsHandler::CNAuditsHandler()
    : CitizenNotesDomainHandler(Audits::Metainfo::domainName) {}
CNAuditsHandler::~CNAuditsHandler() = default;

// static
std::vector<CNAuditsHandler*> CNAuditsHandler::ForAgentHost(
    CitizenNotesAgentHostImpl* host) {
  return host->HandlersByName<CNAuditsHandler>(Audits::Metainfo::domainName);
}

void CNAuditsHandler::SetRenderer(int process_host_id,
                                RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
}

void CNAuditsHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Audits::Frontend>(dispatcher->channel());
  Audits::Dispatcher::wire(dispatcher, this);
}

DispatchResponse CNAuditsHandler::Disable() {
  enabled_ = false;
  return Response::FallThrough();
}

namespace {

void SendStoredIssuesForFrameToAgent(RenderFrameHostImpl* rfh,
                                     protocol::CNAuditsHandler* handler) {
  // Check the storage first. No need to do any work in case its empty.
  CitizenNotesIssueStorage* issue_storage =
      CitizenNotesIssueStorage::GetForPage(rfh->GetOutermostMainFrame()->GetPage());
  if (!issue_storage)
    return;
  auto issues = issue_storage->FindIssuesForAgentOf(rfh);
  for (auto* const issue : issues) {
    handler->OnIssueAdded(issue);
  }
}

}  // namespace

DispatchResponse CNAuditsHandler::Enable() {
  if (enabled_) {
    return Response::FallThrough();
  }

  enabled_ = true;
  if (host_) {
    SendStoredIssuesForFrameToAgent(host_, this);
  }

  return Response::FallThrough();
}

void CNAuditsHandler::OnIssueAdded(
    const protocol::Audits::InspectorIssue* issue) {
  if (enabled_) {
    frontend_->IssueAdded(issue->Clone());
  }
}

}  // namespace protocol
}  // namespace content
