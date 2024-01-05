// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"

#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/buildflags.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/citizen_x/citizennotes_manager.h"
#include "content/browser/citizen_x/citizennotes_renderer_channel.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/browser/citizen_x/cnframe_auto_attacher.h"
#include "content/browser/citizen_x/protocol/cnaudits_handler.h"
#include "content/browser/citizen_x/protocol/cnbackground_service_handler.h"
#include "content/browser/citizen_x/protocol/cnbrowser_handler.h"
#include "content/browser/citizen_x/protocol/cndevice_access_handler.h"
#include "content/browser/citizen_x/protocol/cndom_handler.h"
#include "content/browser/citizen_x/protocol/cnemulation_handler.h"
#include "content/browser/citizen_x/protocol/cnfedcm_handler.h"
#include "content/browser/citizen_x/protocol/cnfetch_handler.h"
#include "content/browser/citizen_x/protocol/cnhandler_helpers.h"
#include "content/browser/citizen_x/protocol/cninput_handler.h"
#include "content/browser/citizen_x/protocol/cninspector_handler.h"
#include "content/browser/citizen_x/protocol/cnio_handler.h"
#include "content/browser/citizen_x/protocol/cnlog_handler.h"
#include "content/browser/citizen_x/protocol/cnmemory_handler.h"
#include "content/browser/citizen_x/protocol/cnnetwork_handler.h"
#include "content/browser/citizen_x/protocol/cnoverlay_handler.h"
#include "content/browser/citizen_x/protocol/cnpage_handler.h"
#include "content/browser/citizen_x/protocol/cnpreload_handler.h"
#include "content/browser/citizen_x/protocol/protocol.h"
#include "content/browser/citizen_x/protocol/cnschema_handler.h"
#include "content/browser/citizen_x/protocol/cnsecurity_handler.h"
#include "content/browser/citizen_x/protocol/cnservice_worker_handler.h"
#include "content/browser/citizen_x/protocol/cnstorage_handler.h"
#include "content/browser/citizen_x/protocol/cnsystem_info_handler.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/citizen_x/protocol/cntracing_handler.h"
#include "content/browser/citizen_x/web_contents_citizennotes_agent_host.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#else
#include "content/browser/citizen_x/protocol/cnwebauthn_handler.h"
#endif

#if BUILDFLAG(USE_VIZ_DEBUGGER)
#include "content/browser/citizen_x/protocol/cnvisual_debugger_handler.h"
#endif

namespace content {

namespace {
using RenderFrameCitizenNotesMap =
    std::map<FrameTreeNode*, RenderFrameCitizenNotesAgentHost*>;
base::LazyInstance<RenderFrameCitizenNotesMap>::Leaky g_agent_host_instances =
    LAZY_INSTANCE_INITIALIZER;

static bool g_was_ever_attached_to_any_frame = false;

RenderFrameCitizenNotesAgentHost* FindAgentHost(FrameTreeNode* frame_tree_node) {
  if (!g_agent_host_instances.IsCreated())
    return nullptr;
  auto it = g_agent_host_instances.Get().find(frame_tree_node);
  return it == g_agent_host_instances.Get().end() ? nullptr : it->second;
}

bool ShouldCreateCitizenNotesForNode(FrameTreeNode* ftn) {
  return !ftn->parent() ||
         (ftn->current_frame_host() &&
          RenderFrameCitizenNotesAgentHost::ShouldCreateCitizenNotesForHost(
              ftn->current_frame_host()));
}

}  // namespace

FrameTreeNode* CNGetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node) {
  while (frame_tree_node && !ShouldCreateCitizenNotesForNode(frame_tree_node))
    frame_tree_node = FrameTreeNode::From(frame_tree_node->parent());
  DCHECK(frame_tree_node);
  return frame_tree_node;
}

// static
scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::GetOrCreateFor(
    WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetPrimaryFrameTree().root();
  // TODO(dgozman): this check should not be necessary. See
  // http://crbug.com/489664.
  if (!node)
    return nullptr;
  return RenderFrameCitizenNotesAgentHost::GetOrCreateFor(node);
}

// static
CitizenNotesAgentHostImpl* RenderFrameCitizenNotesAgentHost::GetFor(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = CNGetFrameTreeNodeAncestor(frame_tree_node);
  return FindAgentHost(frame_tree_node);
}

// static
CitizenNotesAgentHostImpl* RenderFrameCitizenNotesAgentHost::GetFor(
    RenderFrameHostImpl* rfh) {
  return ShouldCreateCitizenNotesForHost(rfh)
             ? FindAgentHost(rfh->frame_tree_node())
             : GetFor(rfh->frame_tree_node());
}

scoped_refptr<CitizenNotesAgentHost> RenderFrameCitizenNotesAgentHost::GetOrCreateFor(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = CNGetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameCitizenNotesAgentHost* result = FindAgentHost(frame_tree_node);
  if (!result) {
    result = new RenderFrameCitizenNotesAgentHost(
        frame_tree_node, frame_tree_node->current_frame_host());
  }
  return result;
}

