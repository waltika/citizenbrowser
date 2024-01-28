// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/worklet_citizennotes_agent_host.h"

namespace content {

WorkletCitizenNotesAgentHost::WorkletCitizenNotesAgentHost(
    int process_id,
    const GURL& url,
    const std::string& name,
    const base::UnguessableToken& citizennotes_worker_token,
    const std::string& parent_id,
    base::OnceCallback<void(CitizenNotesAgentHostImpl*)> destroyed_callback)
    : WorkerOrWorkletCitizenNotesAgentHost(process_id,
                                       url,
                                       name,
                                       citizennotes_worker_token,
                                       parent_id,
                                       std::move(destroyed_callback)) {
  NotifyCreated();
}

WorkletCitizenNotesAgentHost::~WorkletCitizenNotesAgentHost() = default;

std::string WorkletCitizenNotesAgentHost::GetType() {
  return kTypeWorklet;
}

}  // namespace content
