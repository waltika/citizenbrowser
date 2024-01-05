// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnbackground_service_handler.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/citizennotes_background_services_context.h"
#include "content/public/browser/render_process_host.h"

namespace content {
namespace protocol {

namespace {

citizennotes::proto::BackgroundService ServiceNameToEnum(
    const std::string& service_name) {
  if (service_name == BackgroundService::ServiceNameEnum::BackgroundFetch) {
    return citizennotes::proto::BackgroundService::BACKGROUND_FETCH;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::BackgroundSync) {
    return citizennotes::proto::BackgroundService::BACKGROUND_SYNC;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::PushMessaging) {
    return citizennotes::proto::BackgroundService::PUSH_MESSAGING;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::Notifications) {
    return citizennotes::proto::BackgroundService::NOTIFICATIONS;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::PaymentHandler) {
    return citizennotes::proto::BackgroundService::PAYMENT_HANDLER;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::PeriodicBackgroundSync) {
    return citizennotes::proto::BackgroundService::PERIODIC_BACKGROUND_SYNC;
  }
  return citizennotes::proto::BackgroundService::UNKNOWN;
}

std::string ServiceEnumToName(citizennotes::proto::BackgroundService service_enum) {
  switch (service_enum) {
    case citizennotes::proto::BackgroundService::BACKGROUND_FETCH:
      return BackgroundService::ServiceNameEnum::BackgroundFetch;
    case citizennotes::proto::BackgroundService::BACKGROUND_SYNC:
      return BackgroundService::ServiceNameEnum::BackgroundSync;
    case citizennotes::proto::BackgroundService::PUSH_MESSAGING:
      return BackgroundService::ServiceNameEnum::PushMessaging;
    case citizennotes::proto::BackgroundService::NOTIFICATIONS:
      return BackgroundService::ServiceNameEnum::Notifications;
    case citizennotes::proto::BackgroundService::PAYMENT_HANDLER:
      return BackgroundService::ServiceNameEnum::PaymentHandler;
    case citizennotes::proto::BackgroundService::PERIODIC_BACKGROUND_SYNC:
      return BackgroundService::ServiceNameEnum::PeriodicBackgroundSync;
    default:
      NOTREACHED();
  }

  return "invalid";
}

std::unique_ptr<protocol::Array<protocol::BackgroundService::EventMetadata>>
ProtoMapToArray(
    const google::protobuf::Map<std::string, std::string>& event_metadata_map) {
  auto metadata_array = std::make_unique<
      protocol::Array<protocol::BackgroundService::EventMetadata>>();

  for (const auto& entry : event_metadata_map) {
    auto event_metadata = protocol::BackgroundService::EventMetadata::Create()
                              .SetKey(entry.first)
                              .SetValue(entry.second)
                              .Build();
    metadata_array->emplace_back(std::move(event_metadata));
  }

  return metadata_array;
}

std::unique_ptr<protocol::BackgroundService::BackgroundServiceEvent>
ToBackgroundServiceEvent(const citizennotes::proto::BackgroundServiceEvent& event) {
  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(event.timestamp()));
  return protocol::BackgroundService::BackgroundServiceEvent::Create()
      .SetTimestamp(timestamp.InMillisecondsFSinceUnixEpoch() /
                    1'000)  // milliseconds -> seconds
      .SetOrigin(event.origin())
      .SetServiceWorkerRegistrationId(
          base::NumberToString(event.service_worker_registration_id()))
      .SetService(ServiceEnumToName(event.background_service()))
      .SetEventName(event.event_name())
      .SetInstanceId(event.instance_id())
      .SetEventMetadata(ProtoMapToArray(event.event_metadata()))
      .SetStorageKey(event.storage_key())
      .Build();
}

}  // namespace

CNBackgroundServiceHandler::CNBackgroundServiceHandler()
    : CitizenNotesDomainHandler(BackgroundService::Metainfo::domainName),
      citizennotes_context_(nullptr) {}

CNBackgroundServiceHandler::~CNBackgroundServiceHandler() {
  DCHECK(enabled_services_.empty());
}

void CNBackgroundServiceHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<BackgroundService::Frontend>(dispatcher->channel());
  BackgroundService::Dispatcher::wire(dispatcher, this);
}

void CNBackgroundServiceHandler::SetRenderer(int process_host_id,
                                           RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_host_id);
  if (!process_host) {
    SetCitizenNotesContext(nullptr);
    enabled_services_.clear();
    return;
  }

