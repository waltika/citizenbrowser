// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_SHARED_STORAGE_WORKLET_CITIZENNOTES_MANAGER_H_
#define CONTENT_BROWSER_CITIZENNOTES_SHARED_STORAGE_WORKLET_CITIZENNOTES_MANAGER_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace content {

class SharedStorageWorkletCitizenNotesAgentHost;
class SharedStorageWorkletHost;
class RenderFrameHostImpl;

// Manages `SharedStorageWorkletCitizenNotesAgentHost`s for Shared Storage Worklets.
class SharedStorageWorkletCitizenNotesManager {
 public:
  class Observer {
   public:
    virtual void SharedStorageWorkletCreated(
        SharedStorageWorkletCitizenNotesAgentHost* host,
        bool& should_pause_on_start) {}

    virtual void SharedStorageWorkletDestroyed(
        SharedStorageWorkletCitizenNotesAgentHost* host) {}

    virtual ~Observer() = default;
  };

  // Returns the SharedStorageWorkletCitizenNotesManager singleton.
  static SharedStorageWorkletCitizenNotesManager* GetInstance();

  SharedStorageWorkletCitizenNotesManager(
      const SharedStorageWorkletCitizenNotesManager&) = delete;
  SharedStorageWorkletCitizenNotesManager& operator=(
      const SharedStorageWorkletCitizenNotesManager&) = delete;

  void AddAllAgentHosts(std::vector<scoped_refptr<CitizenNotesAgentHost>>* result);

  void WorkletCreated(SharedStorageWorkletHost& worklet_host,
                      const base::UnguessableToken& devtools_worklet_token,
                      bool& wait_for_debugger);
  void WorkletReadyForInspection(
      SharedStorageWorkletHost& worklet_host,
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
          agent_host_receiver);
  void WorkletDestroyed(SharedStorageWorkletHost& worklet_host);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void GetAllForFrame(RenderFrameHostImpl* frame, CitizenNotesAgentHost::List* out);

 private:
  friend struct base::DefaultSingletonTraits<
      SharedStorageWorkletCitizenNotesManager>;

  SharedStorageWorkletCitizenNotesManager();
  ~SharedStorageWorkletCitizenNotesManager();

  base::ObserverList<Observer>::Unchecked observer_list_;

  // We retatin agent hosts as long as the shared storage worklet is alive.
  std::map<SharedStorageWorkletHost*,
           scoped_refptr<SharedStorageWorkletCitizenNotesAgentHost>>
      hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_SHARED_STORAGE_WORKLET_CITIZENNOTES_MANAGER_H_
