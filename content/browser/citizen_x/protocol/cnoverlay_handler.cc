// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnoverlay_handler.h"

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"

#include <stdint.h>
#include <utility>

namespace content {
namespace protocol {

CNOverlayHandler::CNOverlayHandler()
    : CitizenNotesDomainHandler(Overlay::Metainfo::domainName) {}

CNOverlayHandler::~CNOverlayHandler() = default;

void CNOverlayHandler::Wire(UberDispatcher* dispatcher) {
  Overlay::Dispatcher::wire(dispatcher, this);
}

void CNOverlayHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  if (host_ == frame_host)
    return;
  host_ = frame_host;
  UpdateCaptureInputEvents();
}

Response CNOverlayHandler::SetInspectMode(
    const String& in_mode,
    Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  inspect_mode_ = in_mode;
  UpdateCaptureInputEvents();
  return Response::FallThrough();
}

Response CNOverlayHandler::SetPausedInDebuggerMessage(Maybe<String> message) {
  paused_message_ = message.value_or(std::string());
  UpdateCaptureInputEvents();
  return Response::FallThrough();
}

Response CNOverlayHandler::Disable() {
  inspect_mode_ = std::string();
  paused_message_ = std::string();
  UpdateCaptureInputEvents();
  return Response::FallThrough();
}

void CNOverlayHandler::UpdateCaptureInputEvents() {
  if (!host_)
    return;
  auto* web_contents = WebContentsImpl::FromRenderFrameHostImpl(host_);
  if (!web_contents)
    return;
  bool capture_input =
      inspect_mode_ == Overlay::InspectModeEnum::CaptureAreaScreenshot ||
      !paused_message_.empty();
  if (!web_contents->GetInputEventRouter())
    return;
  web_contents->GetInputEventRouter()->set_route_to_root_for_devtools(
      capture_input);
}

}  // namespace protocol
}  // namespace content
