// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/protocol/protocol.h"

namespace content {
namespace protocol {

CitizenNotesDomainHandler::CitizenNotesDomainHandler(const std::string& name)
    : name_(name) {
}

CitizenNotesDomainHandler::~CitizenNotesDomainHandler() = default;

void CitizenNotesDomainHandler::SetRenderer(int process_host_id,
                                        RenderFrameHostImpl* frame_host) {}

void CitizenNotesDomainHandler::Wire(UberDispatcher* dispatcher) {
}

Response CitizenNotesDomainHandler::Disable() {
  return Response::Success();
}

void CitizenNotesDomainHandler::SetSession(CitizenNotesSession* session) {
  session_ = session;
}

const std::string& CitizenNotesDomainHandler::name() const {
  return name_;
}

CitizenNotesSession* CitizenNotesDomainHandler::session() {
  return session_;
}

}  // namespace protocol
}  // namespace content