// static
bool RenderFrameCitizenNotesAgentHost::ShouldCreateCitizenNotesForHost(
    RenderFrameHostImpl* rfh) {
  DCHECK(rfh);
  return rfh->is_local_root();
}

// static
scoped_refptr<RenderFrameCitizenNotesAgentHost>
RenderFrameCitizenNotesAgentHost::CreateForLocalRootOrEmbeddedPageNavigation(
    NavigationRequest* request) {
  // Note that this method does not use FrameTreeNode::current_frame_host(),
  // since it is used while the frame host may not be set as current yet,
  // for example right before commit time. Instead target frame from the
  // navigation handle is used. When this method is invoked it's already known
  // that the navigation will commit to the new frame host.
  FrameTreeNode* frame_tree_node = request->frame_tree_node();
  DCHECK(!FindAgentHost(frame_tree_node));
  return new RenderFrameCitizenNotesAgentHost(frame_tree_node,
                                          request->GetRenderFrameHost());
}

// static
scoped_refptr<RenderFrameCitizenNotesAgentHost>
RenderFrameCitizenNotesAgentHost::FindForDangling(FrameTreeNode* frame_tree_node) {
  return FindAgentHost(frame_tree_node);
}

// static
bool CitizenNotesAgentHost::HasFor(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetPrimaryFrameTree().root();
  return node ? FindAgentHost(node) != nullptr : false;
}

// static
bool RenderFrameCitizenNotesAgentHost::WasEverAttachedToAnyFrame() {
  return g_was_ever_attached_to_any_frame;
}

// static
bool CitizenNotesAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  return RenderFrameCitizenNotesAgentHost::IsDebuggerAttached(web_contents) ||
         WebContentsCitizenNotesAgentHost::IsDebuggerAttached(web_contents);
}

// static
bool RenderFrameCitizenNotesAgentHost::IsDebuggerAttached(
    WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetPrimaryFrameTree().root();
  RenderFrameCitizenNotesAgentHost* host = node ? FindAgentHost(node) : nullptr;
  return host && host->IsAttached();
}

// static
void RenderFrameCitizenNotesAgentHost::AddAllAgentHosts(
    CitizenNotesAgentHost::List* result) {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    // Inner web contents such as guestviews are already handled by
    // ForEachRenderFrameHost.
    if (wc->GetOutermostWebContents() != wc)
      continue;
    wc->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [result](RenderFrameHostImpl* render_frame_host) {
          FrameTreeNode* node = FrameTreeNode::From(render_frame_host);
          if (!ShouldCreateCitizenNotesForNode(node))
            return;
          result->push_back(RenderFrameCitizenNotesAgentHost::GetOrCreateFor(node));
        });
  }
}

// static
void RenderFrameCitizenNotesAgentHost::AttachToWebContents(
    WebContents* web_contents) {
  if (ShouldForceCreation()) {
    // Force agent hosts.
    CitizenNotesAgentHost::GetOrCreateForTab(web_contents);
    CitizenNotesAgentHost::GetOrCreateFor(web_contents);
  }
}

// static
void RenderFrameCitizenNotesAgentHost::UpdateRawHeadersAccess(
    RenderFrameHostImpl* rfh) {
  if (!rfh) {
    return;
  }
  RenderProcessHost* rph = rfh->GetProcess();
  std::set<url::Origin> process_origins;
  for (const auto& entry : g_agent_host_instances.Get()) {
    RenderFrameHostImpl* frame_host = entry.second->frame_host_;
    if (!frame_host)
      continue;
    // Do not skip the nodes if they're about to get attached.
    if (!entry.second->IsAttached() && entry.first != rfh->frame_tree_node()) {
      continue;
    }
    RenderProcessHost* process_host = frame_host->GetProcess();
    if (process_host == rph)
      process_origins.insert(frame_host->GetLastCommittedOrigin());
  }
  GetNetworkService()->SetRawHeadersAccess(
      rph->GetID(),
      std::vector<url::Origin>(process_origins.begin(), process_origins.end()));
}

RenderFrameCitizenNotesAgentHost::RenderFrameCitizenNotesAgentHost(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostImpl* frame_host)
    : CitizenNotesAgentHostImpl(frame_host->citizennotes_frame_token().ToString()),
      auto_attacher_(std::make_unique<CNFrameAutoAttacher>(GetRendererChannel())),
      frame_tree_node_(nullptr) {
  g_was_ever_attached_to_any_frame = true;
  AddRef();  // Balanced in DestroyOnRenderFrameGone.
  auto* wc = WebContentsImpl::FromRenderFrameHostImpl(frame_host);
  WebContentsObserver::Observe(wc);
  SetFrameTreeNode(frame_tree_node);
  ChangeFrameHostAndObservedProcess(frame_host);
  render_frame_alive_ = frame_host_ && frame_host_->IsRenderFrameLive();
  if (frame_tree_node->GetFrameType() != FrameType::kPrimaryMainFrame &&
      frame_tree_node->GetFrameType() != FrameType::kPrerenderMainFrame) {
    render_frame_crashed_ = !render_frame_alive_;
  } else {
    WebContents* web_contents = WebContents::FromRenderFrameHost(frame_host);
    render_frame_crashed_ = web_contents && web_contents->IsCrashed();
  }
  NotifyCreated();
}

