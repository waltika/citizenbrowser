// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnservice_worker_handler.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/citizen_x/service_worker_citizennotes_agent_host.h"
#include "content/browser/citizen_x/service_worker_citizennotes_manager.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_manager.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/service_worker/service_worker_context_watcher.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace protocol {

namespace {

const std::string GetVersionRunningStatusString(
    blink::EmbeddedWorkerStatus running_status) {
  switch (running_status) {
    case blink::EmbeddedWorkerStatus::kStopped:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Stopped;
    case blink::EmbeddedWorkerStatus::kStarting:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Starting;
    case blink::EmbeddedWorkerStatus::kRunning:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Running;
    case blink::EmbeddedWorkerStatus::kStopping:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Stopping;
    default:
      NOTREACHED();
  }
  return std::string();
}

const std::string GetVersionStatusString(
    content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::New;
    case content::ServiceWorkerVersion::INSTALLING:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Installing;
    case content::ServiceWorkerVersion::INSTALLED:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Installed;
    case content::ServiceWorkerVersion::ACTIVATING:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Activating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Activated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Redundant;
    default:
      NOTREACHED();
  }
  return std::string();
}

Response CreateDomainNotEnabledErrorResponse() {
  return Response::ServerError("ServiceWorker domain not enabled");
}

Response CreateContextErrorResponse() {
  return Response::ServerError("Could not connect to the context");
}

Response CreateInvalidVersionIdErrorResponse() {
  return Response::InvalidParams("Invalid version ID");
}

void DidFindRegistrationForDispatchSyncEvent(
    scoped_refptr<BackgroundSyncContextImpl> sync_context,
    const std::string& tag,
    bool last_chance,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<content::ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !registration->active_version())
    return;
  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  scoped_refptr<content::ServiceWorkerVersion> version(
      registration->active_version());
  // Keep the registration while dispatching the sync event.
  background_sync_manager->EmulateDispatchSyncEvent(
      tag, std::move(version), last_chance, base::DoNothing());
}

void DidFindRegistrationForDispatchPeriodicSyncEvent(
    scoped_refptr<BackgroundSyncContextImpl> sync_context,
    const std::string& tag,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<content::ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !registration->active_version()) {
    return;
  }

  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  scoped_refptr<content::ServiceWorkerVersion> version(
      registration->active_version());
  // Keep the registration while dispatching the sync event.
  background_sync_manager->EmulateDispatchPeriodicSyncEvent(
      tag, std::move(version), base::DoNothing());
}

}  // namespace

CNServiceWorkerHandler::CNServiceWorkerHandler(bool allow_inspect_worker)
    : CitizenNotesDomainHandler(ServiceWorker::Metainfo::domainName),
      allow_inspect_worker_(allow_inspect_worker),
      enabled_(false),
      browser_context_(nullptr),
      storage_partition_(nullptr) {}

CNServiceWorkerHandler::~CNServiceWorkerHandler() = default;

void CNServiceWorkerHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<ServiceWorker::Frontend>(dispatcher->channel());
  ServiceWorker::Dispatcher::wire(dispatcher, this);
}

void CNServiceWorkerHandler::SetRenderer(int process_host_id,
                                       RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_host_id);
  // Do not call UpdateHosts yet, wait for load to commit.
  if (!process_host) {
    ClearForceUpdate();
    context_ = nullptr;
    return;
  }

  storage_partition_ =
      static_cast<StoragePartitionImpl*>(process_host->GetStoragePartition());
  DCHECK(storage_partition_);
  browser_context_ = process_host->GetBrowserContext();
  context_ = static_cast<ServiceWorkerContextWrapper*>(
      storage_partition_->GetServiceWorkerContext());
}

Response CNServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::Success();
  if (!context_)
    return CreateContextErrorResponse();
  enabled_ = true;

  context_watcher_ = base::MakeRefCounted<ServiceWorkerContextWatcher>(
      context_,
      base::BindRepeating(&CNServiceWorkerHandler::OnWorkerRegistrationUpdated,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&CNServiceWorkerHandler::OnWorkerVersionUpdated,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&CNServiceWorkerHandler::OnErrorReported,
                          weak_factory_.GetWeakPtr()));
  context_watcher_->Start();

  return Response::Success();
}

