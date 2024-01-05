// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_TARGET_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_TARGET_HANDLER_H_

#include <map>
#include <set>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/citizen_x/citizennotes_throttle_handle.h"
#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/target.h"
#include "content/browser/citizen_x/protocol/cntarget_auto_attacher.h"
#include "content/public/browser/citizennotes_agent_host_observer.h"
#include "net/proxy_resolution/proxy_config.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

class CitizenNotesAgentHostImpl;
class CitizenNotesSession;
class NavigationHandle;
class NavigationThrottle;

namespace protocol {

class CNTargetHandler : public CitizenNotesDomainHandler,
                      public Target::Backend,
                      public CitizenNotesAgentHostObserver,
                      public CNTargetAutoAttacher::Client {
 public:
  enum class AccessMode {
    // Only setAutoAttach is supported. Any non-related target are not
    // accessible.
    kAutoAttachOnly,
    // Standard mode of operation: both auto-attach and discovery.
    kRegular,
    // This mode also allows advanced method like Target.exposeCitizenNotesProtocol,
    // which should not be exposed on a non-browser-wide connection.
    kBrowser
  };
  CNTargetHandler(AccessMode access_mode,
                const std::string& owner_target_id,
                CNTargetAutoAttacher* auto_attacher,
                CitizenNotesSession* session);

  CNTargetHandler(const CNTargetHandler&) = delete;
  CNTargetHandler& operator=(const CNTargetHandler&) = delete;

  ~CNTargetHandler() override;

  static std::vector<CNTargetHandler*> ForAgentHost(CitizenNotesAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  bool ShouldThrottlePopups() const;

  // This is to support legacy protocol, where an autoattacher on service worker
  // targets would not auto-attach service workers.
  // TODO(caseq): update front-end logic and get rid of this.
  void DisableAutoAttachOfServiceWorkers();
  // Unlike the one above, this one indicates the client has opted in into
  // supporting tab targets, so portals are not reported for frame targets.
  void DisableAutoAttachOfPortals();

  // Domain implementation.
  Response SetDiscoverTargets(
      bool discover,
      Maybe<protocol::Array<protocol::Target::FilterEntry>> filter) override;
  void SetAutoAttach(
      bool auto_attach,
      bool wait_for_debugger_on_start,
      Maybe<bool> flatten,
      Maybe<protocol::Array<protocol::Target::FilterEntry>> filter,
      std::unique_ptr<SetAutoAttachCallback> callback) override;
  void AutoAttachRelated(
      const std::string& targetId,
      bool wait_for_debugger_on_start,
      Maybe<protocol::Array<protocol::Target::FilterEntry>> filter,
      std::unique_ptr<AutoAttachRelatedCallback> callback) override;
  Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<Target::RemoteLocation>>) override;
  Response AttachToTarget(const std::string& target_id,
                          Maybe<bool> flatten,
                          std::string* out_session_id) override;
  Response AttachToBrowserTarget(std::string* out_session_id) override;
  Response DetachFromTarget(Maybe<std::string> session_id,
                            Maybe<std::string> target_id) override;
  Response SendMessageToTarget(const std::string& message,
                               Maybe<std::string> session_id,
                               Maybe<std::string> target_id) override;
  Response GetTargetInfo(
      Maybe<std::string> target_id,
      std::unique_ptr<Target::TargetInfo>* target_info) override;
  Response ActivateTarget(const std::string& target_id) override;
  Response CloseTarget(const std::string& target_id,
                       bool* out_success) override;
  Response ExposeCitizenNotesProtocol(const std::string& target_id,
                                  Maybe<std::string> binding_name) override;
  void CreateBrowserContext(
      Maybe<bool> in_disposeOnDetach,
      Maybe<String> in_proxyServer,
      Maybe<String> in_proxyBypassList,
      Maybe<protocol::Array<String>> in_originsToGrantUniversalNetworkAccess,
      std::unique_ptr<CreateBrowserContextCallback> callback) override;
  void DisposeBrowserContext(
      const std::string& context_id,
      std::unique_ptr<DisposeBrowserContextCallback> callback) override;
  Response GetBrowserContexts(
      std::unique_ptr<protocol::Array<String>>* browser_context_ids) override;
  Response CreateTarget(const std::string& url,
                        Maybe<int> width,
                        Maybe<int> height,
                        Maybe<std::string> context_id,
                        Maybe<bool> enable_begin_frame_control,
                        Maybe<bool> new_window,
                        Maybe<bool> background,
                        Maybe<bool> for_tab,
                        std::string* out_target_id) override;
  Response GetTargets(
      Maybe<protocol::Array<protocol::Target::FilterEntry>> filter,
      std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos)
      override;

  void ApplyNetworkContextParamsOverrides(
      BrowserContext* browser_context,
      network::mojom::NetworkContextParams* network_context_params);

  // Adds a ServiceWorker or DedicatedWorker throttle for an auto attaching
  // session. If none is known for this `agent_host`, is a no-op.
  // TODO(crbug.com/1143100): support SharedWorker.
  void AddWorkerThrottle(CitizenNotesAgentHost* agent_host,
                         scoped_refptr<CitizenNotesThrottleHandle> throttle_handle);

 private:
  class Session;
  class Throttle;
  class RequestThrottle;
  class ResponseThrottle;
  class TargetFilter;

  // CNTargetAutoAttacher::Delegate implementation.
  bool AutoAttach(CNTargetAutoAttacher* source,
                  CitizenNotesAgentHost* host,
                  bool waiting_for_debugger) override;
  void AutoDetach(CNTargetAutoAttacher* source, CitizenNotesAgentHost* host) override;
  void SetAttachedTargetsOfType(
      CNTargetAutoAttacher* source,
      const base::flat_set<scoped_refptr<CitizenNotesAgentHost>>& new_hosts,
      const std::string& type) override;
  std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      CNTargetAutoAttacher* auto_attacher,
      NavigationHandle* navigation_handle) override;
  void TargetInfoChanged(CitizenNotesAgentHost* host) override;
  void AutoAttacherDestroyed(CNTargetAutoAttacher* auto_attacher) override;