void RenderFrameCitizenNotesAgentHost::SetFrameTreeNode(
    FrameTreeNode* frame_tree_node) {
  if (frame_tree_node_)
    g_agent_host_instances.Get().erase(frame_tree_node_);
  frame_tree_node_ = frame_tree_node;
  if (frame_tree_node_) {
    DCHECK(web_contents() ==
           WebContentsImpl::FromFrameTreeNode(frame_tree_node));
    // TODO(dgozman): with ConnectWebContents/DisconnectWebContents,
    // we may get two agent hosts for the same FrameTreeNode.
    // That is definitely a bug, and we should fix that, and DCHECK
    // here that there is no other agent host.
    g_agent_host_instances.Get()[frame_tree_node] = this;
  }
}

BrowserContext* RenderFrameCitizenNotesAgentHost::GetBrowserContext() {
  WebContents* contents = web_contents();
  return contents ? contents->GetBrowserContext() : nullptr;
}

WebContents* RenderFrameCitizenNotesAgentHost::GetWebContents() {
  return web_contents();
}

bool RenderFrameCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                                 bool acquire_wake_lock) {
  if (!ShouldAllowSession(frame_host_, session)) {
    return false;
  }

  if (frame_tree_node_ && !frame_tree_node_->parent() &&
      frame_tree_node_->is_on_initial_empty_document()) {
    // Since the CitizenNotes protocol can be used to modify the initial empty
    // document of a tab, notify the browser that the pending URL shouldn't be
    // displayed anymore to eliminate a URL spoof risk.
    frame_host_->DidAccessInitialMainDocument();
  }

  if (!navigation_requests_.empty()) {
    session->SuspendSendingMessagesToAgent();
  }

  session->CreateAndAddHandler<protocol::CNAuditsHandler>();
  session->CreateAndAddHandler<protocol::CNBackgroundServiceHandler>();
  auto* browser_handler =
      session->CreateAndAddHandler<protocol::CNBrowserHandler>(
          session->GetClient()->MayWriteLocalFiles());
  session->CreateAndAddHandler<protocol::CNDeviceAccessHandler>();
  session->CreateAndAddHandler<protocol::CNDOMHandler>(
      session->GetClient()->MayReadLocalFiles());
  auto* emulation_handler =
      session->CreateAndAddHandler<protocol::CNEmulationHandler>();
  session->CreateAndAddHandler<protocol::CNInputHandler>(
      session->GetClient()->MayReadLocalFiles(),
      session->GetClient()->IsTrusted());
  session->CreateAndAddHandler<protocol::CNInspectorHandler>();
  session->CreateAndAddHandler<protocol::CNIOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::CNMemoryHandler>();
#if BUILDFLAG(USE_VIZ_DEBUGGER)
  session->CreateAndAddHandler<protocol::CNVisualDebuggerHandler>();
#endif
  if (!frame_tree_node_ || !frame_tree_node_->parent())
    session->CreateAndAddHandler<protocol::CNOverlayHandler>();
  session->CreateAndAddHandler<protocol::CNNetworkHandler>(
      GetId(),
      frame_host_ ? frame_host_->citizennotes_frame_token()
                  : base::UnguessableToken(),
      GetIOContext(),
      base::BindRepeating(
          &RenderFrameCitizenNotesAgentHost::UpdateResourceLoaderFactories,
          base::Unretained(this)),
      session->GetClient()->MayReadLocalFiles(),
      session->GetClient()->IsTrusted());
  session->CreateAndAddHandler<protocol::CNFetchHandler>(
      GetIOContext(), base::BindRepeating(
                          [](RenderFrameCitizenNotesAgentHost* self,
                             base::OnceClosure done_callback) {
                            self->UpdateResourceLoaderFactories();
                            std::move(done_callback).Run();
                          },
                          base::Unretained(this)));
  session->CreateAndAddHandler<protocol::CNSchemaHandler>();
  const bool may_attach_to_brower = session->GetClient()->IsTrusted();
  session->CreateAndAddHandler<protocol::CNServiceWorkerHandler>(
      /* allow_inspect_worker= */ may_attach_to_brower);
  session->CreateAndAddHandler<protocol::CNStorageHandler>(
      session->GetClient()->IsTrusted());
  session->CreateAndAddHandler<protocol::CNSystemInfoHandler>(
      /* is_browser_session= */ false);
  auto* target_handler = session->CreateAndAddHandler<protocol::CNTargetHandler>(
      may_attach_to_brower
          ? protocol::CNTargetHandler::AccessMode::kRegular
          : protocol::CNTargetHandler::AccessMode::kAutoAttachOnly,
      GetId(), auto_attacher_.get(), session);
  if (session->session_mode() !=
      CitizenNotesSession::Mode::kDoesNotSupportTabTarget) {
    target_handler->DisableAutoAttachOfPortals();
  }
  session->CreateAndAddHandler<protocol::CNPreloadHandler>();
  session->CreateAndAddHandler<protocol::CNPageHandler>(
      emulation_handler, browser_handler,
      session->GetClient()->AllowUnsafeOperations(),
      session->GetClient()->IsTrusted(),
      session->GetClient()->GetNavigationInitiatorOrigin(),
      session->GetClient()->MayReadLocalFiles());
  session->CreateAndAddHandler<protocol::CNSecurityHandler>();
  if (!frame_tree_node_ || !frame_tree_node_->parent()) {
    CitizenNotesSession* root_session = session->GetRootSession();
    CHECK(root_session);
    session->CreateAndAddHandler<protocol::CNTracingHandler>(this, GetIOContext(),
                                                           root_session);
  }
  session->CreateAndAddHandler<protocol::CNLogHandler>();
  session->CreateAndAddHandler<protocol::CNFedCmHandler>();
#if !BUILDFLAG(IS_ANDROID)
  session->CreateAndAddHandler<protocol::CNWebAuthnHandler>();
#endif  // !BUILDFLAG(IS_ANDROID)

  if (sessions().empty()) {
    UpdateRawHeadersAccess(frame_host_);
#if BUILDFLAG(IS_ANDROID)
    if (acquire_wake_lock)
      GetWakeLock()->RequestWakeLock();
#endif
  }
  return true;
}

