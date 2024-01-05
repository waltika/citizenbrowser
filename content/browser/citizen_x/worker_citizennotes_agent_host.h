// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_WORKER_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_WORKER_CITIZENNOTES_AGENT_HOST_H_

#include "base/unguessable_token.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"
#include "url/gurl.h"

namespace content {

class DedicatedWorkerHost;

// The WorkerCitizenNotesAgentHost is the citizennotes host class for dedicated workers,
// (but not shared or service workers), and worklets. It does not have a pointer
// to a DedicatedWorkerHost object, but in case the host is for a dedicated
// worker (and not a worklet) then the citizennotes_worker_token_ is identical to
// the DedicatedWorkerToken of the dedicated worker.
class WorkerCitizenNotesAgentHost : public CitizenNotesAgentHostImpl {
 public:
  static WorkerCitizenNotesAgentHost* GetFor(DedicatedWorkerHost* host);

  WorkerCitizenNotesAgentHost(
      int process_id,
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& citizennotes_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback);

  WorkerCitizenNotesAgentHost(const WorkerCitizenNotesAgentHost&) = delete;
  WorkerCitizenNotesAgentHost& operator=(const WorkerCitizenNotesAgentHost&) = delete;

  // CitizenNotesAgentHost override.
  BrowserContext* GetBrowserContext() override;
  RenderProcessHost* GetProcessHost() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetParentId() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  absl::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;

  void SetRenderer(
      int process_id,
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver);

  void ChildWorkerCreated(
      const GURL& url,
      const std::string& name,
      base::OnceCallback<void(CitizenNotesAgentHostImpl*)> callback);

  const base::UnguessableToken& citizennotes_worker_token() const {
    return citizennotes_worker_token_;
  }

 private:
  ~WorkerCitizenNotesAgentHost() override;
  void Disconnected();
  DedicatedWorkerHost* GetDedicatedWorkerHost();

  // CitizenNotesAgentHostImpl overrides.
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;
  void DetachSession(CitizenNotesSession* session) override;
  protocol::CNTargetAutoAttacher* auto_attacher() override;

  const int process_id_;
  GURL url_;
  std::string name_;
  const std::string parent_id_;
  std::unique_ptr<protocol::CNTargetAutoAttacher> auto_attacher_;
  base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback_;
  const base::UnguessableToken citizennotes_worker_token_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_WORKER_CITIZENNOTES_AGENT_HOST_H_
