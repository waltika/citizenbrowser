// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_WORKER_CITIZENNOTES_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_WORKER_CITIZENNOTES_PARAMS_H_

#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom-blink.h"

namespace blink {

struct CORE_EXPORT WorkerCitizenNotesParams {
  mojo::PendingReceiver<mojom::blink::CitizenNotesAgent> agent_receiver;
  mojo::PendingRemote<mojom::blink::CitizenNotesAgentHost> agent_host_remote;
  bool wait_for_debugger = false;
  base::UnguessableToken citizennotes_worker_token;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_WORKER_CITIZENNOTES_PARAMS_H_
