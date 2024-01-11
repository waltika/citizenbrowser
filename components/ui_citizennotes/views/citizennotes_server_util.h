// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_VIEWS_CITIZENNOTES_SERVER_UTIL_H_
#define COMPONENTS_UI_CITIZENNOTES_VIEWS_CITIZENNOTES_SERVER_UTIL_H_

#include <memory>

#include "components/ui_citizennotes/connector_delegate.h"
#include "components/ui_citizennotes/citizennotes_server.h"

namespace ui_citizennotes {

// A factory helper to construct a UiCitizenNotesServer for Views.
// The connector is used in TracingAgent to hook up with the tracing service.
std::unique_ptr<UiCitizenNotesServer> CreateUiCitizenNotesServerForViews(
    network::mojom::NetworkContext* network_context,
    std::unique_ptr<ConnectorDelegate> connector,
    const base::FilePath& active_port_output_directory);

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_VIEWS_CITIZENNOTES_SERVER_UTIL_H_