  //auto* storage_partition =
  //    static_cast<StoragePartitionImpl*>(process_host->GetStoragePartition());

  //SetCitizenNotesContext(storage_partition->GetDevToolsBackgroundServicesContext()); //TODO: Adapt
  DCHECK(citizennotes_context_);
}

void CNBackgroundServiceHandler::SetCitizenNotesContext(
    CitizenNotesBackgroundServicesContext* citizennotes_context) {
  if (citizennotes_context_ == citizennotes_context) {
    return;
  }

  if (citizennotes_context_ && !enabled_services_.empty()) {
    citizennotes_context_->RemoveObserver(this);
  }

  citizennotes_context_ =
      static_cast<CitizenNotesBackgroundServicesContextImpl*>(citizennotes_context);

  if (citizennotes_context_ && !enabled_services_.empty()) {
    citizennotes_context_->AddObserver(this);
  }
}

Response CNBackgroundServiceHandler::Disable() {
  SetCitizenNotesContext(nullptr);
  enabled_services_.clear();
  return Response::Success();
}

void CNBackgroundServiceHandler::StartObserving(
    const std::string& service,
    std::unique_ptr<StartObservingCallback> callback) {
  DCHECK(citizennotes_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == citizennotes::proto::BackgroundService::UNKNOWN) {
    callback->sendFailure(Response::InvalidParams("Invalid service name"));
    return;
  }

  if (enabled_services_.count(service_enum)) {
    callback->sendSuccess();
    return;
  }

  if (enabled_services_.empty())
    citizennotes_context_->AddObserver(this);
  enabled_services_.insert(service_enum);

  bool is_recording = citizennotes_context_->IsRecording(service_enum);

  DCHECK(frontend_);
  frontend_->RecordingStateChanged(is_recording, service);

  citizennotes_context_->GetLoggedBackgroundServiceEvents(
      service_enum,
      base::BindOnce(&CNBackgroundServiceHandler::DidGetLoggedEvents,
                     weak_ptr_factory_.GetWeakPtr(), service_enum,
                     std::move(callback)));
}

Response CNBackgroundServiceHandler::StopObserving(const std::string& service) {
  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == citizennotes::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  if (!enabled_services_.count(service_enum))
    return Response::Success();

  enabled_services_.erase(service_enum);
  if (enabled_services_.empty())
    citizennotes_context_->RemoveObserver(this);

  return Response::Success();
}

void CNBackgroundServiceHandler::DidGetLoggedEvents(
    citizennotes::proto::BackgroundService service,
    std::unique_ptr<StartObservingCallback> callback,
    std::vector<citizennotes::proto::BackgroundServiceEvent> events) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // These events won't be duplicated in `OnEventReceived` since we are using
  // sequenced task runners.
  for (const auto& event : events)
    frontend_->BackgroundServiceEventReceived(ToBackgroundServiceEvent(event));

  return callback->sendSuccess();
}

Response CNBackgroundServiceHandler::SetRecording(bool should_record,
                                                const std::string& service) {
  DCHECK(citizennotes_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == citizennotes::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  if (should_record) {
    citizennotes_context_->StartRecording(service_enum);
  } else {
    citizennotes_context_->StopRecording(service_enum);
  }

  return Response::Success();
}

Response CNBackgroundServiceHandler::ClearEvents(const std::string& service) {
  DCHECK(citizennotes_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == citizennotes::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  citizennotes_context_->ClearLoggedBackgroundServiceEvents(service_enum);
  return Response::Success();
}

void CNBackgroundServiceHandler::OnEventReceived(
    const citizennotes::proto::BackgroundServiceEvent& event) {
  if (!enabled_services_.count(event.background_service()))
    return;

  frontend_->BackgroundServiceEventReceived(ToBackgroundServiceEvent(event));
}

void CNBackgroundServiceHandler::OnRecordingStateChanged(
    bool should_record,
    citizennotes::proto::BackgroundService service) {
  if (!enabled_services_.count(service))
    return;

  frontend_->RecordingStateChanged(should_record, ServiceEnumToName(service));
}

}  // namespace protocol
}  // namespace content
