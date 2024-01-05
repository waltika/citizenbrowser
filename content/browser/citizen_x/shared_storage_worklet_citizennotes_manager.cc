// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/shared_storage_worklet_citizennotes_manager.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "content/browser/citizen_x/shared_storage_worklet_citizennotes_agent_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
SharedStorageWorkletCitizenNotesManager*
SharedStorageWorkletCitizenNotesManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<SharedStorageWorkletCitizenNotesManager>::get();
}

void SharedStorageWorkletCitizenNotesManager::AddAllAgentHosts(
    std::vector<scoped_refptr<CitizenNotesAgentHost>>* result) {
  for (auto& it : hosts_) {
    result->push_back(it.second.get());
  }
}

void SharedStorageWorkletCitizenNotesManager::WorkletCreated(
    SharedStorageWorkletHost& worklet_host,
    const base::UnguessableToken& citizennotes_worklet_token,
    bool& wait_for_debugger) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!base::Contains(hosts_, &worklet_host));
  CHECK(!wait_for_debugger);

  hosts_[&worklet_host] = MakeRefCounted<SharedStorageWorkletCitizenNotesAgentHost>(
      worklet_host, citizennotes_worklet_token);

  for (auto& observer : observer_list_) {
    bool should_pause_on_start = false;
    observer.SharedStorageWorkletCreated(hosts_[&worklet_host].get(),
                                         should_pause_on_start);
    wait_for_debugger |= should_pause_on_start;
  }
}

void SharedStorageWorkletCitizenNotesManager::WorkletReadyForInspection(
    SharedStorageWorkletHost& worklet_host,
    mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
        agent_host_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  hosts_.at(&worklet_host)
      ->WorkletReadyForInspection(std::move(agent_remote),
                                  std::move(agent_host_receiver));
}

void SharedStorageWorkletCitizenNotesManager::WorkletDestroyed(
    SharedStorageWorkletHost& worklet_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = hosts_.find(&worklet_host);
  CHECK(it != hosts_.end());

  it->second->WorkletDestroyed();

  for (auto& observer : observer_list_) {
    observer.SharedStorageWorkletDestroyed(it->second.get());
  }

  hosts_.erase(it);
}

void SharedStorageWorkletCitizenNotesManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SharedStorageWorkletCitizenNotesManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SharedStorageWorkletCitizenNotesManager::GetAllForFrame(
    RenderFrameHostImpl* frame,
    CitizenNotesAgentHost::List* out) {
  for (auto& it : hosts_) {
    if (it.second->IsRelevantTo(frame)) {
      out->push_back(it.second.get());
    }
  }
}

SharedStorageWorkletCitizenNotesManager::SharedStorageWorkletCitizenNotesManager() =
    default;
SharedStorageWorkletCitizenNotesManager::~SharedStorageWorkletCitizenNotesManager() =
    default;

}  // namespace content
