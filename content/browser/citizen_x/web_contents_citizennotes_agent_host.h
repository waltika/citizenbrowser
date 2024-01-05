// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_WEB_CONTENTS_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_WEB_CONTENTS_CITIZENNOTES_AGENT_HOST_H_

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class FrameTreeNode;
class Portal;

class CONTENT_EXPORT WebContentsCitizenNotesAgentHost
    : public CitizenNotesAgentHostImpl,
      public WebContentsObserver {
 public:
  // Returns appropriate agent host for given Web Contents
  static WebContentsCitizenNotesAgentHost* GetFor(WebContents* web_contents);
  // Similar to GetFor(), but creates a host if it doesn't exist yet.
  static WebContentsCitizenNotesAgentHost* GetOrCreateFor(
      WebContents* web_contents);

  static bool IsDebuggerAttached(WebContents* web_contents);

  WebContentsCitizenNotesAgentHost(const WebContentsCitizenNotesAgentHost&) = delete;
  WebContentsCitizenNotesAgentHost& operator=(const WebContentsCitizenNotesAgentHost&) =
      delete;

  static void AddAllAgentHosts(CitizenNotesAgentHost::List* result);

  // CitizenNotesAgentHostImpl overrides.
  protocol::CNTargetAutoAttacher* auto_attacher() override;

  // Instrumentation methods
  void PortalActivated(const Portal& portal);
  void WillInitiatePrerender(FrameTreeNode* ftn);
  // TODO(caseq): do we need more specific signals here?
  void UpdateChildFrameTrees(bool update_target_info);
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;

 private:
  class AutoAttacher;

  explicit WebContentsCitizenNotesAgentHost(WebContents* wc);
  ~WebContentsCitizenNotesAgentHost() override;

  void InnerAttach(WebContents* web_contents);
  void InnerDetach();

  // CitizenNotesAgentHost overrides.
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

  // CitizenNotesAgentHostImpl overrides.
  CitizenNotesSession::Mode GetSessionMode() override;
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;

  // WebContentsObserver overrides.
  void WebContentsDestroyed() override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void FrameDeleted(int frame_tree_node_id) override;

  CitizenNotesAgentHostImpl* GetPrimaryFrameAgent();
  scoped_refptr<CitizenNotesAgentHost> GetOrCreatePrimaryFrameAgent();

  // The method returns a pointer retaining this. Once the pointer goes
  // out of scope, this may be destroyed.
  [[nodiscard]] scoped_refptr<WebContentsCitizenNotesAgentHost>
  RevalidateSessionAccess();

  std::unique_ptr<AutoAttacher> const auto_attacher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_WEB_CONTENTS_CITIZENNOTES_AGENT_HOST_H_
