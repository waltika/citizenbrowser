// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_SERVICE_WORKER_CITIZENNOTES_MANAGER_H_
#define CONTENT_BROWSER_CITIZENNOTES_SERVICE_WORKER_CITIZENNOTES_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "content/browser/citizen_x/citizennotes_throttle_handle.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"
#include "url/gurl.h"

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
}

namespace content {

class BrowserContext;
class ServiceWorkerCitizenNotesAgentHost;
class ServiceWorkerContextWrapper;

// Manages ServiceWorkerCitizenNotesAgentHost's. This class lives on UI thread.
class ServiceWorkerCitizenNotesManager {
 public:
  class Observer {
   public:
    virtual void WorkerCreated(ServiceWorkerCitizenNotesAgentHost* host,
                               bool* should_pause_on_start) {}
    virtual void WorkerDestroyed(ServiceWorkerCitizenNotesAgentHost* host) {}

   protected:
    virtual ~Observer() {}
  };

  // Returns the ServiceWorkerCitizenNotesManager singleton.
  static ServiceWorkerCitizenNotesManager* GetInstance();

  ServiceWorkerCitizenNotesManager(const ServiceWorkerCitizenNotesManager&) = delete;
  ServiceWorkerCitizenNotesManager& operator=(const ServiceWorkerCitizenNotesManager&) =
      delete;

  ServiceWorkerCitizenNotesAgentHost* GetCitizenNotesAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);
  ServiceWorkerCitizenNotesAgentHost* GetCitizenNotesAgentHostForNewInstallingWorker(
      const ServiceWorkerContextWrapper* context_wrapper,
      int64_t version_id);

  void AddAllAgentHosts(
      std::vector<scoped_refptr<ServiceWorkerCitizenNotesAgentHost>>* result);
  void AddAllAgentHostsForBrowserContext(
      BrowserContext* browser_context,
      std::vector<scoped_refptr<ServiceWorkerCitizenNotesAgentHost>>* result);

  // This function signals the beginning of a main script fetch for a non
  // installed worker. This is currently only used for PlzServiceWorker.
  void WorkerMainScriptFetchingStarting(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id,
      const GURL& url,
      const GURL& scope,
      const GlobalRenderFrameHostId& requesting_frame_id,
      scoped_refptr<CitizenNotesThrottleHandle> throttle_handle);

  // This function is called when a new worker installation failed to fetch
  // the main script. It cleans up internal state.
  void WorkerMainScriptFetchingFailed(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id);

  // Called when a service worker is starting.
  //
  // `client_security_state` may be nullptr.
  void WorkerStarting(
      int worker_process_id,
      int worker_route_id,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id,
      const GURL& url,
      const GURL& scope,
      bool is_installed_version,
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      base::UnguessableToken* citizennotes_worker_token,
      bool* pause_on_start);
  void WorkerReadyForInspection(
      int worker_process_id,
      int worker_route_id,
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver);

  void WorkerVersionInstalled(int worker_process_id, int worker_route_id);
  // If the worker instance is stopped its worker_process_id and
  // worker_route_id will be invalid. For that case we pass context
  // and version_id as well.
  void WorkerVersionDoomed(
      int worker_process_id,
      int worker_route_id,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int64_t version_id);
  void WorkerStopped(int worker_process_id, int worker_route_id);
  void NavigationPreloadRequestSent(int worker_process_id,
                                    int worker_route_id,
                                    const std::string& request_id,
                                    const network::ResourceRequest& request);
  void NavigationPreloadResponseReceived(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const GURL& url,
      const network::mojom::URLResponseHead& head);
  void NavigationPreloadCompleted(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void set_debug_service_worker_on_start(bool debug_on_start);
  bool debug_service_worker_on_start() const {
    return debug_service_worker_on_start_;
  }
  void AgentHostDestroyed(ServiceWorkerCitizenNotesAgentHost* agent_host);

 private:
  friend class base::NoDestructor<ServiceWorkerCitizenNotesManager>;
  friend class ServiceWorkerCitizenNotesAgentHost;

  using WorkerId = std::pair<int, int>;

  ServiceWorkerCitizenNotesManager();
  ~ServiceWorkerCitizenNotesManager();

  scoped_refptr<ServiceWorkerCitizenNotesAgentHost> TakeStoppedHost(
      const ServiceWorkerContextWrapper* context_wrapper,
      int64_t version_id);
  scoped_refptr<ServiceWorkerCitizenNotesAgentHost> TakeNewInstallingHost(
      const ServiceWorkerContextWrapper* context_wrapper,
      int64_t version_id);

  base::ObserverList<Observer>::Unchecked observer_list_;
  bool debug_service_worker_on_start_;

  // We retain agent hosts as long as the service worker is alive.
  std::map<WorkerId, scoped_refptr<ServiceWorkerCitizenNotesAgentHost>> live_hosts_;

  // We store new installing workers. They can be queried directly when fetching
  // the main script from the browser process and are moved to live workers
  // once the process starts up.
  // Note: This is currently only used for plzServiceWorker.
  base::flat_set<scoped_refptr<ServiceWorkerCitizenNotesAgentHost>>
      new_installing_hosts_;

  // Clients may retain agent host for the terminated shared worker,
  // and we reconnect them when shared worker is restarted.
  base::flat_set<ServiceWorkerCitizenNotesAgentHost*> stopped_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_SERVICE_WORKER_CITIZENNOTES_MANAGER_H_
