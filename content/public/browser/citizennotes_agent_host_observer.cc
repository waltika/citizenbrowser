// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/citizennotes_agent_host_observer.h"

namespace content {

CitizenNotesAgentHostObserver::~CitizenNotesAgentHostObserver() {
}

bool CitizenNotesAgentHostObserver::ShouldForceCitizenNotesAgentHostCreation() {
  return false;
}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostCreated(
    CitizenNotesAgentHost* agent_host) {
}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostNavigated(
    CitizenNotesAgentHost* agent_host) {}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostProcessChanged(
    CitizenNotesAgentHost* agent_host) {}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostAttached(
    CitizenNotesAgentHost* agent_host) {
}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostDetached(
    CitizenNotesAgentHost* agent_host) {
}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostDestroyed(
    CitizenNotesAgentHost* agent_host) {
}

void CitizenNotesAgentHostObserver::CitizenNotesAgentHostCrashed(
    CitizenNotesAgentHost* agent_host,
    base::TerminationStatus status) {}

}  // namespace content
