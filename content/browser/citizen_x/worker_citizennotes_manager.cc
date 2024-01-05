// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/worker_citizennotes_manager.h"

#include "base/containers/contains.h"
#include "content/browser/citizen_x/citizennotes_instrumentation.h"
#include "content/browser/citizen_x/worker_citizennotes_agent_host.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
WorkerCitizenNotesManager& WorkerCitizenNotesManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  return *base::Singleton<WorkerCitizenNotesManager>::get();
}

WorkerCitizenNotesManager::WorkerCitizenNotesManager() = default;
WorkerCitizenNotesManager::~WorkerCitizenNotesManager() = default;

WorkerCitizenNotesAgentHost* WorkerCitizenNotesManager::GetCitizenNotesHost(
    DedicatedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = hosts_.find(host);
  return it == hosts_.end() ? nullptr : it->second.get();
}

WorkerCitizenNotesAgentHost* WorkerCitizenNotesManager::GetCitizenNotesHostFromToken(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& it : hosts_) {
    if (it.second->citizennotes_worker_token() == token)
      return it.second.get();
  }
  return nullptr;
}

void WorkerCitizenNotesManager::WorkerCreated(
    DedicatedWorkerHost* host,
    int process_id,
    const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
    scoped_refptr<CitizenNotesThrottleHandle> throttle_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::Contains(hosts_, host));

  hosts_[host] = base::MakeRefCounted<WorkerCitizenNotesAgentHost>(
      process_id, /*agent_remote=*/mojo::NullRemote(),
      /*host_receiver=*/mojo::NullReceiver(), /*url=*/GURL(), /*name=*/"",
      host->GetToken().value(), /*parent_id=*/"",
      /*destroyed_callback=*/base::DoNothing());

  citizennotes_instrumentation::ThrottleWorkerMainScriptFetch(
      host->GetToken().value(), ancestor_render_frame_host_id,
      std::move(throttle_handle));
}

void WorkerCitizenNotesManager::WorkerDestroyed(DedicatedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  hosts_.erase(host);
}

}  // namespace content
