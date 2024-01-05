// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_AUCTION_WORKLET_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_AUCTION_WORKLET_CITIZENNOTES_AGENT_HOST_H_

#include <map>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/gurl.h"

namespace content {

class DebuggableAuctionWorklet;

class AuctionWorkletCitizenNotesAgentHost : public CitizenNotesAgentHostImpl {
 public:
  static bool IsRelevantTo(RenderFrameHostImpl* frame,
                           DebuggableAuctionWorklet* candidate);

 private:
  friend class AuctionWorkletCitizenNotesAgentHostManager;

  static scoped_refptr<AuctionWorkletCitizenNotesAgentHost> Create(
      DebuggableAuctionWorklet* worklet);

  explicit AuctionWorkletCitizenNotesAgentHost(DebuggableAuctionWorklet* worklet);
  ~AuctionWorkletCitizenNotesAgentHost() override;

  // CitizenNotesAgentHost override.
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  void Reload() override;
  std::string GetParentId() override;
  BrowserContext* GetBrowserContext() override;

  // Called by WorkerCitizenNotesAgentHostManager to specify the worklet got
  // destroyed.
  void WorkletDestroyed();

  // CitizenNotesAgentHostImpl overrides.
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;

  RAW_PTR_EXCLUSION DebuggableAuctionWorklet* worklet_ = nullptr;
  mojo::AssociatedRemote<blink::mojom::CitizenNotesAgent> associated_agent_remote_;
};

class AuctionWorkletCitizenNotesAgentHostManager
    : public DebuggableAuctionWorkletTracker::Observer {
 public:
  // Both of these append to `out`.
  void GetAll(CitizenNotesAgentHost::List* out);
  void GetAllForFrame(RenderFrameHostImpl* frame, CitizenNotesAgentHost::List* out);

  scoped_refptr<AuctionWorkletCitizenNotesAgentHost> GetOrCreateFor(
      DebuggableAuctionWorklet* worklet);

  static AuctionWorkletCitizenNotesAgentHostManager& GetInstance();

 private:
  friend class AuctionWorkletCitizenNotesAgentHost;
  friend class base::NoDestructor<AuctionWorkletCitizenNotesAgentHostManager>;

  AuctionWorkletCitizenNotesAgentHostManager();
  ~AuctionWorkletCitizenNotesAgentHostManager() override;

  // DebuggableAuctionWorkletTracker::Observer implementation.
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override;
  void AuctionWorkletDestroyed(DebuggableAuctionWorklet* worklet) override;

  std::map<DebuggableAuctionWorklet*,
           scoped_refptr<AuctionWorkletCitizenNotesAgentHost>>
      hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_AUCTION_WORKLET_CITIZENNOTES_AGENT_HOST_H_
