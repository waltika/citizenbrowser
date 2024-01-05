// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/worker_citizennotes_agent_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/browser/citizen_x/protocol/cnio_handler.h"
#include "content/browser/citizen_x/protocol/cnnetwork_handler.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_agent_host.h"
#include "content/browser/citizen_x/worker_citizennotes_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/public/browser/child_process_host.h"

namespace content {

namespace protocol {
class CNTargetAutoAttacher;
}  // namespace protocol

// static
WorkerCitizenNotesAgentHost* WorkerCitizenNotesAgentHost::GetFor(
    DedicatedWorkerHost* host) {
  return WorkerCitizenNotesManager::GetInstance().GetCitizenNotesHost(host);
}

WorkerCitizenNotesAgentHost::WorkerCitizenNotesAgentHost(
    int process_id,
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& citizennotes_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback)
    : CitizenNotesAgentHostImpl(citizennotes_worker_token.ToString()),
      process_id_(process_id),
      url_(url),
      name_(name),
      parent_id_(parent_id),
      auto_attacher_(std::make_unique<protocol::CNRendererAutoAttacherBase>(
          GetRendererChannel())),
      destroyed_callback_(std::move(destroyed_callback)),
      citizennotes_worker_token_(citizennotes_worker_token) {
  DCHECK(!citizennotes_worker_token.is_empty());
  // TODO(crbug.com/906991): Remove AddRef() and Release() once
  // PlzDedicatedWorker is enabled and the code for non-PlzDedicatedWorker is
  // deleted. Worker agent hosts will be retained by the Worker CitizenNotes manager
  // instead.
  AddRef();  // Self keep-alive while the worker agent is alive.
  NotifyCreated();

  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker))
    SetRenderer(process_id, std::move(agent_remote), std::move(host_receiver));
}

WorkerCitizenNotesAgentHost::~WorkerCitizenNotesAgentHost() = default;

void WorkerCitizenNotesAgentHost::SetRenderer(
    int process_id,
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver) {
  DCHECK(agent_remote);
  DCHECK(host_receiver);

  base::OnceClosure connection_error = (base::BindOnce(
      &WorkerCitizenNotesAgentHost::Disconnected, base::Unretained(this)));
  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(host_receiver), process_id,
                                    std::move(connection_error));
  ProcessHostChanged();
}

void WorkerCitizenNotesAgentHost::ChildWorkerCreated(
    const GURL& url,
    const std::string& name,
    base::OnceCallback<void(CitizenNotesAgentHostImpl*)> callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  url_ = url;
  name_ = name;
  destroyed_callback_ = std::move(callback);
}

void WorkerCitizenNotesAgentHost::Disconnected() {
  auto retain_this = ForceDetachAllSessionsImpl();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  std::move(destroyed_callback_).Run(this);
  Release();  // Matches AddRef() in constructor.
}

BrowserContext* WorkerCitizenNotesAgentHost::GetBrowserContext() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  return process ? process->GetBrowserContext() : nullptr;
}

RenderProcessHost* WorkerCitizenNotesAgentHost::GetProcessHost() {
  return RenderProcessHost::FromID(process_id_);
}

std::string WorkerCitizenNotesAgentHost::GetType() {
  return kTypeDedicatedWorker;
}

std::string WorkerCitizenNotesAgentHost::GetTitle() {
  return name_.empty() ? url_.spec() : name_;
}

std::string WorkerCitizenNotesAgentHost::GetParentId() {
  return parent_id_;
}

GURL WorkerCitizenNotesAgentHost::GetURL() {
  return url_;
}

bool WorkerCitizenNotesAgentHost::Activate() {
  return false;
}

void WorkerCitizenNotesAgentHost::Reload() {}

bool WorkerCitizenNotesAgentHost::Close() {
  return false;
}

bool WorkerCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                            bool acquire_wake_lock) {
  session->CreateAndAddHandler<protocol::CNIOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::CNTargetHandler>(
      protocol::CNTargetHandler::AccessMode::kAutoAttachOnly, GetId(),
      auto_attacher_.get(), session);
  session->CreateAndAddHandler<protocol::CNNetworkHandler>(
      GetId(), citizennotes_worker_token_, GetIOContext(), base::DoNothing(),
      session->GetClient()->MayReadLocalFiles(),
      session->GetClient()->IsTrusted());
  return true;
}

void WorkerCitizenNotesAgentHost::DetachSession(CitizenNotesSession* session) {
  // Destroying session automatically detaches in renderer.
}

protocol::CNTargetAutoAttacher* WorkerCitizenNotesAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

DedicatedWorkerHost* WorkerCitizenNotesAgentHost::GetDedicatedWorkerHost() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition());
  auto* service = storage_partition_impl->GetDedicatedWorkerService();
  return service->GetDedicatedWorkerHostFromToken(
      blink::DedicatedWorkerToken(citizennotes_worker_token_));
}

absl::optional<network::CrossOriginEmbedderPolicy>
WorkerCitizenNotesAgentHost::cross_origin_embedder_policy(const std::string&) {
  DedicatedWorkerHost* host = GetDedicatedWorkerHost();
  if (!host) {
    return absl::nullopt;
  }
  return host->cross_origin_embedder_policy();
}

}  // namespace content
