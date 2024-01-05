// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_BACKGROUND_SERVICES_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_BACKGROUND_SERVICES_CONTEXT_IMPL_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/browser/citizen_x/citizennotes_background_services.pb.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/public/browser/citizennotes_background_services_context.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// This class is responsible for persisting the debugging events for the
// relevant Web Platform Features. The contexts of the feature will have a
// reference to this, and perform the logging operation.
// This class is also responsible for reading back the data to the CitizenNotes
// client, as the protocol handler will have access to an instance of the
// context.
//
// Lives on the UI thread.
class CONTENT_EXPORT CitizenNotesBackgroundServicesContextImpl
    : public CitizenNotesBackgroundServicesContext {
 public:
  using GetLoggedBackgroundServiceEventsCallback = base::OnceCallback<void(
      std::vector<citizennotes::proto::BackgroundServiceEvent>)>;

  class EventObserver : public base::CheckedObserver {
   public:
    // Notifies observers of the logged event.
    virtual void OnEventReceived(
        const citizennotes::proto::BackgroundServiceEvent& event) = 0;
    virtual void OnRecordingStateChanged(
        bool should_record,
        citizennotes::proto::BackgroundService service) = 0;
  };

  CitizenNotesBackgroundServicesContextImpl(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~CitizenNotesBackgroundServicesContextImpl() override;

  CitizenNotesBackgroundServicesContextImpl(
      const CitizenNotesBackgroundServicesContextImpl&) = delete;
  CitizenNotesBackgroundServicesContextImpl& operator=(
      const CitizenNotesBackgroundServicesContextImpl&) = delete;

  void AddObserver(EventObserver* observer);
  void RemoveObserver(const EventObserver* observer);

  // CitizenNotesBackgroundServicesContext overrides:
  bool IsRecording(CitizenNotesBackgroundService service) override;
  void LogBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      blink::StorageKey storage_key,
      CitizenNotesBackgroundService service,
      const std::string& event_name,
      const std::string& instance_id,
      const std::map<std::string, std::string>& event_metadata) override;

  // Helper function for public override. Can be used directly.
  bool IsRecording(citizennotes::proto::BackgroundService service);

  // Enables recording mode for |service|. This is capped at 3 days in case
  // developers forget to switch it off.
  void StartRecording(citizennotes::proto::BackgroundService service);

  // Disables recording mode for |service|.
  void StopRecording(citizennotes::proto::BackgroundService service);

  // Queries all logged events for |service| and returns them in sorted order
  // (by timestamp). |callback| is called with an empty vector if there was an
  // error.
  void GetLoggedBackgroundServiceEvents(
      citizennotes::proto::BackgroundService service,
      GetLoggedBackgroundServiceEventsCallback callback);

  // Clears all logged events related to |service|.
  void ClearLoggedBackgroundServiceEvents(
      citizennotes::proto::BackgroundService service);

 private:
  friend class CitizenNotesBackgroundServicesContextTest;

  // Whether |service| has an expiration time and it was exceeded.
  bool IsRecordingExpired(citizennotes::proto::BackgroundService service);

  void DidGetUserData(
      GetLoggedBackgroundServiceEventsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  void NotifyEventObservers(
      const citizennotes::proto::BackgroundServiceEvent& event);

  void OnRecordingTimeExpired(citizennotes::proto::BackgroundService service);

  const raw_ref<BrowserContext> browser_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Maps from the background service to the time up until the events can be
  // recorded. The BackgroundService enum is used as the index.
  std::array<base::Time, citizennotes::proto::BackgroundService::COUNT>
      expiration_times_;

  base::ObserverList<EventObserver> observers_;

  base::WeakPtrFactory<CitizenNotesBackgroundServicesContextImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_BACKGROUND_SERVICES_CONTEXT_IMPL_H_
