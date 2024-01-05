// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/citizen_x/citizennotes_background_services.pb.h"
#include "content/browser/citizen_x/citizennotes_background_services_context_impl.h"
#include "content/browser/citizen_x/protocol/background_service.h"
#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"

namespace content {

class CitizenNotesBackgroundServicesContext;
class RenderFrameHostImpl;

namespace protocol {

class CNBackgroundServiceHandler
    : public CitizenNotesDomainHandler,
      public BackgroundService::Backend,
      public CitizenNotesBackgroundServicesContextImpl::EventObserver {
 public:
  CNBackgroundServiceHandler();

  CNBackgroundServiceHandler(const CNBackgroundServiceHandler&) = delete;
  CNBackgroundServiceHandler& operator=(const CNBackgroundServiceHandler&) = delete;

  ~CNBackgroundServiceHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  void StartObserving(
      const std::string& service,
      std::unique_ptr<StartObservingCallback> callback) override;
  Response StopObserving(const std::string& service) override;
  Response SetRecording(bool should_record,
                        const std::string& service) override;
  Response ClearEvents(const std::string& service) override;

 private:
  void SetCitizenNotesContext(CitizenNotesBackgroundServicesContext* citizennotes_context);
  void DidGetLoggedEvents(
      citizennotes::proto::BackgroundService service,
      std::unique_ptr<StartObservingCallback> callback,
      std::vector<citizennotes::proto::BackgroundServiceEvent> events);

  void OnEventReceived(
      const citizennotes::proto::BackgroundServiceEvent& event) override;
  void OnRecordingStateChanged(
      bool should_record,
      citizennotes::proto::BackgroundService service) override;

  std::unique_ptr<BackgroundService::Frontend> frontend_;

  // Owned by the storage partition.
  raw_ptr<CitizenNotesBackgroundServicesContextImpl> citizennotes_context_;

  base::flat_set<citizennotes::proto::BackgroundService> enabled_services_;

  base::WeakPtrFactory<CNBackgroundServiceHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_
