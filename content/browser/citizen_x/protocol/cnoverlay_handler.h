// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_OVERLAY_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_OVERLAY_HANDLER_H_

#include <set>

#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/overlay.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class CNOverlayHandler : public CitizenNotesDomainHandler, public Overlay::Backend {
 public:
  CNOverlayHandler();

  CNOverlayHandler(const CNOverlayHandler&) = delete;
  CNOverlayHandler& operator=(const CNOverlayHandler&) = delete;

  ~CNOverlayHandler() override;
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  Response SetInspectMode(
      const String& in_mode,
      Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) override;
  Response SetPausedInDebuggerMessage(Maybe<String> in_message) override;
  Response Disable() override;

 private:
  void UpdateCaptureInputEvents();

  RAW_PTR_EXCLUSION RenderFrameHostImpl* host_ = nullptr;
  std::string inspect_mode_;
  std::string paused_message_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_OVERLAY_HANDLER_H_
