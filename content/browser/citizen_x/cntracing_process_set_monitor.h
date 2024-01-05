// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_TRACING_PROCESS_SET_MONITOR_H_
#define CONTENT_BROWSER_CITIZENNOTES_TRACING_PROCESS_SET_MONITOR_H_

#include <memory>
#include <unordered_set>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/process/process_handle.h"
#include "base/scoped_observation.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

class CNTracingProcessSetMonitor : public CitizenNotesSession::ChildObserver,
                                 public CitizenNotesAgentHostObserver {
 public:
  using ProcessAddedCallback =
      base::RepeatingCallback<void(base::ProcessId pid)>;

  // |callback| will be invoked once per each new process added after start.
  // The process present at start time are not reported via the callback
  // and are available through |GetPids()| below.
  static std::unique_ptr<CNTracingProcessSetMonitor> Start(
      CitizenNotesSession& root_session,
      ProcessAddedCallback callback);

  CNTracingProcessSetMonitor(const CNTracingProcessSetMonitor& r) = delete;
  ~CNTracingProcessSetMonitor() override;

  const std::unordered_set<base::ProcessId>& GetPids() const {
    return known_pids_;
  }

  void AddProcess(base::ProcessId pid);

 private:
  CNTracingProcessSetMonitor(CitizenNotesSession& root_session,
                           ProcessAddedCallback callback);

  // CitizenNotesSession::ChildObserver methods.
  void SessionAttached(CitizenNotesSession& session) override;

  // CitizenNotesAgentHostObserver methods.
  void CitizenNotesAgentHostDetached(CitizenNotesAgentHost* host) override;
  void CitizenNotesAgentHostDestroyed(CitizenNotesAgentHost* host) override;
  void CitizenNotesAgentHostProcessChanged(CitizenNotesAgentHost* host) override;

  void MaybeAddProcess(CitizenNotesAgentHost* host);

  raw_ref<CitizenNotesSession> const root_session_;
  base::ScopedObservation<CitizenNotesSession, CitizenNotesSession::ChildObserver>
      session_observation_{this};
  const ProcessAddedCallback process_added_callback_;

  bool in_init_{false};
  std::unordered_set<const CitizenNotesAgentHost*> hosts_;
  std::unordered_set<base::ProcessId> known_pids_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_TRACING_PROCESS_SET_MONITOR_H_