void RenderFrameCitizenNotesAgentHost::DetachSession(CitizenNotesSession* session) {
  // Destroying session automatically detaches in renderer.
  if (sessions().empty()) {
    UpdateRawHeadersAccess(frame_host_);
#if BUILDFLAG(IS_ANDROID)
    GetWakeLock()->CancelWakeLock();
#endif
  }
}

void RenderFrameCitizenNotesAgentHost::InspectElement(RenderFrameHost* frame_host,
                                                  int x,
                                                  int y) {
  FrameTreeNode* ftn =
      static_cast<RenderFrameHostImpl*>(frame_host)->frame_tree_node();
  RenderFrameCitizenNotesAgentHost* host =
      static_cast<RenderFrameCitizenNotesAgentHost*>(GetOrCreateFor(ftn).get());

  gfx::Point point(x, y);
  // The renderer expects coordinates relative to the local frame root,
  // so we need to transform the coordinates from the root space
  // to the local frame root widget's space.
  if (host->frame_host_) {
    if (RenderWidgetHostView* view = host->frame_host_->GetView()) {
      point = gfx::ToRoundedPoint(
          view->TransformRootPointToViewCoordSpace(gfx::PointF(point)));
    }
  }
  host->GetRendererChannel()->InspectElement(point);
}

void RenderFrameCitizenNotesAgentHost::GetUniqueFormControlId(
    int node_id,
    GetUniqueFormControlIdCallback callback) {
  GetRendererChannel()->GetUniqueFormControlId(node_id, std::move(callback));
}

RenderFrameCitizenNotesAgentHost::~RenderFrameCitizenNotesAgentHost() {
  SetFrameTreeNode(nullptr);
  ChangeFrameHostAndObservedProcess(nullptr);
}

void RenderFrameCitizenNotesAgentHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (!frame_tree_node_) {
    return;
  }
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  for (auto* tracing : protocol::CNTracingHandler::ForAgentHost(this))
    tracing->ReadyToCommitNavigation(request);

  if (request->frame_tree_node() != frame_tree_node_) {
    if (ShouldForceCreation() && request->GetRenderFrameHost() &&
        request->GetRenderFrameHost()->is_local_root_subframe()) {
      // An agent may have been created earlier if auto attach is on.
      if (!FindAgentHost(request->frame_tree_node()))
        CreateForLocalRootOrEmbeddedPageNavigation(request);
    }
    return;
  }

  // Child workers will eventually disconnect, but timing depends on the
  // renderer process. To ensure consistent view over protocol, disconnect them
  // right now.
  GetRendererChannel()->ForceDetachWorkerSessions();
  UpdateFrameHost(request->GetRenderFrameHost());
  // UpdateFrameHost may destruct |this|.
}

void RenderFrameCitizenNotesAgentHost::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  // If we opt for retaning self within the conditional block below, do so
  // till the end of the function, as we require |this| after the conditional.
  scoped_refptr<RenderFrameCitizenNotesAgentHost> protect;
  if (request->frame_tree_node() == frame_tree_node_) {
    navigation_requests_.erase(request);
    if (request->HasCommitted())
      NotifyNavigated();

    if (IsAttached()) {
      UpdateRawHeadersAccess(frame_tree_node_->current_frame_host());
    }
    // UpdateFrameHost may destruct |this|.
    protect = this;
    UpdateFrameHost(frame_tree_node_->current_frame_host());

    if (navigation_requests_.empty()) {
      for (CitizenNotesSession* session : sessions())
        session->ResumeSendingMessagesToAgent();
    }
  }
  auto_attacher_->DidFinishNavigation(
      NavigationRequest::From(navigation_handle));
}

