// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/worker_or_worklet_citizennotes_agent_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/citizen_x/worker_citizennotes_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/child_process_host.h"
#include "third_party/blink/public/common/features.h"

namespace content {

WorkerOrWorkletCitizenNotesAgentHost::WorkerOrWorkletCitizenNotesAgentHost(
    int process_id,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& citizennotes_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback)
    : CitizenNotesAgentHostImpl(citizennotes_worker_token.ToString()),
      citizennotes_worker_token_(citizennotes_worker_token),
      parent_id_(parent_id),
      process_id_(process_id),
      url_(url),
      name_(name),
      destroyed_callback_(std::move(destroyed_callback)) {
  DCHECK(!citizennotes_worker_token.is_empty());
  // TODO(crbug.com/906991): Remove AddRef() and Release() once
  // PlzDedicatedWorker is enabled and the code for non-PlzDedicatedWorker is
  // deleted. Worker agent hosts will be retained by the Worker CitizenNotes manager
  // instead.
  AddRef();  // Self keep-alive while the worker agent is alive.
}

WorkerOrWorkletCitizenNotesAgentHost::~WorkerOrWorkletCitizenNotesAgentHost() = default;

void WorkerOrWorkletCitizenNotesAgentHost::SetRenderer(
    int process_id,
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver) {
  DCHECK(agent_remote);
  DCHECK(host_receiver);

  base::OnceClosure connection_error = (base::BindOnce(
      &WorkerOrWorkletCitizenNotesAgentHost::Disconnected, base::Unretained(this)));
  GetRendererChannel()->SetRenderer(std::move(agent_remote),
                                    std::move(host_receiver), process_id,
                                    std::move(connection_error));
  ProcessHostChanged();
}

void WorkerOrWorkletCitizenNotesAgentHost::ChildWorkerCreated(
    const GURL& url,
    const std::string& name,
    base::OnceCallback<void(CitizenNotesAgentHostImpl*)> callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  url_ = url;
  name_ = name;
  destroyed_callback_ = std::move(callback);
}

void WorkerOrWorkletCitizenNotesAgentHost::Disconnected() {
  auto retain_this = ForceDetachAllSessionsImpl();
  GetRendererChannel()->SetRenderer(mojo::NullRemote(), mojo::NullReceiver(),
                                    ChildProcessHost::kInvalidUniqueID);
  std::move(destroyed_callback_).Run(this);
  Release();  // Matches AddRef() in constructor.
}

BrowserContext* WorkerOrWorkletCitizenNotesAgentHost::GetBrowserContext() {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  return process ? process->GetBrowserContext() : nullptr;
}

RenderProcessHost* WorkerOrWorkletCitizenNotesAgentHost::GetProcessHost() {
  return RenderProcessHost::FromID(process_id_);
}

std::string WorkerOrWorkletCitizenNotesAgentHost::GetTitle() {
  return name_.empty() ? url_.spec() : name_;
}

std::string WorkerOrWorkletCitizenNotesAgentHost::GetParentId() {
  return parent_id_;
}

GURL WorkerOrWorkletCitizenNotesAgentHost::GetURL() {
  return url_;
}

bool WorkerOrWorkletCitizenNotesAgentHost::Activate() {
  return false;
}

void WorkerOrWorkletCitizenNotesAgentHost::Reload() {}

bool WorkerOrWorkletCitizenNotesAgentHost::Close() {
  return false;
}

}  // namespace content
