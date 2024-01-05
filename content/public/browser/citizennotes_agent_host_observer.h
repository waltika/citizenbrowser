// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CITIZENNOTES_AGENT_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_CITIZENNOTES_AGENT_HOST_OBSERVER_H_

#include "base/process/kill.h"
#include "content/common/content_export.h"

namespace content {

class CitizenNotesAgentHost;

// Observer API notifies interested parties about changes in CitizenNotesAgentHosts.
class CONTENT_EXPORT CitizenNotesAgentHostObserver {
 public:
  virtual ~CitizenNotesAgentHostObserver();

  // If observer returns |true|, CitizenNotesAgentHost instances are created
  // (and reported in CitizenNotesAgentHostCreated) for every possible devtools
  // target (e.g. WebContents).
  virtual bool ShouldForceCitizenNotesAgentHostCreation();

  // Called when CitizenNotesAgentHost was created and is ready to be used.
  virtual void CitizenNotesAgentHostCreated(CitizenNotesAgentHost* agent_host);

  // Called when CitizenNotesAgentHost was created and is ready to be used.
  virtual void CitizenNotesAgentHostNavigated(CitizenNotesAgentHost* agent_host);

  // Called when a process associated with inspected target has changed.
  virtual void CitizenNotesAgentHostProcessChanged(CitizenNotesAgentHost* agent_host);

  // Called when client has attached to CitizenNotesAgentHost.
  virtual void CitizenNotesAgentHostAttached(CitizenNotesAgentHost* agent_host);

  // Called when client has detached from CitizenNotesAgentHost.
  virtual void CitizenNotesAgentHostDetached(CitizenNotesAgentHost* agent_host);

  // Called when CitizenNotesAgentHost crashed.
  virtual void CitizenNotesAgentHostCrashed(CitizenNotesAgentHost* agent_host,
                                        base::TerminationStatus status);

  // Called when CitizenNotesAgentHost was destroyed.
  virtual void CitizenNotesAgentHostDestroyed(CitizenNotesAgentHost* agent_host);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CITIZENNOTES_AGENT_HOST_OBSERVER_H_