Response CNServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::Success();
  enabled_ = false;

  ClearForceUpdate();
  DCHECK(context_watcher_);
  context_watcher_->Stop();
  context_watcher_ = nullptr;
  return Response::Success();
}

Response CNServiceWorkerHandler::Unregister(const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  GURL url(scope_url);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
  context_->UnregisterServiceWorker(url, key, base::DoNothing());
  return Response::Success();
}

Response CNServiceWorkerHandler::StartWorker(const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  context_->StartActiveServiceWorker(
      GURL(scope_url),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(scope_url))),
      base::DoNothing());
  return Response::Success();
}

Response CNServiceWorkerHandler::SkipWaiting(const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  context_->SkipWaitingWorker(GURL(scope_url),
                              blink::StorageKey::CreateFirstParty(
                                  url::Origin::Create(GURL(scope_url))));
  return Response::Success();
}

Response CNServiceWorkerHandler::StopWorker(const std::string& version_id) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  if (content::ServiceWorkerVersion* version = context_->GetLiveVersion(id)) {
    version->StopWorker(base::DoNothing());
  }

  return Response::Success();
}

void CNServiceWorkerHandler::StopAllWorkers(
    std::unique_ptr<StopAllWorkersCallback> callback) {
  if (!enabled_) {
    callback->sendFailure(CreateDomainNotEnabledErrorResponse());
    return;
  }
  if (!context_) {
    callback->sendFailure(CreateContextErrorResponse());
    return;
  }
  context_->StopAllServiceWorkers(base::BindOnce(
      &StopAllWorkersCallback::sendSuccess, std::move(callback)));
}

Response CNServiceWorkerHandler::UpdateRegistration(
    const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  context_->UpdateRegistration(GURL(scope_url),
                               blink::StorageKey::CreateFirstParty(
                                   url::Origin::Create(GURL(scope_url))));
  return Response::Success();
}

Response CNServiceWorkerHandler::InspectWorker(const std::string& version_id) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  if (!allow_inspect_worker_)
    return Response::ServerError("Permission denied");
  int64_t id = blink::mojom::kInvalidServiceWorkerVersionId;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  if (content::ServiceWorkerVersion* version = context_->GetLiveVersion(id)) {
    OpenNewCitizenNotesWindow(
        version->embedded_worker()->process_id(),
        version->embedded_worker()->worker_devtools_agent_route_id());
  }

  return Response::Success();
}

Response CNServiceWorkerHandler::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  if (!context_)
    return CreateContextErrorResponse();
  context_->SetForceUpdateOnPageLoad(force_update_on_page_load);
  return Response::Success();
}

Response CNServiceWorkerHandler::DeliverPushMessage(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& data) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!browser_context_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  absl::optional<std::string> payload;
  if (data.size() > 0)
    payload = data;
  browser_context_->DeliverPushMessage(GURL(origin), id,
                                       /* message_id= */ std::string(),
                                       std::move(payload), base::DoNothing());

  return Response::Success();
}

Response CNServiceWorkerHandler::DispatchSyncEvent(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& tag,
    bool last_chance) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!storage_partition_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  scoped_refptr<BackgroundSyncContextImpl> sync_context =
      base::WrapRefCounted(storage_partition_->GetBackgroundSyncContext());

  context_->FindReadyRegistrationForId(
      id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(origin))),
      base::BindOnce(&DidFindRegistrationForDispatchSyncEvent,
                     std::move(sync_context), tag, last_chance));

  return Response::Success();
}

Response CNServiceWorkerHandler::DispatchPeriodicSyncEvent(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& tag) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!storage_partition_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  scoped_refptr<BackgroundSyncContextImpl> sync_context =
      base::WrapRefCounted(storage_partition_->GetBackgroundSyncContext());

  context_->FindReadyRegistrationForId(
      id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(origin))),
      base::BindOnce(&DidFindRegistrationForDispatchPeriodicSyncEvent,
                     std::move(sync_context), tag));

  return Response::Success();
}