void RenderFrameCitizenNotesAgentHost::UpdateFrameHost(
    RenderFrameHostImpl* frame_host) {
  if (frame_host == frame_host_) {
    if (frame_host && !render_frame_alive_)
      UpdateFrameAlive();
    return;
  }

  if (frame_host && !ShouldCreateCitizenNotesForHost(frame_host)) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  RenderFrameHostImpl* old_host = frame_host_;
  ChangeFrameHostAndObservedProcess(frame_host);
  if (IsAttached())
    UpdateRawHeadersAccess(old_host);

  std::vector<CitizenNotesSession*> restricted_sessions;
  for (CitizenNotesSession* session : sessions()) {
    if (!ShouldAllowSession(frame_host_, session)) {
      restricted_sessions.push_back(session);
    }
  }
  scoped_refptr<RenderFrameCitizenNotesAgentHost> protect;
  if (!restricted_sessions.empty()) {
    protect = this;
    ForceDetachRestrictedSessions(restricted_sessions);
  }

  UpdateFrameAlive();
}

void RenderFrameCitizenNotesAgentHost::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  if (request->frame_tree_node() != frame_tree_node_)
    return;
  if (navigation_requests_.empty()) {
    for (CitizenNotesSession* session : sessions())
      session->SuspendSendingMessagesToAgent();
  }
  navigation_requests_.insert(request);
}

void RenderFrameCitizenNotesAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  auto* new_host_impl = static_cast<RenderFrameHostImpl*>(new_host);
  FrameTreeNode* frame_tree_node = new_host_impl->frame_tree_node();
  if (frame_tree_node != frame_tree_node_)
    return;
  UpdateFrameHost(new_host_impl);
  // UpdateFrameHost may destruct |this|.
}

void RenderFrameCitizenNotesAgentHost::FrameDeleted(int frame_tree_node_id) {
  for (auto* tracing : protocol::CNTracingHandler::ForAgentHost(this))
    tracing->FrameDeleted(frame_tree_node_id);
  if (frame_tree_node_ &&
      frame_tree_node_id == frame_tree_node_->frame_tree_node_id()) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
  }
}

void RenderFrameCitizenNotesAgentHost::RenderFrameDeleted(RenderFrameHost* rfh) {
  if (rfh == frame_host_) {
    if (frame_tree_node_) {
      render_frame_alive_ = false;
      UpdateRendererChannel(IsAttached());
    } else {
      // We're already detached from FTN, so this must be a cached
      // instance going away.
      DestroyOnRenderFrameGone();
    }
  }
}

void RenderFrameCitizenNotesAgentHost::DestroyOnRenderFrameGone() {
  scoped_refptr<CitizenNotesAgentHost> retain_this;
  if (IsAttached()) {
    retain_this = ForceDetachAllSessionsImpl();
    UpdateRawHeadersAccess(frame_host_);
  }
  WebContentsObserver::Observe(nullptr);
  ChangeFrameHostAndObservedProcess(nullptr);
  UpdateRendererChannel(IsAttached());
  SetFrameTreeNode(nullptr);
  Release();
}

#if BUILDFLAG(IS_ANDROID)
device::mojom::WakeLock* RenderFrameCitizenNotesAgentHost::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!wake_lock_) {
    mojo::PendingReceiver<device::mojom::WakeLock> receiver =
        wake_lock_.BindNewPipeAndPassReceiver();
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::kPreventDisplaySleep,
          device::mojom::WakeLockReason::kOther, "CitizenNotes",
          std::move(receiver));
    }
  }
  return wake_lock_.get();
}
#endif

void RenderFrameCitizenNotesAgentHost::ChangeFrameHostAndObservedProcess(
    RenderFrameHostImpl* frame_host) {
  if (frame_host_)
    frame_host_->GetProcess()->RemoveObserver(this);
  frame_host_ = frame_host;
  if (frame_host_) {
    DCHECK(WebContentsImpl::FromRenderFrameHostImpl(frame_host_) ==
           web_contents());
    frame_host_->GetProcess()->AddObserver(this);
    ProcessHostChanged();
  }
}

void RenderFrameCitizenNotesAgentHost::UpdateFrameAlive() {
  render_frame_alive_ = frame_host_ && frame_host_->IsRenderFrameLive();
  if (render_frame_alive_ && render_frame_crashed_) {
    render_frame_crashed_ = false;
    for (CitizenNotesSession* session : sessions())
      session->ClearPendingMessages(/*did_crash=*/true);
    for (auto* inspector : protocol::CNInspectorHandler::ForAgentHost(this))
      inspector->TargetReloadedAfterCrash();
  }
  UpdateRendererChannel(IsAttached());
}

void RenderFrameCitizenNotesAgentHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  switch (info.status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      for (auto* inspector : protocol::CNInspectorHandler::ForAgentHost(this))
        inspector->TargetCrashed();
      NotifyCrashed(info.status);
      render_frame_crashed_ = true;
      break;
    default:
      for (auto* inspector : protocol::CNInspectorHandler::ForAgentHost(this))
        inspector->TargetDetached("Render process gone.");
      break;
  }
}

void RenderFrameCitizenNotesAgentHost::OnVisibilityChanged(
    content::Visibility visibility) {
#if BUILDFLAG(IS_ANDROID)
  if (!sessions().empty()) {
    if (visibility == content::Visibility::HIDDEN) {
      GetWakeLock()->CancelWakeLock();
    } else {
      GetWakeLock()->RequestWakeLock();
    }
  }
#endif
}

void RenderFrameCitizenNotesAgentHost::OnNavigationRequestWillBeSent(
    const NavigationRequest& navigation_request) {
  GURL url = navigation_request.common_params().url;
  if (url.SchemeIs(url::kJavaScriptScheme) && frame_host_)
    url = frame_host_->GetLastCommittedURL();
  std::vector<CitizenNotesSession*> restricted_sessions;
  bool is_webui = frame_host_ && frame_host_->web_ui();
  for (CitizenNotesSession* session : sessions()) {
    if (!session->GetClient()->MayAttachToURL(url, is_webui))
      restricted_sessions.push_back(session);
  }
  if (!restricted_sessions.empty())
    ForceDetachRestrictedSessions(restricted_sessions);
}

void RenderFrameCitizenNotesAgentHost::UpdatePortals() {
  auto_attacher_->UpdatePages();
}

void RenderFrameCitizenNotesAgentHost::DidCreateFencedFrame(
    FencedFrame* fenced_frame) {
  auto_attacher_->AutoAttachToPage(fenced_frame->GetInnerRoot()->frame_tree(),
                                   true);
}

void RenderFrameCitizenNotesAgentHost::DisconnectWebContents() {
  WebContentsObserver::Observe(nullptr);
  navigation_requests_.clear();
  SetFrameTreeNode(nullptr);
  // UpdateFrameHost may destruct |this|.
  scoped_refptr<RenderFrameCitizenNotesAgentHost> protect(this);
  UpdateFrameHost(nullptr);
  for (CitizenNotesSession* session : sessions())
    session->ResumeSendingMessagesToAgent();
}

void RenderFrameCitizenNotesAgentHost::ConnectWebContents(WebContents* wc) {
  RenderFrameHostImpl* host =
      static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame());
  DCHECK(host);
  WebContentsObserver::Observe(wc);
  SetFrameTreeNode(host->frame_tree_node());
  UpdateFrameHost(host);
  // UpdateFrameHost may destruct |this|.
}

std::string RenderFrameCitizenNotesAgentHost::GetParentId() {
  if (IsChildFrame()) {
    FrameTreeNode* frame_tree_node =
        CNGetFrameTreeNodeAncestor(frame_tree_node_->parent()->frame_tree_node());
    return RenderFrameCitizenNotesAgentHost::GetOrCreateFor(frame_tree_node)
        ->GetId();
  }

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  if (!contents)
    return "";
  contents = contents->GetOuterWebContents();
  if (contents)
    return CitizenNotesAgentHost::GetOrCreateFor(contents)->GetId();
  return "";
}

std::string RenderFrameCitizenNotesAgentHost::GetOpenerId() {
  if (!frame_tree_node_)
    return std::string();
  FrameTreeNode* opener =
      frame_tree_node_->first_live_main_frame_in_original_opener_chain();
  return opener
             ? opener->current_frame_host()->citizennotes_frame_token().ToString()
             : std::string();
}

std::string RenderFrameCitizenNotesAgentHost::GetOpenerFrameId() {
  if (!frame_tree_node_)
    return std::string();
  const absl::optional<base::UnguessableToken>& opener_citizennotes_frame_token =
      frame_tree_node_->opener_citizennotes_frame_token();
  return opener_citizennotes_frame_token ? opener_citizennotes_frame_token->ToString()
                                     : std::string();
}

bool RenderFrameCitizenNotesAgentHost::CanAccessOpener() {
  return (frame_tree_node_ && frame_tree_node_->opener());
}

std::string RenderFrameCitizenNotesAgentHost::GetType() {
  if (IsChildFrame())
    return kTypeFrame;
  if (frame_tree_node_ && frame_tree_node_->IsFencedFrameRoot())
    return kTypeFrame;
  if (web_contents() &&
      static_cast<WebContentsImpl*>(web_contents())->GetOuterWebContents()) {
    return kTypeGuest;
  }
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate() && web_contents()) {
    std::string type = manager->delegate()->GetTargetType(web_contents());
    if (!type.empty())
      return type;
  }
  return kTypePage;
}

