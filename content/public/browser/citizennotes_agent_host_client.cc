// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/citizennotes_agent_host_client.h"

namespace content {

bool CitizenNotesAgentHostClient::MayAttachToRenderFrameHost(
    RenderFrameHost* render_frame_host) {
  return true;
}

bool CitizenNotesAgentHostClient::MayAttachToURL(const GURL& url, bool is_webui) {
  return true;
}

// Defaults to true, restricted clients must override this to false.
bool CitizenNotesAgentHostClient::IsTrusted() {
  return true;
}

// File access is allowed by default, only restricted clients that represent
// not entirely trusted protocol peers override this to false.
bool CitizenNotesAgentHostClient::MayReadLocalFiles() {
  return true;
}

bool CitizenNotesAgentHostClient::MayWriteLocalFiles() {
  return true;
}

bool CitizenNotesAgentHostClient::UsesBinaryProtocol() {
  return false;
}

// Only clients that already have powers of local code execution should override
// this to true.
bool CitizenNotesAgentHostClient::AllowUnsafeOperations() {
  return false;
}

absl::optional<url::Origin>
CitizenNotesAgentHostClient::GetNavigationInitiatorOrigin() {
  return absl::nullopt;
}

std::string CitizenNotesAgentHostClient::GetTypeForMetrics() {
  return "Other";
}

}  // namespace content
