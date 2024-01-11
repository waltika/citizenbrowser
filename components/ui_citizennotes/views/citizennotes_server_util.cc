// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_citizennotes/views/citizennotes_server_util.h"

#include <memory>
#include <utility>

#include "components/ui_citizennotes/css_agent.h"
#include "components/ui_citizennotes/citizennotes_server.h"
#include "components/ui_citizennotes/page_agent.h"
#include "components/ui_citizennotes/switches.h"
#include "components/ui_citizennotes/tracing_agent.h"
#include "components/ui_citizennotes/views/dom_agent_views.h"
#include "components/ui_citizennotes/views/overlay_agent_views.h"
#include "components/ui_citizennotes/views/page_agent_views.h"

namespace ui_citizennotes {

std::unique_ptr<UiCitizenNotesServer> CreateUiCitizenNotesServerForViews(
    network::mojom::NetworkContext* network_context,
    std::unique_ptr<ConnectorDelegate> connector,
    const base::FilePath& active_port_output_directory) {
  constexpr int kUiCitizenNotesDefaultPort = 9223;
  int port = UiCitizenNotesServer::GetUiCitizenNotesPort(switches::kEnableUiCitizenNotes,
                                                 kUiCitizenNotesDefaultPort);
  auto server = UiCitizenNotesServer::CreateForViews(network_context, port,
                                                 active_port_output_directory);
  DCHECK(server);
  auto client =
      std::make_unique<UiCitizenNotesClient>("UiCitizenNotesClient", server.get());
  auto dom_agent_views = DOMAgentViews::Create();
  auto* dom_agent_views_ptr = dom_agent_views.get();
  client->AddAgent(std::make_unique<PageAgentViews>(dom_agent_views_ptr));
  client->AddAgent(std::move(dom_agent_views));
  client->AddAgent(std::make_unique<CSSAgent>(dom_agent_views_ptr));
  client->AddAgent(OverlayAgentViews::Create(dom_agent_views_ptr));
  auto tracing_agent = std::make_unique<TracingAgent>(std::move(connector));
  server->set_tracing_agent(tracing_agent.get());
  client->AddAgent(std::move(tracing_agent));
  server->AttachClient(std::move(client));
  return server;
}

}  // namespace ui_citizennotes
