// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_CITIZENNOTES_SERVER_H_
#define COMPONENTS_UI_CITIZENNOTES_CITIZENNOTES_SERVER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/ui_citizennotes/citizennotes_client.h"
#include "components/ui_citizennotes/citizennotes_export.h"
#include "components/ui_citizennotes/dom.h"
#include "components/ui_citizennotes/forward.h"
#include "components/ui_citizennotes/protocol.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/server/http_server.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ui_citizennotes {

class TracingAgent;

class UI_CITIZENNOTES_EXPORT UiCitizenNotesServer
    : public network::server::HttpServer::Delegate {
 public:
  // Network tags to be used for the UI citizennotes servers.
  static const net::NetworkTrafficAnnotationTag kUiCitizenNotesServerTag;

  UiCitizenNotesServer(const UiCitizenNotesServer&) = delete;
  UiCitizenNotesServer& operator=(const UiCitizenNotesServer&) = delete;

  ~UiCitizenNotesServer() override;

  // Returns an empty unique_ptr if ui citizennotes flag isn't enabled or if a
  // server instance has already been created. If |port| is 0, the server will
  // choose an available port. If |port| is 0 and |active_port_output_directory|
  // is present, the server will write the chosen port to
  // |kUICitizenNotesActivePortFileName| on |active_port_output_directory|.
  static std::unique_ptr<UiCitizenNotesServer> CreateForViews(
      network::mojom::NetworkContext* network_context,
      int port,
      const base::FilePath& active_port_output_directory = base::FilePath());

  // Creates a TCPServerSocket to be used by a UiCitizenNotesServer.
  static void CreateTCPServerSocket(
      mojo::PendingReceiver<network::mojom::TCPServerSocket>
          server_socket_receiver,
      network::mojom::NetworkContext* network_context,
      int port,
      net::NetworkTrafficAnnotationTag tag,
      network::mojom::NetworkContext::CreateTCPServerSocketCallback callback);

  // Returns a list of attached UiCitizenNotesClient name + URL
  using NameUrlPair = std::pair<std::string, std::string>;
  static std::vector<NameUrlPair> GetClientNamesAndUrls();

  // Returns true if UI Citizennotes is enabled by the given commandline switch.
  static bool IsUiCitizenNotesEnabled(const char* enable_citizennotes_flag);

  // Returns the port number specified by a command line flag. If a number is
  // not specified as a command line argument, returns the |default_port|.
  static int GetUiCitizenNotesPort(const char* enable_citizennotes_flag,
                               int default_port);

  void AttachClient(std::unique_ptr<UiCitizenNotesClient> client);
  void SendOverWebSocket(int connection_id, base::StringPiece message);

  int port() const { return port_; }

  TracingAgent* tracing_agent() { return tracing_agent_; }
  void set_tracing_agent(TracingAgent* agent) { tracing_agent_ = agent; }

  // Sets the callback which will be invoked when the session is closed.
  // Marks as a const function so it can be called after the server is set up
  // and used as a constant instance.
  void SetOnSessionEnded(base::OnceClosure callback) const;
  // Sets a callback that tests can use to wait for the server to be ready to
  // accept connections.
  void SetOnSocketConnectedForTesting(base::OnceClosure on_socket_connected);

 private:
  UiCitizenNotesServer(int port,
                   const net::NetworkTrafficAnnotationTag tag,
                   const base::FilePath& active_port_output_directory);

  void MakeServer(
      mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket,
      int result,
      const absl::optional<net::IPEndPoint>& local_addr);

  // HttpServer::Delegate
  void OnConnect(int connection_id) override;
  void OnHttpRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  using ClientsList = std::vector<std::unique_ptr<UiCitizenNotesClient>>;
  using ConnectionsMap = std::map<uint32_t, UiCitizenNotesClient*>;
  ClientsList clients_;
  ConnectionsMap connections_;

  std::unique_ptr<network::server::HttpServer> server_;

  // The port the citizennotes server listens on
  int port_;

  // Output directory for |kUICitizenNotesActivePortFileName| when
  // --enable-ui-citizennotes=0.
  base::FilePath active_port_output_directory_;

  const net::NetworkTrafficAnnotationTag tag_;

  RAW_PTR_EXCLUSION TracingAgent* tracing_agent_ = nullptr;

  // Invoked when the server doesn't have any live connection.
  mutable base::OnceClosure on_session_ended_;
  base::OnceClosure on_socket_connected_;

  // The server (owned by Chrome for now)
  static UiCitizenNotesServer* citizennotes_server_;

  SEQUENCE_CHECKER(citizennotes_server_sequence_);
  base::WeakPtrFactory<UiCitizenNotesServer> weak_ptr_factory_{this};
};

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_CITIZENNOTES_SERVER_H_
