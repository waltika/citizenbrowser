// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CITIZENNOTES_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CITIZENNOTES_AGENT_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class CoreProbeSink;
class CitizenNotesSession;
class ExecutionContext;
class InspectedFrames;
class InspectorTaskRunner;
class WorkerThread;
struct WorkerCitizenNotesParams;

// All public methods of this class are expected to be called on the same thread
// that created the instance. That might be the main thread or a worker thread.
// If used on a worker via BindReceiverForWorker() this class will delegate
// internally to the IO thread to avoid blocking the worker thread. See
// CitizenNotesAgent::IOAgent for more details.
class CORE_EXPORT CitizenNotesAgent : public GarbageCollected<CitizenNotesAgent>,
                                  public mojom::blink::CitizenNotesAgent {
 public:
  class Client {
   public:
    virtual ~Client() = default;
    virtual void AttachSession(CitizenNotesSession*, bool restore) = 0;
    virtual void DetachSession(CitizenNotesSession*) = 0;
    virtual void InspectElement(const gfx::Point&) = 0;
    virtual void DebuggerTaskStarted() = 0;
    virtual void DebuggerTaskFinished() = 0;
  };

  static std::unique_ptr<WorkerCitizenNotesParams> WorkerThreadCreated(
      ExecutionContext* parent_context,
      WorkerThread*,
      const KURL&,
      const String& global_scope_name,
      const absl::optional<const DedicatedWorkerToken>& token);
  static void WorkerThreadTerminated(ExecutionContext* parent_context,
                                     WorkerThread*);

  CitizenNotesAgent(Client*,
                InspectedFrames*,
                CoreProbeSink*,
                scoped_refptr<InspectorTaskRunner> inspector_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~CitizenNotesAgent() override;

  void Dispose();
  void FlushProtocolNotifications();
  void DebuggerPaused();
  void DebuggerResumed();
  // For workers, we use the IO thread similar to CitizenNotesSession::IOSession to
  // ensure that we can always interrupt a worker that is stuck in JS. We don't
  // use an associated channel for workers, meaning we don't have the ordering
  // constraints related to navigation that the non-worker agents have.
  void BindReceiverForWorker(
      mojo::PendingRemote<mojom::blink::CitizenNotesAgentHost>,
      mojo::PendingReceiver<mojom::blink::CitizenNotesAgent>,
      scoped_refptr<base::SingleThreadTaskRunner>);
  // Used for non-worker agents. These do not use the IO thread like we do for
  // workers, and they use associated mojo interfaces.
  void BindReceiver(
      mojo::PendingAssociatedRemote<mojom::blink::CitizenNotesAgentHost>,
      mojo::PendingAssociatedReceiver<mojom::blink::CitizenNotesAgent>,
      scoped_refptr<base::SingleThreadTaskRunner>);
  virtual void Trace(Visitor*) const;

 private:
  friend class CitizenNotesSession;
  class IOAgent;

  // mojom::blink::CitizenNotesAgent implementation.
  void AttachCitizenNotesSession(
      mojo::PendingAssociatedRemote<mojom::blink::CitizenNotesSessionHost>,
      mojo::PendingAssociatedReceiver<mojom::blink::CitizenNotesSession>
          main_session,
      mojo::PendingReceiver<mojom::blink::CitizenNotesSession> io_session,
      mojom::blink::CitizenNotesSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses,
      bool client_is_trusted,
      const WTF::String& session_id,
      bool session_waits_for_debugger) override;
  void InspectElement(const gfx::Point& point) override;
  void ReportChildTargets(bool report,
                          bool wait_for_debugger,
                          base::OnceClosure callback) override;
  void GetUniqueFormControlId(int nodeId,
                              GetUniqueFormControlIdCallback callback) override;

  void ReportChildTargetsPostCallbackToIO(bool report,
                                          bool wait_for_debugger,
                                          CrossThreadOnceClosure callback);

  struct WorkerData {
    KURL url;
    mojo::PendingRemote<mojom::blink::CitizenNotesAgent> agent_remote;
    mojo::PendingReceiver<mojom::blink::CitizenNotesAgentHost> host_receiver;
    base::UnguessableToken citizennotes_worker_token;
    bool waiting_for_debugger;
    String name;
    mojom::blink::CitizenNotesExecutionContextType context_type;
  };
  void ReportChildTarget(std::unique_ptr<WorkerData>);

  void CleanupConnection();

  void AttachCitizenNotesSessionImpl(
      mojo::PendingAssociatedRemote<mojom::blink::CitizenNotesSessionHost>,
      mojo::PendingAssociatedReceiver<mojom::blink::CitizenNotesSession>
          main_session,
      mojo::PendingReceiver<mojom::blink::CitizenNotesSession> io_session,
      mojom::blink::CitizenNotesSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses,
      bool client_is_trusted,
      const WTF::String& session_id,
      bool session_waits_for_debugger);
  void InspectElementImpl(const gfx::Point& point);
  void ReportChildTargetsImpl(bool report,
                              bool wait_for_debugger,
                              base::OnceClosure callback);
  Client* client_;
  // CitizenNotesAgent is not tied to ExecutionContext
  HeapMojoAssociatedReceiver<mojom::blink::CitizenNotesAgent, CitizenNotesAgent>
      associated_receiver_{this, nullptr};
  // CitizenNotesAgent is not tied to ExecutionContext
  HeapMojoRemote<mojom::blink::CitizenNotesAgentHost> host_remote_{nullptr};
  // CitizenNotesAgent is not tied to ExecutionContext
  HeapMojoAssociatedRemote<mojom::blink::CitizenNotesAgentHost>
      associated_host_remote_{nullptr};
  Member<InspectedFrames> inspected_frames_;
  Member<CoreProbeSink> probe_sink_;
  HeapHashSet<Member<CitizenNotesSession>> sessions_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  HashMap<WorkerThread*, std::unique_ptr<WorkerData>>
      unreported_child_worker_threads_;
  IOAgent* io_agent_{nullptr};
  bool report_child_workers_ = false;
  bool pause_child_workers_on_start_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CITIZENNOTES_AGENT_H_
