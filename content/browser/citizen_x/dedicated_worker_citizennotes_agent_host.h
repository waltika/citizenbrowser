// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_DEDICATED_WORKER_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_DEDICATED_WORKER_CITIZENNOTES_AGENT_HOST_H_

#include "content/browser/citizen_x/worker_or_worklet_citizennotes_agent_host.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace content {

namespace protocol {
class CNTargetAutoAttacher;
}  // namespace protocol

class DedicatedWorkerHost;

class DedicatedWorkerCitizenNotesAgentHost final
    : public WorkerOrWorkletCitizenNotesAgentHost {
 public:
  static DedicatedWorkerCitizenNotesAgentHost* GetFor(DedicatedWorkerHost* host);

  DedicatedWorkerCitizenNotesAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& citizennotes_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback);

 private:
  ~DedicatedWorkerCitizenNotesAgentHost() override;

  // CitizenNotesAgentHost overrides
  std::string GetType() override;

  // CitizenNotesAgentHostImpl overrides
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;
  protocol::CNTargetAutoAttacher* auto_attacher() override;
  std::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

  DedicatedWorkerHost* GetDedicatedWorkerHost();

  std::unique_ptr<protocol::CNTargetAutoAttacher> const auto_attacher_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_CITIZENNOTES_DEDICATED_WORKER_CITIZENNOTES_AGENT_HOST_H_
