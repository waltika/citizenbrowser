// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_SHARED_STORAGE_WORKLET_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_SHARED_STORAGE_WORKLET_CITIZENNOTES_AGENT_HOST_H_

#include <string>

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"

namespace content {

class SharedStorageWorkletHost;

class CONTENT_EXPORT SharedStorageWorkletCitizenNotesAgentHost
    : public CitizenNotesAgentHostImpl {
 public:
  SharedStorageWorkletCitizenNotesAgentHost(
      SharedStorageWorkletHost& worklet_host,
      const base::UnguessableToken& devtools_worklet_token);

  SharedStorageWorkletCitizenNotesAgentHost(
      const SharedStorageWorkletCitizenNotesAgentHost&) = delete;
  SharedStorageWorkletCitizenNotesAgentHost& operator=(
      const SharedStorageWorkletCitizenNotesAgentHost&) = delete;

  void WorkletReadyForInspection(
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost>
          agent_host_receiver);
  void WorkletDestroyed();

  bool IsRelevantTo(RenderFrameHostImpl* frame);

 private:
  FRIEND_TEST_ALL_PREFIXES(SharedStorageWorkletCitizenNotesAgentHostTest,
                           BasicAttributes);

  ~SharedStorageWorkletCitizenNotesAgentHost() override;

  // CitizenNotesAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  // CitizenNotesAgentHostImpl override.
  protocol::CNTargetAutoAttacher* auto_attacher() override;
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;

  std::unique_ptr<protocol::CNTargetAutoAttacher> auto_attacher_;
  raw_ptr<SharedStorageWorkletHost> worklet_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_SHARED_STORAGE_WORKLET_CITIZENNOTES_AGENT_HOST_H_
