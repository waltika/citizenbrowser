// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_RENDER_FRAME_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_RENDER_FRAME_CITIZENNOTES_AGENT_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "build/build_config.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/android/view_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

class BrowserContext;
class FencedFrame;
class FrameTreeNode;
class CNFrameAutoAttacher;
class NavigationRequest;
class RenderFrameHostImpl;

class CONTENT_EXPORT RenderFrameCitizenNotesAgentHost
    : public CitizenNotesAgentHostImpl,
      private WebContentsObserver,
      private RenderProcessHostObserver {
 public:
  // Returns true when CitizenNotes was ever attached to any RenderFrameHost.
  // TODO(https://crbug.com/1434900): Remove this method after the experiment
  // associated with the bug entry.
  static bool WasEverAttachedToAnyFrame();

  static bool IsDebuggerAttached(WebContents* web_contents);

  static void AddAllAgentHosts(CitizenNotesAgentHost::List* result);

  // Returns appropriate agent host for given frame tree node, traversing
  // up to local root as needed.
  static CitizenNotesAgentHostImpl* GetFor(FrameTreeNode* frame_tree_node);
  // Returns appropriate agent host for given RenderFrameHost, traversing
  // up to local root as needed. This will have an effect different from
  // calling the above overload as GetFor(rfh->frame_tree_node()) when
  // given RFH is a pending local root.
  static CitizenNotesAgentHostImpl* GetFor(RenderFrameHostImpl* rfh);

  // Similar to GetFor(), but creates a host if it doesn't exist yet.
  static scoped_refptr<CitizenNotesAgentHost> GetOrCreateFor(
      FrameTreeNode* frame_tree_node);

  // Whether the RFH passed may have associated CitizenNotes agent host
  // (i.e. the specified RFH is a local root). This does not indicate
  // whether CitizenNotesAgentHost has actually been created.
  static bool ShouldCreateCitizenNotesForHost(RenderFrameHostImpl* rfh);

  // This method is called when new frame is created for an emebedded page
  // (portal or fenced frame) or local root navigation.
  static scoped_refptr<RenderFrameCitizenNotesAgentHost>
  CreateForLocalRootOrEmbeddedPageNavigation(NavigationRequest* request);
  static scoped_refptr<RenderFrameCitizenNotesAgentHost> FindForDangling(
      FrameTreeNode* frame_tree_node);

  RenderFrameCitizenNotesAgentHost(const RenderFrameCitizenNotesAgentHost&) = delete;
  RenderFrameCitizenNotesAgentHost& operator=(const RenderFrameCitizenNotesAgentHost&) =
      delete;

  static void AttachToWebContents(WebContents* web_contents);
  static bool ShouldAllowSession(RenderFrameHost* frame_host,
                                 CitizenNotesSession* session);

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  void OnNavigationRequestWillBeSent(
      const NavigationRequest& navigation_request);
  void UpdatePortals();
  void DidCreateFencedFrame(FencedFrame* fenced_frame);

  // CitizenNotesAgentHost overrides.
  // TODO(caseq): remove (Dis)connectWebContents() on frame targets once
  // front-end uses tab target mode.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetOpenerFrameId() override;
  bool CanAccessOpener() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  bool Activate() override;
  void Reload() override;

  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;

  absl::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id) override;
  absl::optional<network::CrossOriginOpenerPolicy> cross_origin_opener_policy(
      const std::string& id) override;
  absl::optional<std::vector<network::mojom::ContentSecurityPolicyHeader>>
  content_security_policy(const std::string& id) override;

  // This is used to enable compatibility shims, including disabling some
  // features that are incompatible with older clients.
  bool HasSessionsWithoutTabTargetSupport() const;

  void SetFrameTreeNode(FrameTreeNode* frame_tree_node);

  RenderFrameHostImpl* GetFrameHostForTesting() { return frame_host_; }

 private:
  friend class CitizenNotesAgentHost;
  friend class RenderFrameCitizenNotesAgentHostFencedFrameBrowserTest;

  static void UpdateRawHeadersAccess(RenderFrameHostImpl* rfh);

  RenderFrameCitizenNotesAgentHost(FrameTreeNode*, RenderFrameHostImpl*);
  ~RenderFrameCitizenNotesAgentHost() override;

  // CitizenNotesAgentHostImpl overrides.
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;
  void DetachSession(CitizenNotesSession* session) override;
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;
  void GetUniqueFormControlId(int node_id,
                              GetUniqueFormControlIdCallback callback) override;
  void UpdateRendererChannel(bool force) override;
  protocol::CNTargetAutoAttacher* auto_attacher() override;
  std::string GetSubtype() override;
  RenderProcessHost* GetProcessHost() override;
  void MainThreadDebuggerPaused() override;
  void MainThreadDebuggerResumed() override;

  // WebContentsObserver overrides.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(int frame_tree_node_id) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // RenderProcessHostObserver overrides.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  bool IsChildFrame();

  void DestroyOnRenderFrameGone();
  void UpdateFrameHost(RenderFrameHostImpl* frame_host);
  void ChangeFrameHostAndObservedProcess(RenderFrameHostImpl* frame_host);
  void UpdateFrameAlive();

#if BUILDFLAG(IS_ANDROID)
  device::mojom::WakeLock* GetWakeLock();
#endif

  void UpdateResourceLoaderFactories();

#if BUILDFLAG(IS_ANDROID)
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
#endif

  std::unique_ptr<CNFrameAutoAttacher> auto_attacher_;
  // The active host we are talking to.
  RAW_PTR_EXCLUSION RenderFrameHostImpl* frame_host_ = nullptr;
  base::flat_set<NavigationRequest*> navigation_requests_;
  bool render_frame_alive_ = false;
  bool render_frame_crashed_ = false;

  // TODO(https://crbug.com/1449114): Remove these fields once we collect enough
  // data.
  bool is_debugger_paused_ = false;
  bool is_debugger_pause_situation_recorded_ = false;

  // The FrameTreeNode associated with this agent.
  RAW_PTR_EXCLUSION FrameTreeNode* frame_tree_node_;
};

// Returns the ancestor FrameTreeNode* for which a RenderFrameCitizenNotesAgentHost
// should be created (i.e. the next local root).
FrameTreeNode* CNGetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node);

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_RENDER_FRAME_CITIZENNOTES_AGENT_HOST_H_
