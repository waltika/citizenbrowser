// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_DOM_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_DOM_HANDLER_H_

#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/dom.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class CNDOMHandler : public CitizenNotesDomainHandler,
                   public DOM::Backend {
 public:
  explicit CNDOMHandler(bool allow_file_access);

  CNDOMHandler(const CNDOMHandler&) = delete;
  CNDOMHandler& operator=(const CNDOMHandler&) = delete;

  ~CNDOMHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  Response SetFileInputFiles(
      std::unique_ptr<protocol::Array<std::string>> files,
      Maybe<DOM::NodeId> node_id,
      Maybe<DOM::BackendNodeId> backend_node_id,
      Maybe<String> in_object_id) override;

 private:
  RAW_PTR_EXCLUSION RenderFrameHostImpl* host_;
  bool allow_file_access_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_DOM_HANDLER_H_
