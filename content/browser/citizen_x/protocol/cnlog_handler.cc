// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnlog_handler.h"

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"

namespace content {
namespace protocol {

CNLogHandler::CNLogHandler() : CitizenNotesDomainHandler(Log::Metainfo::domainName) {}
CNLogHandler::~CNLogHandler() = default;

// static
std::vector<CNLogHandler*> CNLogHandler::ForAgentHost(CitizenNotesAgentHostImpl* host) {
  return host->HandlersByName<CNLogHandler>(Log::Metainfo::domainName);
}

void CNLogHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Log::Frontend>(dispatcher->channel());
  Log::Dispatcher::wire(dispatcher, this);
}

DispatchResponse CNLogHandler::Disable() {
  enabled_ = false;
  return Response::FallThrough();
}

DispatchResponse CNLogHandler::Enable() {
  enabled_ = true;
  return Response::FallThrough();
}

void CNLogHandler::EntryAdded(Log::LogEntry* entry) {
  if (!enabled_) {
    return;
  }
  frontend_->EntryAdded(entry->Clone());
}

}  // namespace protocol
}  // namespace content
