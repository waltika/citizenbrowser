// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_WORKLET_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_WORKLET_CITIZENNOTES_AGENT_HOST_H_

#include "content/browser/citizen_x/worker_or_worklet_citizennotes_agent_host.h"

namespace content {

class WorkletCitizenNotesAgentHost final : public WorkerOrWorkletCitizenNotesAgentHost {
 public:
  WorkletCitizenNotesAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback);

 private:
  ~WorkletCitizenNotesAgentHost() override;

  // CitizenNotes agent host overrides
  std::string GetType() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_WORKLET_CITIZENNOTES_AGENT_HOST_H_
