// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/citizennotes_protocol_test_support.h"

#include <memory>

#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

CitizenNotesProtocolTest::CitizenNotesProtocolTest() = default;
CitizenNotesProtocolTest::~CitizenNotesProtocolTest() = default;

void CitizenNotesProtocolTest::Attach() {
  AttachToWebContents(shell()->web_contents());
  shell()->web_contents()->SetDelegate(this);
}

void CitizenNotesProtocolTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
}

void CitizenNotesProtocolTest::TearDownOnMainThread() {
  DetachProtocolClient();
}

bool CitizenNotesProtocolTest::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  console_messages_.push_back(base::UTF16ToUTF8(message));
  return true;
}

}  // namespace content
