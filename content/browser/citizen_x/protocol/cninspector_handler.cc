// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cninspector_handler.h"

#include <memory>

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {
namespace protocol {

CNInspectorHandler::CNInspectorHandler()
    : CitizenNotesDomainHandler(Inspector::Metainfo::domainName) {}

CNInspectorHandler::~CNInspectorHandler() = default;

// static
std::vector<CNInspectorHandler*> CNInspectorHandler::ForAgentHost(
    CitizenNotesAgentHostImpl* host) {
  return host->HandlersByName<CNInspectorHandler>(
      Inspector::Metainfo::domainName);
}

void CNInspectorHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Inspector::Frontend>(dispatcher->channel());
  Inspector::Dispatcher::wire(dispatcher, this);
}

void CNInspectorHandler::SetRenderer(int process_host_id,
                                   RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
}

void CNInspectorHandler::TargetCrashed() {
  target_crashed_ = true;
  frontend_->TargetCrashed();
}

void CNInspectorHandler::TargetReloadedAfterCrash() {
  // Only send the event if targetCrashed was previously sent in this session.
  if (!target_crashed_)
    return;
  frontend_->TargetReloadedAfterCrash();
}

void CNInspectorHandler::TargetDetached(const std::string& reason) {
  frontend_->Detached(reason);
}

Response CNInspectorHandler::Enable() {
  if (host_ && !host_->IsRenderFrameLive())
    TargetCrashed();
  return Response::Success();
}

Response CNInspectorHandler::Disable() {
  return Response::Success();
}

}  // namespace protocol
}  // namespace content