std::string RenderFrameCitizenNotesAgentHost::GetTitle() {
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate() && web_contents()) {
    std::string title = manager->delegate()->GetTargetTitle(web_contents());
    if (!title.empty())
      return title;
  }
  if (frame_host_) {
    if (IsChildFrame())
      return frame_host_->GetLastCommittedURL().spec();
    if (!frame_host_->GetPage().IsPrimary()) {
      NavigationEntryImpl* entry = frame_host_->frame_tree_node()
                                       ->frame_tree()
                                       .controller()
                                       .GetLastCommittedEntry();
      return entry ? base::UTF16ToUTF8(entry->GetTitleForDisplay())
                   : std::string();
    }
  }
  if (web_contents())
    return base::UTF16ToUTF8(web_contents()->GetTitle());
  return GetURL().spec();
}

std::string RenderFrameCitizenNotesAgentHost::GetDescription() {
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate() && web_contents())
    return manager->delegate()->GetTargetDescription(web_contents());
  return std::string();
}

GURL RenderFrameCitizenNotesAgentHost::GetURL() {
  // Order is important here.
  if (frame_host_ && (IsChildFrame() || !frame_host_->GetPage().IsPrimary()))
    return frame_host_->GetLastCommittedURL();
  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    return web_contents->GetVisibleURL();
  }
  return GURL();
}

GURL RenderFrameCitizenNotesAgentHost::GetFaviconURL() {
  WebContents* wc = web_contents();
  if (!wc)
    return GURL();
  NavigationEntry* entry = wc->GetController().GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().url;
  return GURL();
}

bool RenderFrameCitizenNotesAgentHost::Activate() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc) {
    wc->Activate();
    return true;
  }
  return false;
}

void RenderFrameCitizenNotesAgentHost::Reload() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc)
    wc->GetController().Reload(ReloadType::NORMAL, true);
}

bool RenderFrameCitizenNotesAgentHost::Close() {
  if (web_contents()) {
    web_contents()->ClosePage();
    return true;
  }
  return false;
}

base::TimeTicks RenderFrameCitizenNotesAgentHost::GetLastActivityTime() {
  if (WebContents* contents = web_contents())
    return contents->GetLastActiveTime();
  return base::TimeTicks();
}

void RenderFrameCitizenNotesAgentHost::UpdateRendererChannel(bool force) {
  is_debugger_paused_ = false;
  is_debugger_pause_situation_recorded_ = false;

  mojo::PendingAssociatedRemote<blink::mojom::CitizenNotesAgent> agent_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::CitizenNotesAgentHost>
      host_receiver;
  if (frame_host_ && render_frame_alive_ && force) {
    mojo::PendingAssociatedRemote<blink::mojom::CitizenNotesAgentHost> host_remote;
    host_receiver = host_remote.InitWithNewEndpointAndPassReceiver();
    frame_host_->BindCitizenNotesAgent(
        std::move(host_remote),
        agent_remote.InitWithNewEndpointAndPassReceiver());
  }
  int process_id = frame_host_ ? frame_host_->GetProcess()->GetID()
                               : ChildProcessHost::kInvalidUniqueID;
  GetRendererChannel()->SetRendererAssociated(std::move(agent_remote),
                                              std::move(host_receiver),
                                              process_id, frame_host_);
  auto_attacher_->SetRenderFrameHost(frame_host_);
}

protocol::CNTargetAutoAttacher* RenderFrameCitizenNotesAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

namespace {

constexpr char kSubtypeDisconnected[] = "disconnected";
constexpr char kSubtypePrerender[] = "prerender";
constexpr char kSubtypeFenced[] = "fenced";

}  // namespace

std::string RenderFrameCitizenNotesAgentHost::GetSubtype() {
  if (!frame_tree_node_)
    return kSubtypeDisconnected;

  switch (frame_tree_node_->GetFrameType()) {
    case FrameType::kPrimaryMainFrame:
    // TODO(caseq): figure out what's best to return for subframes in a tree
    // other than primary.
    case FrameType::kSubframe:
      return "";
    case FrameType::kPrerenderMainFrame:
      return kSubtypePrerender;
    case FrameType::kFencedFrameRoot:
      return kSubtypeFenced;
  }
}

RenderProcessHost* RenderFrameCitizenNotesAgentHost::GetProcessHost() {
  return frame_host_ ? frame_host_->GetProcess() : nullptr;
}