  bool ShouldWaitForDebuggerOnStart(
      NavigationRequest* navigation_request) const;

  Response FindSession(Maybe<std::string> session_id,
                       Maybe<std::string> target_id,
                       Session** session);
  void ClearThrottles();
  void SetAutoAttachInternal(bool auto_attach,
                             bool wait_for_debugger_on_start,
                             bool flatten,
                             base::OnceClosure callback);
  void UpdateAgentHostObserver();

  // CitizenNotesAgentHostObserver implementation.
  bool ShouldForceCitizenNotesAgentHostCreation() override;
  void CitizenNotesAgentHostCreated(CitizenNotesAgentHost* agent_host) override;
  void CitizenNotesAgentHostNavigated(CitizenNotesAgentHost* agent_host) override;
  void CitizenNotesAgentHostDestroyed(CitizenNotesAgentHost* agent_host) override;
  void CitizenNotesAgentHostAttached(CitizenNotesAgentHost* agent_host) override;
  void CitizenNotesAgentHostDetached(CitizenNotesAgentHost* agent_host) override;
  void CitizenNotesAgentHostCrashed(CitizenNotesAgentHost* agent_host,
                                base::TerminationStatus status) override;
  bool discover() const { return !!discover_target_filter_; }
  Session* FindWaitingSession(CitizenNotesAgentHost* host);

  const AccessMode access_mode_;
  const std::string owner_target_id_;
  const CitizenNotesSession::Mode session_mode_;
  RAW_PTR_EXCLUSION CitizenNotesSession* const root_session_;
  RAW_PTR_EXCLUSION CNTargetAutoAttacher* const auto_attacher_;
  std::unique_ptr<Target::Frontend> frontend_;

  bool flatten_auto_attach_ = false;
  bool auto_attach_ = false;
  // The below is set iff (auto_attach_ ||
  // !auto_attach_related_targets_.empty())
  std::unique_ptr<TargetFilter> auto_attach_target_filter_;
  bool wait_for_debugger_on_start_ = false;
  std::map<CitizenNotesAgentHost*, Session*> auto_attached_sessions_;
  base::flat_map<CNTargetAutoAttacher*, bool /* wait_for_debugger_on_start */>
      auto_attach_related_targets_;
  bool auto_attach_service_workers_ = true;
  bool auto_attach_portals_ = true;

  std::unique_ptr<TargetFilter> discover_target_filter_;
  bool observing_agent_hosts_ = false;
  std::map<std::string, std::unique_ptr<Session>> attached_sessions_;
  std::set<CitizenNotesAgentHost*> reported_hosts_;
  base::flat_set<std::string> dispose_on_detach_context_ids_;
  base::flat_map<std::string, net::ProxyConfig> contexts_with_overridden_proxy_;
  base::flat_set<Throttle*> throttles_;
  absl::optional<net::ProxyConfig> pending_proxy_config_;
  base::WeakPtrFactory<CNTargetHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_TARGET_HANDLER_H_
