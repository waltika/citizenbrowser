// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_MOJOM_CITIZENNOTES_AGENT_HOST_H_
#define CONTENT_BROWSER_CITIZENNOTES_MOJOM_CITIZENNOTES_AGENT_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/public/browser/citizennotes_agent_host.h"

namespace content {

class MojomCitizenNotesAgentHostDelegate;

// Agent host that implements a basic blink::mojom::CitizenNotesAgent flow using a
// delegate.
class MojomCitizenNotesAgentHost : public CitizenNotesAgentHostImpl {
 public:
  static void GetAll(CitizenNotesAgentHost::List* out_list);

  MojomCitizenNotesAgentHost(
      const std::string& id,
      std::unique_ptr<MojomCitizenNotesAgentHostDelegate> delegate);

  MojomCitizenNotesAgentHost(const MojomCitizenNotesAgentHost&) = delete;
  MojomCitizenNotesAgentHost& operator=(const MojomCitizenNotesAgentHost&) = delete;

 private:
  ~MojomCitizenNotesAgentHost() override;

  // CitizenNotesAgentHostImpl overrides:
  bool AttachSession(CitizenNotesSession* session, bool acquire_wake_lock) override;

  // CitizenNotesAgentHost overrides:
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  void Reload() override;

  mojo::AssociatedRemote<blink::mojom::CitizenNotesAgent> associated_agent_remote_;
  std::unique_ptr<MojomCitizenNotesAgentHostDelegate> delegate_;

  static std::vector<std::string>& host_ids();
};
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_MOJOM_CITIZENNOTES_AGENT_HOST_H_
