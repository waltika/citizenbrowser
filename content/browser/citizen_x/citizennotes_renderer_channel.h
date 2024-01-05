// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_RENDERER_CHANNEL_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_RENDERER_CHANNEL_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace gfx {
class Point;
}

namespace content {

class CitizenNotesAgentHostImpl;
class CitizenNotesSession;
class RenderFrameHostImpl;
class WorkerCitizenNotesAgentHost;

// This class encapsulates a connection to blink::mojom::CitizenNotesAgent
// in the renderer (either RenderFrame or some kind of worker).
// When the renderer changes (e.g. worker restarts or a new RenderFrame
// is used for the frame), different CitizenNotesAgentHostImpl subclasses
// retrieve a new blink::mojom::CitizenNotesAgent pointer, and this channel
// starts using it for all existing and future sessions.
class CitizenNotesRendererChannel : public blink::mojom::CitizenNotesAgentHost {
 public:
  explicit CitizenNotesRendererChannel(CitizenNotesAgentHostImpl* owner);

  CitizenNotesRendererChannel(const CitizenNotesRendererChannel&) = delete;
  CitizenNotesRendererChannel& operator=(const CitizenNotesRendererChannel&) = delete;

  ~CitizenNotesRendererChannel() override;

  // Dedicated workers use non-associated version,
  // while frames and other workers use CitizenNotesAgent associated
  // with respective control interfraces. See mojom for details.
  void SetRenderer(
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver,
      int process_id,
      base::OnceClosure connection_error = base::NullCallback());
  void SetRendererAssociated(
      mojo::PendingAssociatedRemote<blink::mojom::CitizenNotesAgent> agent_remote,
      mojo::PendingAssociatedReceiver<blink::mojom::CitizenNotesAgentHost>
          host_receiver,
      int process_id,
      RenderFrameHostImpl* frame_host);
  void AttachSession(CitizenNotesSession* session);
  void InspectElement(const gfx::Point& point);
  using GetUniqueFormCallback = base::OnceCallback<void(uint64_t)>;
  void GetUniqueFormControlId(int node_id, GetUniqueFormCallback callback);
  void ForceDetachWorkerSessions();

  using ChildTargetCreatedCallback =
      base::RepeatingCallback<void(CitizenNotesAgentHostImpl*,
                                   bool waiting_for_debugger)>;
  void SetReportChildTargets(ChildTargetCreatedCallback report_callback,
                             bool wait_for_debugger,
                             base::OnceClosure completion_callback);

 private:
  // blink::mojom::CitizenNotesAgentHost implementation.
  void ChildTargetCreated(
      mojo::PendingRemote<blink::mojom::CitizenNotesAgent> worker_citizennotes_agent,
      mojo::PendingReceiver<blink::mojom::CitizenNotesAgentHost> host_receiver,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& citizennotes_worker_token,
      bool waiting_for_debugger,
      blink::mojom::CitizenNotesExecutionContextType context_type) override;
  void ChildTargetDestroyed(CitizenNotesAgentHostImpl*);
  void MainThreadDebuggerPaused() override;
  void MainThreadDebuggerResumed() override;

  void CleanupConnection();
  void SetRendererInternal(blink::mojom::CitizenNotesAgent* agent,
                           int process_id,
                           RenderFrameHostImpl* frame_host,
                           bool force_using_io);
  void ReportChildTargetsCallback();

  RAW_PTR_EXCLUSION CitizenNotesAgentHostImpl* owner_;
  mojo::Receiver<blink::mojom::CitizenNotesAgentHost> receiver_{this};
  mojo::AssociatedReceiver<blink::mojom::CitizenNotesAgentHost>
      associated_receiver_{this};
  mojo::Remote<blink::mojom::CitizenNotesAgent> agent_remote_;
  mojo::AssociatedRemote<blink::mojom::CitizenNotesAgent> associated_agent_remote_;
  int process_id_;
  RAW_PTR_EXCLUSION RenderFrameHostImpl* frame_host_ = nullptr;
  base::flat_set<WorkerCitizenNotesAgentHost*> child_targets_;
  ChildTargetCreatedCallback child_target_created_callback_;
  bool wait_for_debugger_ = false;
  base::OnceClosure set_report_completion_callback_;
  base::WeakPtrFactory<CitizenNotesRendererChannel> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_RENDERER_CHANNEL_H_
