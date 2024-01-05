// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_AGENT_HOST_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/containers/flat_map.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "content/browser/citizen_x/citizennotes_io_context.h"
#include "content/browser/citizen_x/citizennotes_renderer_channel.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/common/content_export.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

class BrowserContext;

namespace protocol {
class CNTargetAutoAttacher;
}  // namespace protocol

// Describes interface for managing citizennotes agents from the browser process.
class CONTENT_EXPORT CitizenNotesAgentHostImpl : public CitizenNotesAgentHost {
 public:
  // Returns CitizenNotesAgentHost with a given |id| or nullptr of it doesn't exist.
  static scoped_refptr<CitizenNotesAgentHostImpl> GetForId(const std::string& id);

  CitizenNotesAgentHostImpl(const CitizenNotesAgentHostImpl&) = delete;
  CitizenNotesAgentHostImpl& operator=(const CitizenNotesAgentHostImpl&) = delete;

  // CitizenNotesAgentHost implementation.
  bool AttachClient(CitizenNotesAgentHostClient* client) override;
  bool AttachClientWithoutWakeLock(CitizenNotesAgentHostClient* client) override;
  bool DetachClient(CitizenNotesAgentHostClient* client) override;
  void DispatchProtocolMessage(CitizenNotesAgentHostClient* client,
                               base::span<const uint8_t> message) override;
  bool IsAttached() override;
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;
  void GetUniqueFormControlId(int node_id,
                              GetUniqueFormControlIdCallback callback) override;
  std::string GetId() override;
  std::string CreateIOStreamFromData(
      scoped_refptr<base::RefCountedMemory> data) override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetOpenerFrameId() override;
  bool CanAccessOpener() override;
  std::string GetDescription() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  base::TimeTicks GetLastActivityTime() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;
  RenderProcessHost* GetProcessHost() override;

  struct NetworkLoaderFactoryParamsAndInfo {
    NetworkLoaderFactoryParamsAndInfo();
    NetworkLoaderFactoryParamsAndInfo(
        url::Origin,
        net::SiteForCookies,
        network::mojom::URLLoaderFactoryParamsPtr);
    NetworkLoaderFactoryParamsAndInfo(NetworkLoaderFactoryParamsAndInfo&&);
    ~NetworkLoaderFactoryParamsAndInfo();
    url::Origin origin;
    net::SiteForCookies site_for_cookies;
    network::mojom::URLLoaderFactoryParamsPtr factory_params;
  };
  // Creates network factory parameters for citizennotes-initiated subresource
  // requests.
  virtual NetworkLoaderFactoryParamsAndInfo
  CreateNetworkFactoryParamsForCitizenNotes();

  virtual CitizenNotesSession::Mode GetSessionMode();

  bool Inspect();

  template <typename Handler>
  std::vector<Handler*> HandlersByName(const std::string& name) {
    std::vector<Handler*> result;
    if (sessions_.empty())
      return result;
    for (CitizenNotesSession* session : sessions_) {
      auto it = session->handlers().find(name);
      if (it != session->handlers().end())
        result.push_back(static_cast<Handler*>(it->second.get()));
    }
    return result;
  }

  virtual absl::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id);
  virtual absl::optional<network::CrossOriginOpenerPolicy>
  cross_origin_opener_policy(const std::string& id);
  virtual absl::optional<
      std::vector<network::mojom::ContentSecurityPolicyHeader>>
  content_security_policy(const std::string& id);

  virtual protocol::CNTargetAutoAttacher* auto_attacher();
  virtual std::string GetSubtype();

  base::ProcessId GetProcessId() const { return process_id_; }

 protected:
  explicit CitizenNotesAgentHostImpl(const std::string& id);
  ~CitizenNotesAgentHostImpl() override;

  static bool ShouldForceCreation();

  // Returning |false| will block the attach.
  virtual bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock);
  virtual void DetachSession(CitizenNotesSession* session);
  virtual void UpdateRendererChannel(bool force);

  void NotifyCreated();
  void NotifyNavigated();
  void NotifyCrashed(base::TerminationStatus status);

  void SetProcessId(base::ProcessId process_id);
  void ProcessHostChanged();

  void ForceDetachRestrictedSessions(
      const std::vector<CitizenNotesSession*>& restricted_sessions);
  CitizenNotesIOContext* GetIOContext() { return &io_context_; }
  CitizenNotesRendererChannel* GetRendererChannel() { return &renderer_channel_; }

  const std::vector<CitizenNotesSession*>& sessions() const { return sessions_; }
  // Returns refptr retaining `this`. All other references may be removed
  // at this point, so `this` will become invalid as soon as returned refptr
  // gets destroyed.
  [[nodiscard]] scoped_refptr<CitizenNotesAgentHost> ForceDetachAllSessionsImpl();

  // Called when the corresponding renderer process notifies that the main
  // thread debugger is paused or resumed.
  // TODO(https://crbug.com/1449114): Remove this method when we collect enough
  // data to understand how likely that situation could happen.
  virtual void MainThreadDebuggerPaused();
  virtual void MainThreadDebuggerResumed();

 private:
  // Note that calling this may result in the instance being deleted,
  // as instance may be owned by client sessions. This should not be
  // used by methods of derived classes, use `ForceDetachAllSessionsImpl()`
  // above instead.
  void ForceDetachAllSessions() override;

  friend class CitizenNotesAgentHost;  // for static methods
  friend class CitizenNotesSession;
  friend class CitizenNotesRendererChannel;

  bool AttachInternal(std::unique_ptr<CitizenNotesSession> session);
  bool AttachInternal(std::unique_ptr<CitizenNotesSession> session,
                      bool acquire_wake_lock);
  void DetachInternal(CitizenNotesSession* session);
  void NotifyAttached();
  void NotifyDetached();
  void NotifyDestroyed();
  CitizenNotesSession* SessionByClient(CitizenNotesAgentHostClient* client);

  const std::string id_;
  std::vector<CitizenNotesSession*> sessions_;
  base::flat_map<CitizenNotesAgentHostClient*, std::unique_ptr<CitizenNotesSession>>
      session_by_client_;
  CitizenNotesIOContext io_context_;
  CitizenNotesRendererChannel renderer_channel_;
  base::ProcessId process_id_ = base::kNullProcessId;

  static int s_force_creation_count_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_AGENT_HOST_IMPL_H_
