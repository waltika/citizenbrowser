// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_AUDITS_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_AUDITS_HANDLER_H_

#include "content/browser/citizen_x/protocol/audits.h"
#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class CitizenNotesAgentHostImpl;
class RenderFrameHostImpl;

namespace protocol {

class CNAuditsHandler final : public CitizenNotesDomainHandler,
                            public Audits::Backend {
 public:
  CNAuditsHandler();

  CNAuditsHandler(const CNAuditsHandler&) = delete;
  CNAuditsHandler& operator=(const CNAuditsHandler&) = delete;

  ~CNAuditsHandler() override;

  static std::vector<CNAuditsHandler*> ForAgentHost(CitizenNotesAgentHostImpl* host);

  // CitizenNotesDomainHandler implementation.
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // Audits::Backend implementation.
  DispatchResponse Disable() override;
  DispatchResponse Enable() override;

  void OnIssueAdded(const protocol::Audits::InspectorIssue* issue);

 private:
  std::unique_ptr<Audits::Frontend> frontend_;
  bool enabled_ = false;
  RAW_PTR_EXCLUSION RenderFrameHostImpl* host_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_AUDITS_HANDLER_H_
