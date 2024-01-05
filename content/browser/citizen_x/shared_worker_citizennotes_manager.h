// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_SHARED_WORKER_CITIZENNOTES_MANAGER_H_
#define CONTENT_BROWSER_CITIZENNOTES_SHARED_WORKER_CITIZENNOTES_MANAGER_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace content {

class SharedWorkerCitizenNotesAgentHost;
class SharedWorkerHost;

// Manages WorkerCitizenNotesAgentHost's for Shared Workers.
// This class lives on UI thread.
class SharedWorkerCitizenNotesManager {
 public:
  // Returns the SharedWorkerCitizenNotesManager singleton.
  static SharedWorkerCitizenNotesManager* GetInstance();

  SharedWorkerCitizenNotesManager(const SharedWorkerCitizenNotesManager&) = delete;
  SharedWorkerCitizenNotesManager& operator=(const SharedWorkerCitizenNotesManager&) =
      delete;

  void AddAllAgentHosts(
      std::vector<scoped_refptr<SharedWorkerCitizenNotesAgentHost>>* result);
  void AgentHostDestroyed(SharedWorkerCitizenNotesAgentHost* agent_host);

  void WorkerCreated(SharedWorkerHost* worker_host,
                     bool* pause_on_start,
                     base::UnguessableToken* citizennotes_worker_token);
  void WorkerReadyForInspection(
      SharedWorkerHost* worker_host,
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
          agent_host_receiver);
  void WorkerDestroyed(SharedWorkerHost* worker_host);

  SharedWorkerCitizenNotesAgentHost* GetCitizenNotesHost(SharedWorkerHost* host);

 private:
  friend struct base::DefaultSingletonTraits<SharedWorkerCitizenNotesManager>;

  SharedWorkerCitizenNotesManager();
  ~SharedWorkerCitizenNotesManager();

  // We retatin agent hosts as long as the shared worker is alive.
  std::map<SharedWorkerHost*, scoped_refptr<SharedWorkerCitizenNotesAgentHost>>
      live_hosts_;
  // Clients may retain agent host for the terminated shared worker,
  // and we reconnect them when shared worker is restarted.
  base::flat_set<SharedWorkerCitizenNotesAgentHost*> terminated_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_SHARED_WORKER_CITIZENNOTES_MANAGER_H_
