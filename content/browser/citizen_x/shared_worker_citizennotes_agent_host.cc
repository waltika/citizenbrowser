// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/shared_worker_citizennotes_agent_host.h"

#include <memory>
#include <utility>

#include "content/browser/citizen_x/citizennotes_renderer_channel.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/browser/citizen_x/protocol/cnfetch_handler.h"
#include "content/browser/citizen_x/protocol/cninspector_handler.h"
#include "content/browser/citizen_x/protocol/cnio_handler.h"
#include "content/browser/citizen_x/protocol/cnnetwork_handler.h"
#include "content/browser/citizen_x/protocol/protocol.h"
#include "content/browser/citizen_x/protocol/cnschema_handler.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_manager.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace content {

// static
SharedWorkerCitizenNotesAgentHost* SharedWorkerCitizenNotesAgentHost::GetFor(
    SharedWorkerHost* worker_host) {
  return SharedWorkerCitizenNotesManager::GetInstance()->GetCitizenNotesHost(
      worker_host);
}

SharedWorkerCitizenNotesAgentHost::SharedWorkerCitizenNotesAgentHost(
    SharedWorkerHost* worker_host,
    const base::UnguessableToken& citizennotes_worker_token)
    : CitizenNotesAgentHostImpl(citizennotes_worker_token.ToString()),
      auto_attacher_(std::make_unique<protocol::CNRendererAutoAttacherBase>(
          GetRendererChannel())),
      state_(WORKER_NOT_READY),
      worker_host_(worker_host),
      citizennotes_worker_token_(citizennotes_worker_token),
      instance_(worker_host->instance()) {
  NotifyCreated();
}

SharedWorkerCitizenNotesAgentHost::~SharedWorkerCitizenNotesAgentHost() {
  SharedWorkerCitizenNotesManager::GetInstance()->AgentHostDestroyed(this);
}

BrowserContext* SharedWorkerCitizenNotesAgentHost::GetBrowserContext() {
  if (!worker_host_)
    return nullptr;
  return worker_host_->GetProcessHost()->GetBrowserContext();
}

std::string SharedWorkerCitizenNotesAgentHost::GetType() {
  return kTypeSharedWorker;
}

std::string SharedWorkerCitizenNotesAgentHost::GetTitle() {
  return instance_.name();
}

GURL SharedWorkerCitizenNotesAgentHost::GetURL() {
  return instance_.url();
}

blink::StorageKey SharedWorkerCitizenNotesAgentHost::GetStorageKey() const {
  return instance_.storage_key();
}

bool SharedWorkerCitizenNotesAgentHost::Activate() {
  return false;
}

void SharedWorkerCitizenNotesAgentHost::Reload() {
}

bool SharedWorkerCitizenNotesAgentHost::Close() {
  if (worker_host_)
    worker_host_->Destruct();
  return true;
}

bool SharedWorkerCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                                  bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::CNIOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::CNInspectorHandler>();
  session->CreateAndAddHandler<protocol::CNNetworkHandler>(
      GetId(), citizennotes_worker_token_, GetIOContext(),
      base::BindRepeating([] {}), session->GetClient()->MayReadLocalFiles(),
      session->GetClient()->IsTrusted());
  // TODO(crbug.com/1143100): support pushing updated loader factories down to
  // renderer.
  session->CreateAndAddHandler<protocol::CNFetchHandler>(
      GetIOContext(),
      base::BindRepeating([](base::OnceClosure cb) { std::move(cb).Run(); }));
  session->CreateAndAddHandler<protocol::CNSchemaHandler>();
  session->CreateAndAddHandler<protocol::CNTargetHandler>(
      protocol::CNTargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  return true;
}

void SharedWorkerCitizenNotesAgentHost::DetachSession(CitizenNotesSession* session) {
  // Destroying session automatically detaches in renderer.
}

bool SharedWorkerCitizenNotesAgentHost::Matches(SharedWorkerHost* worker_host) {
  return instance_.Matches(worker_host->instance().url(),
                           worker_host->instance().name(),
                           worker_host->instance().storage_key(),
                           worker_host->instance().same_site_cookies());
}

void SharedWorkerCitizenNotesAgentHost::WorkerReadyForInspection(
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
        agent_host_receiver) {
  DCHECK_EQ(WORKER_NOT_READY, state_);
  DCHECK(worker_host_);
  state_ = WORKER_READY;
  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(agent_host_receiver),
                                    worker_host_->GetProcessHost()->GetID());
  for (auto* inspector : protocol::CNInspectorHandler::ForAgentHost(this))
    inspector->TargetReloadedAfterCrash();
}

void SharedWorkerCitizenNotesAgentHost::WorkerRestarted(
    SharedWorkerHost* worker_host) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  DCHECK(!worker_host_);
  state_ = WORKER_NOT_READY;
  worker_host_ = worker_host;
}

void SharedWorkerCitizenNotesAgentHost::WorkerDestroyed() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  DCHECK(worker_host_);
  state_ = WORKER_TERMINATED;
  for (auto* inspector : protocol::CNInspectorHandler::ForAgentHost(this))
    inspector->TargetCrashed();
  worker_host_ = nullptr;
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
}

CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo
SharedWorkerCitizenNotesAgentHost::CreateNetworkFactoryParamsForCitizenNotes() {
  DCHECK(worker_host_);
  return {GetStorageKey().origin(), net::SiteForCookies::FromUrl(GetURL()),
          worker_host_->CreateNetworkFactoryParamsForSubresources()};
}

RenderProcessHost* SharedWorkerCitizenNotesAgentHost::GetProcessHost() {
  DCHECK(worker_host_);
  return worker_host_->GetProcessHost();
}

protocol::CNTargetAutoAttacher* SharedWorkerCitizenNotesAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

}  // namespace content
