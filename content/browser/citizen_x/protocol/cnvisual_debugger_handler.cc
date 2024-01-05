// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnvisual_debugger_handler.h"

#include <string.h>
#include <algorithm>

#include "base/json/json_writer.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace content {
namespace protocol {

CNVisualDebuggerHandler::CNVisualDebuggerHandler()
    : CitizenNotesDomainHandler(VisualDebugger::Metainfo::domainName) {}

CNVisualDebuggerHandler::~CNVisualDebuggerHandler() {
  StopStream();
}

void CNVisualDebuggerHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<VisualDebugger::Frontend>(dispatcher->channel());
  VisualDebugger::Dispatcher::wire(dispatcher, this);
}

DispatchResponse CNVisualDebuggerHandler::FilterStream(
    std::unique_ptr<base::Value::Dict> in_filter) {
  auto* host = GpuProcessHost::Get();
  host->gpu_host()->FilterVisualDebugStream(std::move(*in_filter));

  return DispatchResponse::Success();
}

DispatchResponse CNVisualDebuggerHandler::StartStream() {
  enabled_ = true;

  auto* host = GpuProcessHost::Get();
  host->gpu_host()->StartVisualDebugStream(base::BindPostTask(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&CNVisualDebuggerHandler::OnFrameResponse,
                          weak_ptr_factory_.GetWeakPtr()),
      FROM_HERE));
  return DispatchResponse::Success();
}

void CNVisualDebuggerHandler::OnFrameResponse(base::Value json) {
  // This should be called via the 'BindPostTask' in 'StartStream' function
  // above and thus should be in the correct thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  frontend_->FrameResponse(
      std::make_unique<base::Value::Dict>(std::move(json).TakeDict()));
}

DispatchResponse CNVisualDebuggerHandler::StopStream() {
  if (enabled_) {
    auto* host = GpuProcessHost::Get();
    host->gpu_host()->StopVisualDebugStream();
  }
  enabled_ = false;
  return DispatchResponse::Success();
}
}  // namespace protocol
}  // namespace content
