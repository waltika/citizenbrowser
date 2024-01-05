// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_SHARED_WORKER_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_SHARED_WORKER_CITIZENNOTES_AGENT_HOST_H_

#include <string>
#include <vector>

#include "base/unguessable_token.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/public/browser/shared_worker_instance.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class SharedWorkerHost;

class SharedWorkerCitizenNotesAgentHost : public CitizenNotesAgentHostImpl {
 public:
  using List = std::vector<scoped_refptr<SharedWorkerCitizenNotesAgentHost>>;

  static SharedWorkerCitizenNotesAgentHost* GetFor(SharedWorkerHost* worker_host);

  SharedWorkerCitizenNotesAgentHost(
      SharedWorkerHost* worker_host,
      const base::UnguessableToken& citizennotes_worker_token);

  SharedWorkerCitizenNotesAgentHost(const SharedWorkerCitizenNotesAgentHost&) = delete;
  SharedWorkerCitizenNotesAgentHost& operator=(
      const SharedWorkerCitizenNotesAgentHost&) = delete;

  // CitizenNotesAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  NetworkLoaderFactoryParamsAndInfo CreateNetworkFactoryParamsForCitizenNotes()
      override;
  RenderProcessHost* GetProcessHost() override;
  protocol::CNTargetAutoAttacher* auto_attacher() override;

  blink::StorageKey GetStorageKey() const;

  bool Matches(SharedWorkerHost* worker_host);
  void WorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
          agent_host_receiver);
  void WorkerRestarted(SharedWorkerHost* worker_host);
  void WorkerDestroyed();

  const base::UnguessableToken& citizennotes_worker_token() const {
    return citizennotes_worker_token_;
  }

 private:
  ~SharedWorkerCitizenNotesAgentHost() override;

  // CitizenNotesAgentHostImpl overrides.
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;
  void DetachSession(CitizenNotesSession* session) override;

  std::unique_ptr<protocol::CNTargetAutoAttacher> auto_attacher_;

  enum WorkerState {
    WORKER_NOT_READY,
    WORKER_READY,
    WORKER_TERMINATED,
  };
  WorkerState state_;
  RAW_PTR_EXCLUSION SharedWorkerHost* worker_host_;
  base::UnguessableToken citizennotes_worker_token_;
  SharedWorkerInstance instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_SHARED_WORKER_CITIZENNOTES_AGENT_HOST_H_
