// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_CITIZENNOTES_OS_CITIZENNOTES_AGENT_H_
#define SERVICES_ACCESSIBILITY_FEATURES_CITIZENNOTES_OS_CITIZENNOTES_AGENT_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"
#include "v8/include/v8-inspector.h"

namespace ax {

class V8Environment;
class OSCitizenNotesSession;
class DebugCommandQueue;

// This class acts as the receiver for a Devtools agent host. Specifically one
// that uses AccessibilityServiceCitizenNotesDelegate.
class OSCitizenNotesAgent : public blink::mojom::CitizenNotesAgent,
                        public v8_inspector::V8InspectorClient {
 public:
  OSCitizenNotesAgent(
      V8Environment& v8_env,
      scoped_refptr<base::SequencedTaskRunner> io_session_task_runner);
  OSCitizenNotesAgent(const OSCitizenNotesAgent&) = delete;
  OSCitizenNotesAgent& operator=(const OSCitizenNotesAgent&) = delete;
  ~OSCitizenNotesAgent() override;

  // Connects an incoming Mojo debugging connection to endpoint `agent`.
  void Connect(
      mojo::PendingAssociatedReceiver<blink::mojom::CitizenNotesAgent> agent);

  // If any session debugging `context_group_id` has an instrumentation
  // breakpoint named `name` set, asks for execution to be paused at next
  // statement.
  void MaybeTriggerInstrumentationBreakpoint(const std::string& name);

  // Creates the V8Session that OSCitizenNotesSession communicates with.
  std::unique_ptr<v8_inspector::V8InspectorSession> ConnectSession(
      OSCitizenNotesSession* session,
      bool session_waits_for_debugger);

 private:
  // CitizenNotesAgent implementation.
  void AttachCitizenNotesSession(
      mojo::PendingAssociatedRemote<blink::mojom::CitizenNotesSessionHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::CitizenNotesSession>
          session_receiver,
      mojo::PendingReceiver<blink::mojom::CitizenNotesSession> io_session_receiver,
      blink::mojom::CitizenNotesSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses,
      bool client_is_trusted,
      const std::string& session_id,
      bool session_waits_for_debugger) override;
  void InspectElement(const ::gfx::Point& point) override;
  void ReportChildTargets(bool report,
                          bool wait_for_debugger,
                          ReportChildTargetsCallback callback) override;
  void GetUniqueFormControlId(int32_t nodeId,
                              GetUniqueFormControlIdCallback callback) override;
  // V8InspectorClient implementation.

  // TODO(b/290815208): Implement after source files are able to be loaded.
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;
  // TODO(b/290815208): Implement
  void runIfWaitingForDebugger(int context_group_id) override;

  // Called via ~OSCitizenNotesSession.
  void SessionDestroyed(OSCitizenNotesSession* session);

  const raw_ref<V8Environment> v8_env_;
  const scoped_refptr<DebugCommandQueue> debug_command_queue_;
  const scoped_refptr<base::SequencedTaskRunner> io_session_task_runner_;
  const std::unique_ptr<v8_inspector::V8Inspector> v8_inspector_;

  // Mojo pipes connected to `this`.
  mojo::AssociatedReceiver<blink::mojom::CitizenNotesAgent> receiver_{this};

  // All OSCitizenNotesSession objects have their lifetime limited by their mojo
  // pipes. Thus this class always outlives the sessions.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::CitizenNotesSession>
      mojom_sessions_;
  // These pointers are kept around for access to the session for non-mojo
  // related tasks. The session destroyed callback ensures these are erased when
  // a session is disconnected.
  std::set<OSCitizenNotesSession*> sessions_;

  SEQUENCE_CHECKER(v8_sequence_checker_);
  base::WeakPtrFactory<OSCitizenNotesAgent> weak_ptr_factory_{this};
};

}  // namespace ax
#endif
