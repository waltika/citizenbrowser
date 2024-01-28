// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/dedicated_worker_citizennotes_agent_host.h"

#include "content/browser/citizen_x/protocol/cnio_handler.h"
#include "content/browser/citizen_x/protocol/cnnetwork_handler.h"
#include "content/browser/citizen_x/protocol/cntarget_auto_attacher.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/citizen_x/worker_citizennotes_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"

namespace content {

// static
DedicatedWorkerCitizenNotesAgentHost* DedicatedWorkerCitizenNotesAgentHost::GetFor(
    DedicatedWorkerHost* host) {
  return WorkerCitizenNotesManager::GetInstance().GetCitizenNotesHost(host);
}

DedicatedWorkerCitizenNotesAgentHost::DedicatedWorkerCitizenNotesAgentHost(
    int process_id,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& citizennotes_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback)
    : WorkerOrWorkletCitizenNotesAgentHost(process_id,
                                       url,
                                       name,
                                       citizennotes_worker_token,
                                       parent_id,
                                       std::move(destroyed_callback)),
      auto_attacher_(std::make_unique<protocol::CNRendererAutoAttacherBase>(
          GetRendererChannel())) {
  NotifyCreated();
}

DedicatedWorkerCitizenNotesAgentHost::~DedicatedWorkerCitizenNotesAgentHost() = default;

std::string DedicatedWorkerCitizenNotesAgentHost::GetType() {
  return kTypeDedicatedWorker;
}

DedicatedWorkerHost*
DedicatedWorkerCitizenNotesAgentHost::GetDedicatedWorkerHost() {
  RenderProcessHost* const process = GetProcessHost();
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition());
  auto* service = storage_partition_impl->GetDedicatedWorkerService();
  return service->GetDedicatedWorkerHostFromToken(
      blink::DedicatedWorkerToken(citizennotes_worker_token()));
}

bool DedicatedWorkerCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                                     bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::CNIOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::CNTargetHandler>(
      protocol::CNTargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  session->CreateAndAddHandler<protocol::CNNetworkHandler>(
      GetId(), citizennotes_worker_token(), GetIOContext(), base::DoNothing(),
      session->GetClient()->MayReadLocalFiles(),
      session->GetClient()->IsTrusted());
  return true;
}

protocol::CNTargetAutoAttacher*
DedicatedWorkerCitizenNotesAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

std::optional<network::CrossOriginEmbedderPolicy>
DedicatedWorkerCitizenNotesAgentHost::cross_origin_embedder_policy(
    const std::string&) {
  DedicatedWorkerHost* const host = GetDedicatedWorkerHost();
  return host ? std::make_optional(host->cross_origin_embedder_policy())
              : std::nullopt;
}

}  // namespace content
