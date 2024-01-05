/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/cnworker_inspector_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/inspector/citizennotes_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_reporter.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/worker_citizennotes_params.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace blink {

// static
CNWorkerInspectorController* CNWorkerInspectorController::Create(
    WorkerThread* thread,
    const KURL& url,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    std::unique_ptr<WorkerCitizenNotesParams> citizennotes_params) {
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(thread->GetIsolate());
  return debugger ? MakeGarbageCollected<CNWorkerInspectorController>(
                        thread, url, debugger, std::move(inspector_task_runner),
                        std::move(citizennotes_params))
                  : nullptr;
}

CNWorkerInspectorController::CNWorkerInspectorController(
    WorkerThread* thread,
    const KURL& url,
    WorkerThreadDebugger* debugger,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    std::unique_ptr<WorkerCitizenNotesParams> citizennotes_params)
    : debugger_(debugger),
      thread_(thread),
      inspected_frames_(nullptr),
      probe_sink_(MakeGarbageCollected<CoreProbeSink>()),
      worker_thread_id_(base::PlatformThread::CurrentId()) {
  // The constructor must run on the backing thread of |thread|. Otherwise, it
  // would be incorrect to initialize |worker_thread_id_| with the current
  // thread id.
  DCHECK(thread->IsCurrentThread());

  probe_sink_->AddInspectorIssueReporter(
      MakeGarbageCollected<InspectorIssueReporter>(
          thread->GetInspectorIssueStorage()));
  probe_sink_->AddInspectorTraceEvents(
      MakeGarbageCollected<InspectorTraceEvents>());
  worker_citizennotes_token_ = citizennotes_params->citizennotes_worker_token;
  parent_citizennotes_token_ = thread->GlobalScope()->GetParentCitizenNotesToken();
  url_ = url;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      Platform::Current()->GetIOTaskRunner();
  if (!parent_citizennotes_token_.is_empty() && io_task_runner) {
    // There may be no io task runner in unit tests.
    wait_for_debugger_ = citizennotes_params->wait_for_debugger;
    agent_ = MakeGarbageCollected<CitizenNotesAgent>(
        this, inspected_frames_.Get(), probe_sink_.Get(),
        std::move(inspector_task_runner), std::move(io_task_runner));
    agent_->BindReceiverForWorker(
        std::move(citizennotes_params->agent_host_remote),
        std::move(citizennotes_params->agent_receiver),
        thread->GetTaskRunner(TaskType::kInternalInspector));
  }
  trace_event::AddEnabledStateObserver(this);
  EmitTraceEvent();
}

CNWorkerInspectorController::~CNWorkerInspectorController() {
  DCHECK(!thread_);
  trace_event::RemoveEnabledStateObserver(this);
}

void CNWorkerInspectorController::AttachSession(CitizenNotesSession* session,
                                              bool restore) {
  if (!session_count_)
    thread_->GetWorkerBackingThread().BackingThread().AddTaskObserver(this);
  session->ConnectToV8(debugger_->GetV8Inspector(),
                       debugger_->ContextGroupId(thread_));
  session->CreateAndAppend<InspectorLogAgent>(
      thread_->GetConsoleMessageStorage(), nullptr, session->V8Session());
  session->CreateAndAppend<InspectorEventBreakpointsAgent>(
      session->V8Session());
  if (auto* scope = DynamicTo<WorkerGlobalScope>(thread_->GlobalScope())) {
    auto* network_agent = session->CreateAndAppend<InspectorNetworkAgent>(
        inspected_frames_.Get(), scope, session->V8Session());
    auto* virtual_time_controller =
        thread_->GetScheduler()->GetVirtualTimeController();
    DCHECK(virtual_time_controller);
    session->CreateAndAppend<InspectorEmulationAgent>(nullptr,
                                                      *virtual_time_controller);
    session->CreateAndAppend<InspectorAuditsAgent>(
        network_agent, thread_->GetInspectorIssueStorage(),
        /*inspected_frames=*/nullptr, /*web_autofill_client=*/nullptr);
    session->CreateAndAppend<InspectorMediaAgent>(inspected_frames_.Get(),
                                                  scope);
  }
  ++session_count_;
}

void CNWorkerInspectorController::DetachSession(CitizenNotesSession*) {
  --session_count_;
  if (!session_count_)
    thread_->GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);
}

void CNWorkerInspectorController::InspectElement(const gfx::Point&) {
  NOTREACHED();
}

void CNWorkerInspectorController::DebuggerTaskStarted() {
  thread_->DebuggerTaskStarted();
}

void CNWorkerInspectorController::DebuggerTaskFinished() {
  thread_->DebuggerTaskFinished();
}

void CNWorkerInspectorController::Dispose() {
  if (agent_)
    agent_->Dispose();
  thread_ = nullptr;
}

void CNWorkerInspectorController::FlushProtocolNotifications() {
  if (agent_)
    agent_->FlushProtocolNotifications();
}

void CNWorkerInspectorController::WaitForDebuggerIfNeeded() {
  if (!wait_for_debugger_)
    return;
  wait_for_debugger_ = false;
  debugger_->PauseWorkerOnStart(thread_);
}

void CNWorkerInspectorController::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {}

void CNWorkerInspectorController::DidProcessTask(
    const base::PendingTask& pending_task) {
  FlushProtocolNotifications();
}

void CNWorkerInspectorController::OnTraceLogEnabled() {
  EmitTraceEvent();
}

void CNWorkerInspectorController::OnTraceLogDisabled() {}

void CNWorkerInspectorController::EmitTraceEvent() {
  if (worker_citizennotes_token_.is_empty())
    return;
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("citizennotes.timeline"),
      "TracingSessionIdForWorker",
      inspector_tracing_session_id_for_worker_event::Data,
      worker_citizennotes_token_, parent_citizennotes_token_, url_, worker_thread_id_);
}

void CNWorkerInspectorController::Trace(Visitor* visitor) const {
  visitor->Trace(agent_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(probe_sink_);
}

}  // namespace blink
