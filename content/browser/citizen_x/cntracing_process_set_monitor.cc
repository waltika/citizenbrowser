// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/cntracing_process_set_monitor.h"

#include "content/public/browser/render_process_host.h"

namespace content {

// static
std::unique_ptr<CNTracingProcessSetMonitor> CNTracingProcessSetMonitor::Start(
    CitizenNotesSession& root_session,
    ProcessAddedCallback callback) {
  return base::WrapUnique(
      new CNTracingProcessSetMonitor(root_session, std::move(callback)));
}

void CNTracingProcessSetMonitor::SessionAttached(CitizenNotesSession& session) {
  CHECK(session.GetRootSession() == &*root_session_);
  auto* const host =
      static_cast<CitizenNotesAgentHostImpl*>(session.GetAgentHost());
  CHECK(host);
  if (bool inserted = hosts_.insert(host).second; inserted) {
    MaybeAddProcess(host);
  }
}

void CNTracingProcessSetMonitor::CitizenNotesAgentHostDetached(
    CitizenNotesAgentHost* host) {
  hosts_.erase(host);
}

void CNTracingProcessSetMonitor::CitizenNotesAgentHostDestroyed(
    CitizenNotesAgentHost* host) {
  hosts_.erase(host);
}

void CNTracingProcessSetMonitor::CitizenNotesAgentHostProcessChanged(
    CitizenNotesAgentHost* host) {
  if (!hosts_.contains(host)) {
    return;
  }
  MaybeAddProcess(host);
}

void CNTracingProcessSetMonitor::MaybeAddProcess(CitizenNotesAgentHost* host) {
  base::ProcessId pid =
      static_cast<CitizenNotesAgentHostImpl*>(host)->GetProcessId();
  if (pid == base::kNullProcessId) {
    return;
  }
  AddProcess(pid);
}

void CNTracingProcessSetMonitor::AddProcess(base::ProcessId pid) {
  const bool inserted = known_pids_.insert(pid).second;
  if (inserted && !in_init_) {
    process_added_callback_.Run(pid);
  }
}

CNTracingProcessSetMonitor::CNTracingProcessSetMonitor(
    CitizenNotesSession& root_session,
    ProcessAddedCallback callback)
    : root_session_(root_session),
      process_added_callback_(std::move(callback)) {
  base::AutoReset<bool> auto_reset(&in_init_, true);
  session_observation_.Observe(&root_session);
  CitizenNotesAgentHost::AddObserver(this);
  SessionAttached(root_session);
}

CNTracingProcessSetMonitor::~CNTracingProcessSetMonitor() {
  CitizenNotesAgentHost::RemoveObserver(this);
}

}  // namespace content
