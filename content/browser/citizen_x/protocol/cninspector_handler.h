// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_INSPECTOR_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_INSPECTOR_HANDLER_H_

#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/inspector.h"

namespace content {

class CitizenNotesAgentHostImpl;
class RenderFrameHostImpl;

namespace protocol {

class CNInspectorHandler : public CitizenNotesDomainHandler,
                         public Inspector::Backend {
 public:
  CNInspectorHandler();

  CNInspectorHandler(const CNInspectorHandler&) = delete;
  CNInspectorHandler& operator=(const CNInspectorHandler&) = delete;

  ~CNInspectorHandler() override;

  static std::vector<CNInspectorHandler*> ForAgentHost(
      CitizenNotesAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  void TargetCrashed();
  void TargetReloadedAfterCrash();
  void TargetDetached(const std::string& reason);

  Response Enable() override;
  Response Disable() override;

 private:
  std::unique_ptr<Inspector::Frontend> frontend_;
  RAW_PTR_EXCLUSION RenderFrameHostImpl* host_ = nullptr;
  bool target_crashed_ = false;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_INSPECTOR_HANDLER_H_
