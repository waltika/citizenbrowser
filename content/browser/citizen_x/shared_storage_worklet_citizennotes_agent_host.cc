// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/shared_storage_worklet_citizennotes_agent_host.h"

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "content/browser/citizen_x/citizennotes_renderer_channel.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/browser/citizen_x/protocol/cninspector_handler.h"
#include "content/browser/citizen_x/protocol/protocol.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace content {

namespace {

RenderFrameHostImpl* ContainingLocalRoot(RenderFrameHostImpl* frame) {
  while (!frame->is_local_root()) {
    frame = frame->GetParent();
  }
  return frame;
}

}  // namespace

SharedStorageWorkletCitizenNotesAgentHost::SharedStorageWorkletCitizenNotesAgentHost(
    SharedStorageWorkletHost& worklet_host,
    const base::UnguessableToken& citizennotes_worklet_token)
    : CitizenNotesAgentHostImpl(citizennotes_worklet_token.ToString()),
      auto_attacher_(std::make_unique<protocol::CNRendererAutoAttacherBase>(
          GetRendererChannel())),
      worklet_host_(&worklet_host) {
  NotifyCreated();
}

SharedStorageWorkletCitizenNotesAgentHost::
    ~SharedStorageWorkletCitizenNotesAgentHost() = default;

BrowserContext* SharedStorageWorkletCitizenNotesAgentHost::GetBrowserContext() {
  if (!worklet_host_ || !worklet_host_->GetProcessHost()) {
    return nullptr;
  }

  return worklet_host_->GetProcessHost()->GetBrowserContext();
}

std::string SharedStorageWorkletCitizenNotesAgentHost::GetType() {
  return kTypeSharedStorageWorklet;
}

std::string SharedStorageWorkletCitizenNotesAgentHost::GetTitle() {
  if (!worklet_host_) {
    return std::string();
  }

  return base::StrCat({"Shared storage worklet for ",
                       worklet_host_->script_source_url().spec()});
}

GURL SharedStorageWorkletCitizenNotesAgentHost::GetURL() {
  return worklet_host_ ? worklet_host_->script_source_url() : GURL();
}

bool SharedStorageWorkletCitizenNotesAgentHost::Activate() {
  return false;
}

void SharedStorageWorkletCitizenNotesAgentHost::Reload() {}

bool SharedStorageWorkletCitizenNotesAgentHost::Close() {
  return false;
}

bool SharedStorageWorkletCitizenNotesAgentHost::AttachSession(
    CitizenNotesSession* session,
    bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::CNInspectorHandler>();
  session->CreateAndAddHandler<protocol::CNTargetHandler>(
      protocol::CNTargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  return true;
}

void SharedStorageWorkletCitizenNotesAgentHost::WorkletReadyForInspection(
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
        agent_host_receiver) {
  CHECK(worklet_host_->GetProcessHost());

  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(agent_host_receiver),
                                    worklet_host_->GetProcessHost()->GetID());
}

void SharedStorageWorkletCitizenNotesAgentHost::WorkletDestroyed() {
  CHECK(worklet_host_);
  worklet_host_ = nullptr;

  for (auto* inspector : protocol::CNInspectorHandler::ForAgentHost(this)) {
    inspector->TargetCrashed();
  }
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
}

bool SharedStorageWorkletCitizenNotesAgentHost::IsRelevantTo(
    RenderFrameHostImpl* frame) {
  return ContainingLocalRoot(frame) ==
         ContainingLocalRoot(worklet_host_->GetFrame());
}

protocol::CNTargetAutoAttacher*
SharedStorageWorkletCitizenNotesAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

}  // namespace content
