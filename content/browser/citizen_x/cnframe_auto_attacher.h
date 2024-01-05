// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_CITIZENNOTES_FRAME_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_CITIZENNOTES_FRAME_AUTO_ATTACHER_H_

#include "base/functional/callback.h"
#include "content/browser/citizen_x/protocol/cntarget_auto_attacher.h"
#include "content/browser/citizen_x/service_worker_citizennotes_manager.h"
#include "content/browser/citizen_x/shared_storage_worklet_citizennotes_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"

namespace content {

class CitizenNotesRendererChannel;
class FrameTree;
class NavigationRequest;
class RenderFrameHostImpl;
class ServiceWorkerCitizenNotesAgentHost;

class CNFrameAutoAttacher : public protocol::CNRendererAutoAttacherBase,
                          public ServiceWorkerCitizenNotesManager::Observer,
                          public DebuggableAuctionWorkletTracker::Observer,
                          public SharedStorageWorkletCitizenNotesManager::Observer {
 public:
  explicit CNFrameAutoAttacher(CitizenNotesRendererChannel* renderer_channel);
  ~CNFrameAutoAttacher() override;

  void SetRenderFrameHost(RenderFrameHostImpl* render_frame_host);
  void DidFinishNavigation(NavigationRequest* navigation_request);
  void UpdatePages();
  void AutoAttachToPage(FrameTree* frame_tree, bool wait_for_debugger_on_start);

 protected:
  // Base overrides.
  void UpdateAutoAttach(base::OnceClosure callback) override;

  // ServiceWorkerCitizenNotesManager::Observer implementation.
  void WorkerCreated(ServiceWorkerCitizenNotesAgentHost* host,
                     bool* should_pause_on_start) override;
  void WorkerDestroyed(ServiceWorkerCitizenNotesAgentHost* host) override;

  // DebuggableAuctionWorkletTracker::Observer implementation.
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override;

  // SharedStorageWorkletCitizenNotesManager::Observer implementation.
  void SharedStorageWorkletCreated(SharedStorageWorkletCitizenNotesAgentHost* host,
                                   bool& should_pause_on_start) override;
  void SharedStorageWorkletDestroyed(
      SharedStorageWorkletCitizenNotesAgentHost* host) override;

  void ReattachServiceWorkers();
  void UpdateFrames();

 private:
  RAW_PTR_EXCLUSION RenderFrameHostImpl* render_frame_host_ = nullptr;
  bool observing_service_workers_ = false;
  bool observing_auction_worklets_ = false;
  bool observing_shared_storage_worklets_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_FRAME_AUTO_ATTACHER_H_
