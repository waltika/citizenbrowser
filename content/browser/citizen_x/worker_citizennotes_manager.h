// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_WORKER_CITIZENNOTES_MANAGER_H_
#define CONTENT_BROWSER_CITIZENNOTES_WORKER_CITIZENNOTES_MANAGER_H_

#include <map>

#include "base/memory/singleton.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/citizennotes_throttle_handle.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace content {

class WorkerCitizenNotesAgentHost;
class DedicatedWorkerHost;

// Manages WorkerCitizenNotesAgentHost's for Dedicated Workers. This class lives on
// UI thread. This is only used for PlzDedicatedWorker.
class WorkerCitizenNotesManager {
 public:
  // Returns the WorkerCitizenNotesManager singleton.
  static WorkerCitizenNotesManager& GetInstance();

  WorkerCitizenNotesAgentHost* GetCitizenNotesHost(DedicatedWorkerHost* host);
  WorkerCitizenNotesAgentHost* GetCitizenNotesHostFromToken(
      const base::UnguessableToken& token);
  void WorkerCreated(
      DedicatedWorkerHost* host,
      int process_id,
      const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
      scoped_refptr<CitizenNotesThrottleHandle> throttle_handle);
  void WorkerDestroyed(DedicatedWorkerHost* host);

 private:
  friend struct base::DefaultSingletonTraits<WorkerCitizenNotesManager>;

  WorkerCitizenNotesManager();
  ~WorkerCitizenNotesManager();

  // Retains agent hosts as long as the dedicated worker is alive.
  std::map<DedicatedWorkerHost*, scoped_refptr<WorkerCitizenNotesAgentHost>> hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_WORKER_CITIZENNOTES_MANAGER_H_
