// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/web_contents_citizennotes_agent_host.h"

#include "base/unguessable_token.h"
#include "content/browser/citizen_x/protocol/cnio_handler.h"
#include "content/browser/citizen_x/protocol/cntarget_auto_attacher.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/citizen_x/protocol/cntracing_handler.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {
using WebContentsCitizenNotesMap =
    std::map<WebContents*, WebContentsCitizenNotesAgentHost*>;
base::LazyInstance<WebContentsCitizenNotesMap>::Leaky g_agent_host_instances =
    LAZY_INSTANCE_INITIALIZER;

WebContentsCitizenNotesAgentHost* FindAgentHost(WebContents* wc) {
  if (!g_agent_host_instances.IsCreated())
    return nullptr;
  auto it = g_agent_host_instances.Get().find(wc);
  return it == g_agent_host_instances.Get().end() ? nullptr : it->second;
}

// This implements the CitizenNotes definition of outermost web contents,
// which is currently the one associated to outermost frame, not
// traversing the guest view (so for something within a guest view),
// this returns the parent guest view. This is for compatibility,
// as guest views were historically exposed as independent targets.
// Note this differs from `WebContents::GetResponsibleWebContents()`
// that traverses guest views.
WebContents* GetRootWebContentsForCitizenNotes(WebContents* wc) {
  RenderFrameHost* current = wc->GetPrimaryMainFrame();
  while (RenderFrameHost* parent = current->GetParentOrOuterDocument()) {
    current = parent;
  }
  return WebContents::FromRenderFrameHost(current);
}

bool ShouldCreateCitizenNotesAgentHost(WebContents* wc) {
  return wc == GetRootWebContentsForCitizenNotes(wc);
}

}  // namespace

// static
scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::GetForTab(WebContents* wc) {
  return WebContentsCitizenNotesAgentHost::GetFor(wc);
}

// static
scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::GetOrCreateForTab(
    WebContents* wc) {
  return WebContentsCitizenNotesAgentHost::GetOrCreateFor(wc);
}

class WebContentsCitizenNotesAgentHost::AutoAttacher
    : public protocol::CNTargetAutoAttacher {
 public:
  AutoAttacher() = default;

  void UpdateChildFrameTrees(bool update_target_info) {
    if (!auto_attach())
      return;
    base::flat_set<scoped_refptr<CitizenNotesAgentHost>> pages =
        UpdateAssociatedPages();
    if (update_target_info) {
      for (auto& page : pages)
        DispatchTargetInfoChanged(page.get());
    }
  }

  void WillInitiatePrerender(FrameTreeNode* ftn) {
    if (!auto_attach())
      return;
    auto host = RenderFrameCitizenNotesAgentHost::GetOrCreateFor(ftn);
    DispatchAutoAttach(host.get(), wait_for_debugger_on_start());
  }

  void SetWebContents(WebContents* wc) {
    web_contents_ = wc;
    UpdateAssociatedPages();
  }

 private:
  void UpdateAutoAttach(base::OnceClosure callback) override {
    UpdateAssociatedPages();
    protocol::CNTargetAutoAttacher::UpdateAutoAttach(std::move(callback));
  }

  base::flat_set<scoped_refptr<CitizenNotesAgentHost>> UpdateAssociatedPages() {
    base::flat_set<scoped_refptr<CitizenNotesAgentHost>> hosts;
    if (auto_attach() && web_contents_) {
      auto* rfh = static_cast<RenderFrameHostImpl*>(
          web_contents_->GetPrimaryMainFrame());
      web_contents_->ForEachRenderFrameHost(
          [&hosts](RenderFrameHost* rfh) { AddFrame(hosts, rfh); });
      // In case primary main frame has been filtered out but some criteria
      // in AddFrame(), ensure it's added.
      hosts.insert(
          RenderFrameCitizenNotesAgentHost::GetOrCreateFor(rfh->frame_tree_node()));
    }
    DispatchSetAttachedTargetsOfType(hosts, CitizenNotesAgentHost::kTypePage);
    return hosts;
  }

  static void AddFrame(base::flat_set<scoped_refptr<CitizenNotesAgentHost>>& hosts,
                       RenderFrameHost* rfh) {
    RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
    // We do not expose cached hosts as separate targets for now.
    if (rfhi->IsInBackForwardCache())
      return;
    FrameTreeNode* ftn = rfhi->frame_tree_node();
    // We're interested only in main frames, with the expcetion of fenced frames
    // that are reported as regular subframes via FrameAutoAttacher.
    if (!ftn->IsMainFrame())
      return;
    if (ftn->IsFencedFrameRoot())
      return;
    // Ignore other kinds of embedders such as GuestViews.
    if (ftn->render_manager()->GetOuterDelegateNode()) {
      return;
    }

    hosts.insert(RenderFrameCitizenNotesAgentHost::GetOrCreateFor(ftn));
  }

  RAW_PTR_EXCLUSION WebContents* web_contents_ = nullptr;
};

