// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_DOMAIN_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_DOMAIN_HANDLER_H_

#include "content/browser/citizen_x/protocol/protocol.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_agent_host.h"

namespace content {

class RenderFrameHostImpl;
class CitizenNotesSession;

namespace protocol {

class CitizenNotesDomainHandler {
 public:
  explicit CitizenNotesDomainHandler(const std::string& name);

  CitizenNotesDomainHandler(const CitizenNotesDomainHandler&) = delete;
  CitizenNotesDomainHandler& operator=(const CitizenNotesDomainHandler&) = delete;

  virtual ~CitizenNotesDomainHandler();

  virtual void SetRenderer(int process_host_id,
                           RenderFrameHostImpl* frame_host);
  virtual void Wire(UberDispatcher* dispatcher);
  void SetSession(CitizenNotesSession* session);
  virtual Response Disable();

  const std::string& name() const;

 protected:
  CitizenNotesSession* session();

 private:
  std::string name_;
  RAW_PTR_EXCLUSION CitizenNotesSession* session_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_DOMAIN_HANDLER_H_
