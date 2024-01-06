/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/controller/citizen_notes_frontend_impl.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_citizen_notes_host.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/citizen_notes_host.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/classic_script.h"

namespace blink {

// static
void CitizenNotesFrontendImpl::BindMojoRequest(
    LocalFrame* local_frame,
    mojo::PendingAssociatedReceiver<mojom::blink::CitizenNotesFrontend> receiver) {
  if (!local_frame)
    return;
  local_frame->ProvideSupplement(MakeGarbageCollected<CitizenNotesFrontendImpl>(
      *local_frame, std::move(receiver)));
}

// static
CitizenNotesFrontendImpl* CitizenNotesFrontendImpl::From(LocalFrame* local_frame) {
  if (!local_frame)
    return nullptr;
  return local_frame->RequireSupplement<CitizenNotesFrontendImpl>();
}

// static
const char CitizenNotesFrontendImpl::kSupplementName[] = "CitizenNotesFrontendImpl";

CitizenNotesFrontendImpl::CitizenNotesFrontendImpl(
    LocalFrame& frame,
    mojo::PendingAssociatedReceiver<mojom::blink::CitizenNotesFrontend> receiver)
    : Supplement<LocalFrame>(frame) {
  receiver_.Bind(std::move(receiver),
                 frame.GetTaskRunner(TaskType::kMiscPlatformAPI));
}

CitizenNotesFrontendImpl::~CitizenNotesFrontendImpl() = default;

void CitizenNotesFrontendImpl::DidClearWindowObject() {
  if (host_.is_bound()) {
    v8::Isolate* isolate = GetSupplementable()->DomWindow()->GetIsolate();
    // Use higher limit for CitizenNotes isolate so that it does not OOM when
    // profiling large heaps.
    isolate->IncreaseHeapLimitForDebugging();
    ScriptState* script_state = ToScriptStateForMainWorld(GetSupplementable());
    DCHECK(script_state);
    ScriptState::Scope scope(script_state);
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (citizennotes_host_)
      citizennotes_host_->DisconnectClient();
    citizennotes_host_ =
        MakeGarbageCollected<CitizenNotesHost>(this, GetSupplementable());
    v8::Local<v8::Object> global = script_state->GetContext()->Global();
    v8::Local<v8::Value> citizennotes_host_obj =
        ToV8(citizennotes_host_.Get(), global, script_state->GetIsolate());
    DCHECK(!citizennotes_host_obj.IsEmpty());
    global
        ->Set(script_state->GetContext(),
              V8AtomicString(isolate, "CitizenNotesHost"), citizennotes_host_obj)
        .Check();
  }

  if (!api_script_.empty()) {
    ClassicScript::CreateUnspecifiedScript(api_script_)
        ->RunScript(GetSupplementable()->DomWindow());
  }
}

void CitizenNotesFrontendImpl::SetupCitizenNotesFrontend(
    const String& api_script,
    mojo::PendingAssociatedRemote<mojom::blink::CitizenNotesFrontendHost> host) {
  LocalFrame* frame = GetSupplementable();
  DCHECK(frame->IsMainFrame());
  if (frame->GetWidgetForLocalRoot()) {
    frame->GetWidgetForLocalRoot()->SetLayerTreeDebugState(
        cc::LayerTreeDebugState());
  } else {
    frame->AddWidgetCreationObserver(this);
  }
  frame->GetPage()->GetSettings().SetForceDarkModeEnabled(false);
  api_script_ = api_script;
  host_.Bind(std::move(host),
             GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  host_.set_disconnect_handler(WTF::BindOnce(
      &CitizenNotesFrontendImpl::DestroyOnHostGone, WrapWeakPersistent(this)));
  GetSupplementable()->GetPage()->SetDefaultPageScaleLimits(1.f, 1.f);
}

void CitizenNotesFrontendImpl::OnLocalRootWidgetCreated() {
  GetSupplementable()->GetWidgetForLocalRoot()->SetLayerTreeDebugState(
      cc::LayerTreeDebugState());
}

void CitizenNotesFrontendImpl::SetupCitizenNotesExtensionAPI(
    const String& extension_api) {
  DCHECK(!GetSupplementable()->IsMainFrame());
  api_script_ = extension_api;
}

void CitizenNotesFrontendImpl::SendMessageToEmbedder(base::Value::Dict message) {
  if (host_.is_bound())
    host_->DispatchEmbedderMessage(std::move(message));
}

void CitizenNotesFrontendImpl::DestroyOnHostGone() {
  if (citizennotes_host_)
    citizennotes_host_->DisconnectClient();
  GetSupplementable()->RemoveSupplement<CitizenNotesFrontendImpl>();
}

void CitizenNotesFrontendImpl::Trace(Visitor* visitor) const {
  visitor->Trace(citizennotes_host_);
  visitor->Trace(host_);
  visitor->Trace(receiver_);
  Supplement<LocalFrame>::Trace(visitor);
}

}  // namespace blink