// static
WebContentsCitizenNotesAgentHost* WebContentsCitizenNotesAgentHost::GetFor(
    WebContents* web_contents) {
  return FindAgentHost(GetRootWebContentsForCitizenNotes(web_contents));
}

// static
WebContentsCitizenNotesAgentHost* WebContentsCitizenNotesAgentHost::GetOrCreateFor(
    WebContents* web_contents) {
  web_contents = GetRootWebContentsForCitizenNotes(web_contents);
  if (auto* host = FindAgentHost(web_contents))
    return host;
  return new WebContentsCitizenNotesAgentHost(web_contents);
}

// static
bool WebContentsCitizenNotesAgentHost::IsDebuggerAttached(
    WebContents* web_contents) {
  WebContentsCitizenNotesAgentHost* host = FindAgentHost(web_contents);
  return host && host->IsAttached();
}

// static
void WebContentsCitizenNotesAgentHost::AddAllAgentHosts(
    CitizenNotesAgentHost::List* result) {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    if (ShouldCreateCitizenNotesAgentHost(wc))
      result->push_back(GetOrCreateFor(wc));
  }
}

WebContentsCitizenNotesAgentHost::WebContentsCitizenNotesAgentHost(WebContents* wc)
    : CitizenNotesAgentHostImpl(base::UnguessableToken::Create().ToString()),
      auto_attacher_(std::make_unique<AutoAttacher>()) {
  InnerAttach(wc);
  NotifyCreated();
}

void WebContentsCitizenNotesAgentHost::InnerAttach(WebContents* wc) {
  CHECK(!web_contents());
  // With ConnectWebContents(), we may be attaching to a WC that has
  // a different host created.
  // TODO(caseq): find a better solution. See also a similar comment in
  // RenderFrameCitizenNotesAgentHost::SetFrameTreeNode();
  auto prev_entry = g_agent_host_instances.Get().find(wc);
  if (prev_entry != g_agent_host_instances.Get().end()) {
    CHECK_NE(prev_entry->second, this);
    prev_entry->second->InnerDetach();
  }
  const bool inserted =
      g_agent_host_instances.Get().insert(std::make_pair(wc, this)).second;
  CHECK(inserted);
  auto_attacher_->SetWebContents(wc);
  Observe(wc);
  // Once created, persist till underlying WC is detached, so that
  // the target id is retained.
  AddRef();
}

void WebContentsCitizenNotesAgentHost::InnerDetach() {
  DCHECK_EQ(this, FindAgentHost(web_contents()));
  auto_attacher_->SetWebContents(nullptr);
  g_agent_host_instances.Get().erase(web_contents());
  Observe(nullptr);
  // We may or may not be destruced here, depending on embedders
  // potentially retaining references.
  Release();
}

void WebContentsCitizenNotesAgentHost::WillInitiatePrerender(FrameTreeNode* ftn) {
  auto_attacher_->WillInitiatePrerender(ftn);
  for (auto* tracing : protocol::CNTracingHandler::ForAgentHost(this)) {
    tracing->WillInitiatePrerender(ftn);
  }
}

void WebContentsCitizenNotesAgentHost::UpdateChildFrameTrees(
    bool update_target_info) {
  auto_attacher_->UpdateChildFrameTrees(update_target_info);
}

void WebContentsCitizenNotesAgentHost::InspectElement(RenderFrameHost* frame_host,
                                                  int x,
                                                  int y) {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    host->InspectElement(frame_host, x, y);
  }
}

WebContentsCitizenNotesAgentHost::~WebContentsCitizenNotesAgentHost() {
  DCHECK(!web_contents());
}

void WebContentsCitizenNotesAgentHost::DisconnectWebContents() {
  InnerDetach();
}

void WebContentsCitizenNotesAgentHost::ConnectWebContents(
    WebContents* web_contents) {
  InnerAttach(web_contents);
}

BrowserContext* WebContentsCitizenNotesAgentHost::GetBrowserContext() {
  return web_contents()->GetBrowserContext();
}

WebContents* WebContentsCitizenNotesAgentHost::GetWebContents() {
  return web_contents();
}

std::string WebContentsCitizenNotesAgentHost::GetParentId() {
  return std::string();
}

std::string WebContentsCitizenNotesAgentHost::GetOpenerId() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetOpenerId();
  return "";
}

std::string WebContentsCitizenNotesAgentHost::GetOpenerFrameId() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetOpenerFrameId();
  return "";
}

bool WebContentsCitizenNotesAgentHost::CanAccessOpener() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->CanAccessOpener();
  return false;
}

std::string WebContentsCitizenNotesAgentHost::GetType() {
  return kTypeTab;
}

std::string WebContentsCitizenNotesAgentHost::GetTitle() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetTitle();
  return "";
}

std::string WebContentsCitizenNotesAgentHost::GetDescription() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetDescription();
  return "";
}