void RenderFrameCitizenNotesAgentHost::MainThreadDebuggerPaused() {
  is_debugger_paused_ = true;

  if (!GetWebContents() || is_debugger_pause_situation_recorded_) {
    return;
  }

  if (GetWebContents() != GetWebContents()->GetOutermostWebContents()) {
    return;
  }

  url::Origin our_origin =
      url::Origin::Create(GetWebContents()->GetLastCommittedURL());

  bool is_same_origin_debugger_attached_in_another_renderer = false;
  bool is_same_origin_debugger_paused_in_another_renderer = false;
  for (const auto& entry : g_agent_host_instances.Get()) {
    RenderFrameCitizenNotesAgentHost* agent_host = entry.second;
    if (agent_host == this || !agent_host->GetWebContents()) {
      continue;
    }
    if (agent_host->GetWebContents() !=
        agent_host->GetWebContents()->GetOutermostWebContents()) {
      continue;
    }
    if (!our_origin.IsSameOriginWith(
            agent_host->GetWebContents()->GetLastCommittedURL())) {
      continue;
    }

    if (agent_host->IsAttached()) {
      is_same_origin_debugger_attached_in_another_renderer = true;
    }
    if (agent_host->is_debugger_paused_) {
      is_same_origin_debugger_paused_in_another_renderer = true;
    }
  }

  base::UmaHistogramBoolean(
      "CitizenNotes.IsSameOriginDebuggerAttachedInAnotherRenderer",
      is_same_origin_debugger_attached_in_another_renderer);
  base::UmaHistogramBoolean(
      "CitizenNotes.IsSameOriginDebuggerPausedInAnotherRenderer",
      is_same_origin_debugger_paused_in_another_renderer);

  is_debugger_pause_situation_recorded_ = true;
}

void RenderFrameCitizenNotesAgentHost::MainThreadDebuggerResumed() {
  is_debugger_paused_ = false;
}

bool RenderFrameCitizenNotesAgentHost::IsChildFrame() {
  return frame_tree_node_ && frame_tree_node_->parent();
}

bool RenderFrameCitizenNotesAgentHost::ShouldAllowSession(
    RenderFrameHost* frame_host,
    CitizenNotesSession* session) {
  // There's not much we can say if there's not host yet, but we'll
  // check again when host is updated.
  if (!frame_host) {
    return true;
  }
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate() &&
      !manager->delegate()->AllowInspectingRenderFrameHost(frame_host)) {
    return false;
  }
  return session->GetClient()->MayAttachToRenderFrameHost(frame_host);
}

void RenderFrameCitizenNotesAgentHost::UpdateResourceLoaderFactories() {
  if (!frame_host_)
    return;

  frame_host_->ForEachRenderFrameHostWithAction([this](
                                                    RenderFrameHostImpl* rfh) {
    if (frame_host_ != rfh && (rfh->is_local_root_subframe() ||
                               &frame_host_->GetPage() != &rfh->GetPage())) {
      return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
    }
    rfh->UpdateSubresourceLoaderFactories();
    return content::RenderFrameHost::FrameIterationAction::kContinue;
  });
}

absl::optional<network::CrossOriginEmbedderPolicy>
RenderFrameCitizenNotesAgentHost::cross_origin_embedder_policy(
    const std::string& id) {
  FrameTreeNode* frame_tree_node =
      protocol::FrameTreeNodeFromCitizenNotesFrameToken(
          frame_host_->frame_tree_node(), id);
  if (!frame_tree_node) {
    return absl::nullopt;
  }
  RenderFrameHostImpl* rfhi = frame_tree_node->current_frame_host();
  return rfhi->cross_origin_embedder_policy();
}

absl::optional<network::CrossOriginOpenerPolicy>
RenderFrameCitizenNotesAgentHost::cross_origin_opener_policy(
    const std::string& id) {
  FrameTreeNode* frame_tree_node =
      protocol::FrameTreeNodeFromCitizenNotesFrameToken(
          frame_host_->frame_tree_node(), id);
  if (!frame_tree_node) {
    return absl::nullopt;
  }
  RenderFrameHostImpl* rfhi = frame_tree_node->current_frame_host();
  return rfhi->cross_origin_opener_policy();
}

absl::optional<std::vector<network::mojom::ContentSecurityPolicyHeader>>
RenderFrameCitizenNotesAgentHost::content_security_policy(const std::string& id) {
  FrameTreeNode* frame_tree_node =
      protocol::FrameTreeNodeFromCitizenNotesFrameToken(
          frame_host_->frame_tree_node(), id);
  if (!frame_tree_node) {
    return absl::nullopt;
  }
  RenderFrameHostImpl* rfhi = frame_tree_node->current_frame_host();
  const PolicyContainerPolicies& policies =
      rfhi->policy_container_host()->policies();
  if (policies.content_security_policies.empty()) {
    return absl::nullopt;
  } else {
    std::vector<network::mojom::ContentSecurityPolicyHeader> csp_headers;
    for (const auto& content_security_policy :
         policies.content_security_policies) {
      csp_headers.push_back(*content_security_policy->header);
    }
    return csp_headers;
  }
}

bool RenderFrameCitizenNotesAgentHost::HasSessionsWithoutTabTargetSupport() const {
  const std::vector<CitizenNotesSession*>& sessions =
      CitizenNotesAgentHostImpl::sessions();

  return std::any_of(sessions.begin(), sessions.end(), [](CitizenNotesSession* s) {
    return s->session_mode() == CitizenNotesSession::Mode::kDoesNotSupportTabTarget;
  });
}

}  // namespace content
