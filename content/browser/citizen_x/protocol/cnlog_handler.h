// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_LOG_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_LOG_HANDLER_H_

#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/log.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class CitizenNotesAgentHostImpl;

namespace protocol {

class CNLogHandler final : public CitizenNotesDomainHandler, public Log::Backend {
 public:
  CNLogHandler();

  CNLogHandler(const CNLogHandler&) = delete;
  CNLogHandler& operator=(const CNLogHandler&) = delete;

  ~CNLogHandler() override;

  static std::vector<CNLogHandler*> ForAgentHost(CitizenNotesAgentHostImpl* host);

  // CitizenNotesDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;

  // Log::Backend implementation.
  DispatchResponse Disable() override;
  DispatchResponse Enable() override;

  void EntryAdded(Log::LogEntry* entry);

 private:
  std::unique_ptr<Log::Frontend> frontend_;
  bool enabled_ = false;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_LOG_HANDLER_H_