GURL WebContentsCitizenNotesAgentHost::GetURL() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetURL();
  return GURL();
}

GURL WebContentsCitizenNotesAgentHost::GetFaviconURL() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetFaviconURL();
  return GURL();
}

bool WebContentsCitizenNotesAgentHost::Activate() {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    return host->Activate();
  }
  return false;
}

void WebContentsCitizenNotesAgentHost::Reload() {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    host->Reload();
  }
}

bool WebContentsCitizenNotesAgentHost::Close() {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    return host->Close();
  }
  return false;
}

base::TimeTicks WebContentsCitizenNotesAgentHost::GetLastActivityTime() {
  if (CitizenNotesAgentHost* host = GetPrimaryFrameAgent())
    return host->GetLastActivityTime();
  return base::TimeTicks();
}

absl::optional<network::CrossOriginEmbedderPolicy>
WebContentsCitizenNotesAgentHost::cross_origin_embedder_policy(
    const std::string& id) {
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<network::CrossOriginOpenerPolicy>
WebContentsCitizenNotesAgentHost::cross_origin_opener_policy(
    const std::string& id) {
  NOTREACHED();
  return absl::nullopt;
}

CitizenNotesAgentHostImpl* WebContentsCitizenNotesAgentHost::GetPrimaryFrameAgent() {
  if (WebContents* wc = web_contents()) {
    return RenderFrameCitizenNotesAgentHost::GetFor(
        static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame()));
  }
  return nullptr;
}

scoped_refptr<CitizenNotesAgentHost>
WebContentsCitizenNotesAgentHost::GetOrCreatePrimaryFrameAgent() {
  if (WebContents* wc = web_contents()) {
    return RenderFrameCitizenNotesAgentHost::GetOrCreateFor(
        static_cast<WebContentsImpl*>(wc)->GetPrimaryFrameTree().root());
  }
  return nullptr;
}

void WebContentsCitizenNotesAgentHost::WebContentsDestroyed() {
  auto retain_this = ForceDetachAllSessionsImpl();
  InnerDetach();
}

void WebContentsCitizenNotesAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CHECK(web_contents());
  if (new_host == web_contents()->GetPrimaryMainFrame()) {
    std::ignore = RevalidateSessionAccess();
    // `this` is invalid at this point!
  }
}

void WebContentsCitizenNotesAgentHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(web_contents());
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  for (auto* tracing : protocol::CNTracingHandler::ForAgentHost(this)) {
    tracing->ReadyToCommitNavigation(request);
  }
}

void WebContentsCitizenNotesAgentHost::FrameDeleted(int frame_tree_node_id) {
  for (auto* tracing : protocol::CNTracingHandler::ForAgentHost(this)) {
    tracing->FrameDeleted(frame_tree_node_id);
  }
}

// CitizenNotesAgentHostImpl overrides.
CitizenNotesSession::Mode WebContentsCitizenNotesAgentHost::GetSessionMode() {
  return CitizenNotesSession::Mode::kSupportsTabTarget;
}

bool WebContentsCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                                 bool acquire_wake_lock) {
  if (web_contents() && !RenderFrameCitizenNotesAgentHost::ShouldAllowSession(
                            web_contents()->GetPrimaryMainFrame(), session)) {
    return false;
  }
  session->SetBrowserOnly(true);
  const bool may_attach_to_brower = session->GetClient()->IsTrusted();
  session->CreateAndAddHandler<protocol::CNTargetHandler>(
      may_attach_to_brower
          ? protocol::CNTargetHandler::AccessMode::kRegular
          : protocol::CNTargetHandler::AccessMode::kAutoAttachOnly,
      GetId(), auto_attacher_.get(), session);
  CitizenNotesSession* root_session = session->GetRootSession();
  CHECK(root_session);
  session->CreateAndAddHandler<protocol::CNIOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::CNTracingHandler>(this, GetIOContext(),
                                                         root_session);
  return true;
}

protocol::CNTargetAutoAttacher* WebContentsCitizenNotesAgentHost::auto_attacher() {
  DCHECK(auto_attacher_);
  return auto_attacher_.get();
}

scoped_refptr<WebContentsCitizenNotesAgentHost>
WebContentsCitizenNotesAgentHost::RevalidateSessionAccess() {
  scoped_refptr<WebContentsCitizenNotesAgentHost> retain_this(this);
  WebContents* wc = web_contents();
  if (!wc) {
    return retain_this;
  }
  std::vector<CitizenNotesSession*> restricted_sessions;
  for (CitizenNotesSession* session : sessions()) {
    if (!RenderFrameCitizenNotesAgentHost::ShouldAllowSession(
            wc->GetPrimaryMainFrame(), session)) {
      restricted_sessions.push_back(session);
    }
  }
  if (!restricted_sessions.empty()) {
    ForceDetachRestrictedSessions(restricted_sessions);
  }
  return retain_this;
}

}  // namespace content
