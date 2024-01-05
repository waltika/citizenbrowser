// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_FORWARDING_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_FORWARDING_AGENT_HOST_H_

#include <memory>

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"

namespace content {

class CitizenNotesExternalAgentProxyDelegate;

class CNForwardingAgentHost : public CitizenNotesAgentHostImpl {
 public:
  CNForwardingAgentHost(
      const std::string& id,
      std::unique_ptr<CitizenNotesExternalAgentProxyDelegate> delegate);

  CNForwardingAgentHost(const CNForwardingAgentHost&) = delete;
  CNForwardingAgentHost& operator=(const CNForwardingAgentHost&) = delete;

 private:
  ~CNForwardingAgentHost() override;

  // CitizenNotesAgentHostImpl overrides.
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;
  void DetachSession(CitizenNotesSession* session) override;

  // CitizenNotesAgentHost implementation.
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;
  std::string GetDescription() override;

  std::unique_ptr<CitizenNotesExternalAgentProxyDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_FORWARDING_AGENT_HOST_H_
