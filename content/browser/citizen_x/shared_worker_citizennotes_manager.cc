// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/shared_worker_citizennotes_manager.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_agent_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
SharedWorkerCitizenNotesManager* SharedWorkerCitizenNotesManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<SharedWorkerCitizenNotesManager>::get();
}

void SharedWorkerCitizenNotesManager::AddAllAgentHosts(
    SharedWorkerCitizenNotesAgentHost::List* result) {
  for (auto& it : live_hosts_)
    result->push_back(it.second.get());
}

void SharedWorkerCitizenNotesManager::WorkerCreated(
    SharedWorkerHost* worker_host,
    bool* pause_on_start,
    base::UnguessableToken* citizennotes_worker_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::Contains(live_hosts_, worker_host));

  auto it = base::ranges::find_if(
      terminated_hosts_,
      [&worker_host](SharedWorkerCitizenNotesAgentHost* agent_host) {
        return agent_host->Matches(worker_host);
      });
  if (it == terminated_hosts_.end()) {
    *citizennotes_worker_token = base::UnguessableToken::Create();
    live_hosts_[worker_host] =
        new SharedWorkerCitizenNotesAgentHost(worker_host, *citizennotes_worker_token);
    *pause_on_start = false;
    return;
  }

  SharedWorkerCitizenNotesAgentHost* agent_host = *it;
  terminated_hosts_.erase(it);
  live_hosts_[worker_host] = agent_host;
  agent_host->WorkerRestarted(worker_host);
  *pause_on_start = agent_host->IsAttached();
  *citizennotes_worker_token = agent_host->citizennotes_worker_token();
}

void SharedWorkerCitizenNotesManager::WorkerReadyForInspection(
    SharedWorkerHost* worker_host,
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
        agent_host_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  live_hosts_[worker_host]->WorkerReadyForInspection(
      std::move(agent_remote), std::move(agent_host_receiver));
}

void SharedWorkerCitizenNotesManager::WorkerDestroyed(
    SharedWorkerHost* worker_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<SharedWorkerCitizenNotesAgentHost> agent_host =
      live_hosts_[worker_host];
  live_hosts_.erase(worker_host);
  terminated_hosts_.insert(agent_host.get());
  agent_host->WorkerDestroyed();
}

void SharedWorkerCitizenNotesManager::AgentHostDestroyed(
    SharedWorkerCitizenNotesAgentHost* agent_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = terminated_hosts_.find(agent_host);
  // Might be missing during shutdown due to different
  // destruction order of this manager, shared workers
  // and their agent hosts.
  if (it != terminated_hosts_.end())
    terminated_hosts_.erase(it);
}

SharedWorkerCitizenNotesAgentHost* SharedWorkerCitizenNotesManager::GetCitizenNotesHost(
    SharedWorkerHost* host) {
  auto it = live_hosts_.find(host);
  return it == live_hosts_.end() ? nullptr : it->second.get();
}

SharedWorkerCitizenNotesManager::SharedWorkerCitizenNotesManager() = default;
SharedWorkerCitizenNotesManager::~SharedWorkerCitizenNotesManager() = default;

}  // namespace content