void CNServiceWorkerHandler::OpenNewCitizenNotesWindow(int process_id,
                                                 int devtools_agent_route_id) {
  scoped_refptr<CitizenNotesAgentHostImpl> agent_host(
      ServiceWorkerCitizenNotesManager::GetInstance()
          ->GetCitizenNotesAgentHostForWorker(process_id, devtools_agent_route_id));
  if (!agent_host.get())
    return;
  agent_host->Inspect();
}

void CNServiceWorkerHandler::OnWorkerRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  using Registration = ServiceWorker::ServiceWorkerRegistration;
  auto result = std::make_unique<protocol::Array<Registration>>();
  for (const auto& registration : registrations) {
    result->emplace_back(
        Registration::Create()
            .SetRegistrationId(
                base::NumberToString(registration.registration_id))
            .SetScopeURL(registration.scope.spec())
            .SetIsDeleted(registration.delete_flag ==
                          ServiceWorkerRegistrationInfo::IS_DELETED)
            .Build());
  }
  frontend_->WorkerRegistrationUpdated(std::move(result));
}

void CNServiceWorkerHandler::OnWorkerVersionUpdated(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  using Version = ServiceWorker::ServiceWorkerVersion;
  auto result = std::make_unique<protocol::Array<Version>>();
  for (const auto& version : versions) {
    base::flat_set<std::string> client_set;

    for (const auto& client : version.clients) {
      if (absl::holds_alternative<GlobalRenderFrameHostId>(client.second)) {
        WebContents* web_contents = WebContentsImpl::FromRenderFrameHostID(
            absl::get<GlobalRenderFrameHostId>(client.second));
        // There is a possibility that the frame is already deleted
        // because of the thread hopping.
        if (!web_contents)
          continue;
        client_set.insert(
            CitizenNotesAgentHost::GetOrCreateFor(web_contents)->GetId());
      }
    }
    auto clients = std::make_unique<protocol::Array<std::string>>();
    for (std::string& client : client_set)
      clients->emplace_back(std::move(client));

    std::unique_ptr<Version> version_value =
        Version::Create()
            .SetVersionId(base::NumberToString(version.version_id))
            .SetRegistrationId(base::NumberToString(version.registration_id))
            .SetScriptURL(version.script_url.spec())
            .SetRunningStatus(
                GetVersionRunningStatusString(version.running_status))
            .SetStatus(GetVersionStatusString(version.status))
            .SetScriptLastModified(
                version.script_last_modified.InSecondsFSinceUnixEpoch())
            .SetScriptResponseTime(
                version.script_response_time.InSecondsFSinceUnixEpoch())
            .SetControlledClients(std::move(clients))
            .Build();
    scoped_refptr<CitizenNotesAgentHostImpl> host(
        ServiceWorkerCitizenNotesManager::GetInstance()
            ->GetCitizenNotesAgentHostForWorker(
                version.process_id,
                version.devtools_agent_route_id));
    if (host) {
      version_value->SetTargetId(host->GetId());
    }
    if (version.router_rules) {
      version_value->SetRouterRules(*version.router_rules);
    }
    result->emplace_back(std::move(version_value));
  }
  frontend_->WorkerVersionUpdated(std::move(result));
}

void CNServiceWorkerHandler::OnErrorReported(
    int64_t registration_id,
    int64_t version_id,
    const ServiceWorkerContextObserver::ErrorInfo& info) {
  frontend_->WorkerErrorReported(
      ServiceWorker::ServiceWorkerErrorMessage::Create()
          .SetErrorMessage(base::UTF16ToUTF8(info.error_message))
          .SetRegistrationId(base::NumberToString(registration_id))
          .SetVersionId(base::NumberToString(version_id))
          .SetSourceURL(info.source_url.spec())
          .SetLineNumber(info.line_number)
          .SetColumnNumber(info.column_number)
          .Build());
}

void CNServiceWorkerHandler::ClearForceUpdate() {
  if (context_)
    context_->SetForceUpdateOnPageLoad(false);
}

}  // namespace protocol
}  // namespace content
