// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_WORKER_OR_WORKLET_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_WORKER_OR_WORKLET_CITIZENNOTES_AGENT_HOST_H_

#include "base/unguessable_token.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"
#include "url/gurl.h"

namespace content {

// This is a base class for dedicated (but not shared or service) workers and
// for common worklets. See DedicatedWorkerCitizenNotesAgentHost and
// WorkletCitizenNotesAgentHost for concrete implementation.
class WorkerOrWorkletCitizenNotesAgentHost : public CitizenNotesAgentHostImpl {
 public:
  WorkerOrWorkletCitizenNotesAgentHost(
      const WorkerOrWorkletCitizenNotesAgentHost&) = delete;
  WorkerOrWorkletCitizenNotesAgentHost& operator=(
      const WorkerOrWorkletCitizenNotesAgentHost&) = delete;

  // CitizenNotesAgentHost overrides.
  BrowserContext* GetBrowserContext() override;
  RenderProcessHost* GetProcessHost() override;
  std::string GetTitle() override;
  std::string GetParentId() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

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

 protected:
  WorkerOrWorkletCitizenNotesAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& citizennotes_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback);

  ~WorkerOrWorkletCitizenNotesAgentHost() override;

 private:
  void Disconnected();

  const base::UnguessableToken citizennotes_worker_token_;
  const std::string parent_id_;
  const int process_id_;

  GURL url_;
  std::string name_;
  base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_WORKER_OR_WORKLET_CITIZENNOTES_AGENT_HOST_H_
