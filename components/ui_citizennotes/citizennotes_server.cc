// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_citizennotes/citizennotes_server.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/ui_citizennotes/switches.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ui_citizennotes {

namespace {
const char kChromeCitizenNotesPrefix[] =
    "citizennotes://citizennotes/bundled/citizennotes_app.html?uiCitizenNotes=true&ws=";

const base::FilePath::CharType kUICitizenNotesActivePortFileName[] =
    FILE_PATH_LITERAL("UICitizenNotesActivePort");

void WriteUICitizennotesPortToFile(base::FilePath output_dir, int port) {
  base::FilePath path = output_dir.Append(kUICitizenNotesActivePortFileName);
  std::string port_target_string = base::StringPrintf("%d", port);
  if (!base::WriteFile(path, port_target_string)) {
    LOG(ERROR) << "Error writing uiCitizenNotes active port to file";
  }
}
}  // namespace

UiCitizenNotesServer* UiCitizenNotesServer::citizennotes_server_ = nullptr;

const net::NetworkTrafficAnnotationTag UiCitizenNotesServer::kUiCitizenNotesServerTag =
    net::DefineNetworkTrafficAnnotation("ui_citizennotes_server", R"(
      semantics {
        sender: "UI Citizennotes Server"
        description:
          "Backend for UI CitizenNotes, to inspect Aura/Views UI."
        trigger:
          "Run with '--enable-ui-citizennotes' switch."
        data: "Debugging data, including any data on the open pages."
        destination: OTHER
        destination_other: "The data can be sent to any destination."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with '--enable-ui-citizennotes' switch."
        policy_exception_justification:
          "Not implemented, only used in Citizennotes and is behind a switch."
      })");

UiCitizenNotesServer::UiCitizenNotesServer(
    int port,
    net::NetworkTrafficAnnotationTag tag,
    const base::FilePath& active_port_output_directory)
    : port_(port),
      active_port_output_directory_(active_port_output_directory),
      tag_(tag) {
  DCHECK(!citizennotes_server_);
  citizennotes_server_ = this;
}

UiCitizenNotesServer::~UiCitizenNotesServer() {
  citizennotes_server_ = nullptr;
}

// static
std::unique_ptr<UiCitizenNotesServer> UiCitizenNotesServer::CreateForViews(
    network::mojom::NetworkContext* network_context,
    int port,
    const base::FilePath& active_port_output_directory) {
  // TODO(mhashmi): Change port if more than one inspectable clients
  auto server = base::WrapUnique(new UiCitizenNotesServer(
      port, kUiCitizenNotesServerTag, active_port_output_directory));
  mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket;
  auto receiver = server_socket.InitWithNewPipeAndPassReceiver();
  CreateTCPServerSocket(std::move(receiver), network_context, port,
                        kUiCitizenNotesServerTag,
                        base::BindOnce(&UiCitizenNotesServer::MakeServer,
                                       server->weak_ptr_factory_.GetWeakPtr(),
                                       std::move(server_socket)));
  return server;
}

void UiCitizenNotesServer::SetOnSocketConnectedForTesting(
    base::OnceClosure on_socket_connected) {
  if (server_) {
    std::move(on_socket_connected).Run();
    return;
  }
  on_socket_connected_ = std::move(on_socket_connected);
}

// static
void UiCitizenNotesServer::CreateTCPServerSocket(
    mojo::PendingReceiver<network::mojom::TCPServerSocket>
        server_socket_receiver,
    network::mojom::NetworkContext* network_context,
    int port,
    net::NetworkTrafficAnnotationTag tag,
    network::mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
  // Create the socket using the address 127.0.0.1 to listen on all interfaces.
  net::IPAddress address(127, 0, 0, 1);
  constexpr int kBacklog = 1;

  auto options = network::mojom::TCPServerSocketOptions::New();
  options->backlog = kBacklog;
  network_context->CreateTCPServerSocket(
      net::IPEndPoint(address, port), std::move(options),
      net::MutableNetworkTrafficAnnotationTag(tag),
      std::move(server_socket_receiver), std::move(callback));
}

// static
std::vector<UiCitizenNotesServer::NameUrlPair>
UiCitizenNotesServer::GetClientNamesAndUrls() {
  std::vector<NameUrlPair> pairs;
  if (!citizennotes_server_)
    return pairs;

  for (ClientsList::size_type i = 0; i != citizennotes_server_->clients_.size();
       i++) {
    pairs.push_back(std::pair<std::string, std::string>(
        citizennotes_server_->clients_[i]->name(),
        base::StringPrintf("%s127.0.0.1:%d/%" PRIuS,
                           kChromeCitizenNotesPrefix,
                           citizennotes_server_->port(), i)));
  }
  return pairs;
}

// static
bool UiCitizenNotesServer::IsUiCitizenNotesEnabled(const char* enable_citizennotes_flag) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      enable_citizennotes_flag);
}

// static
int UiCitizenNotesServer::GetUiCitizenNotesPort(const char* enable_citizennotes_flag,
                                        int default_port) {
  // `enable_citizennotes_flag` is specified only when uiCitizenNotes were started with
  // browser start. If not specified at run time, we should use default port.
  if (!IsUiCitizenNotesEnabled(enable_citizennotes_flag))
    return default_port;

  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          enable_citizennotes_flag);
  int port = 0;
  return base::StringToInt(switch_value, &port) ? port : default_port;
}

void UiCitizenNotesServer::AttachClient(std::unique_ptr<UiCitizenNotesClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(citizennotes_server_sequence_);
  clients_.push_back(std::move(client));
}

void UiCitizenNotesServer::SendOverWebSocket(int connection_id,
                                         base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(citizennotes_server_sequence_);
  server_->SendOverWebSocket(connection_id, message, tag_);
}

void UiCitizenNotesServer::SetOnSessionEnded(base::OnceClosure callback) const {
  on_session_ended_ = std::move(callback);
}

void UiCitizenNotesServer::MakeServer(
    mojo::PendingRemote<network::mojom::TCPServerSocket> server_socket,
    int result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(citizennotes_server_sequence_);
  if (result == net::OK) {
    server_ = std::make_unique<network::server::HttpServer>(
        std::move(server_socket), this);
    // When --enable-ui-citizennotes=0, the browser will pick an available port and
    // write to |kUICitizenNotesActivePortFileName|. The file is useful for other
    // programs such as Telemetry to know which port to listen to.
    if (port_ == 0 && local_addr) {
      port_ = local_addr->port();
      if (!active_port_output_directory_.empty()) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock()},
            base::BindOnce(&WriteUICitizennotesPortToFile,
                           active_port_output_directory_, port_));
      }
    }
  }
  if (on_socket_connected_)
    std::move(on_socket_connected_).Run();
}

// HttpServer::Delegate Implementation
void UiCitizenNotesServer::OnConnect(int connection_id) {
  base::RecordAction(base::UserMetricsAction("ui_citizennotes_Connect"));
}

void UiCitizenNotesServer::OnHttpRequest(
    int connection_id,
    const network::server::HttpServerRequestInfo& info) {
  NOTIMPLEMENTED();
}

void UiCitizenNotesServer::OnWebSocketRequest(
    int connection_id,
    const network::server::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(citizennotes_server_sequence_);
  size_t target_id = 0;
  if (info.path.empty() ||
      !base::StringToSizeT(info.path.substr(1), &target_id) ||
      target_id >= clients_.size()) {
    return;
  }

  UiCitizenNotesClient* client = clients_[target_id].get();
  // Only one user can inspect the client at a time
  if (client->connected())
    return;
  client->set_connection_id(connection_id);
  connections_[connection_id] = client;
  server_->AcceptWebSocket(connection_id, info, tag_);
}

void UiCitizenNotesServer::OnWebSocketMessage(int connection_id, std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(citizennotes_server_sequence_);
  auto it = connections_.find(connection_id);
  DCHECK(it != connections_.end());
  UiCitizenNotesClient* client = it->second;
  client->Dispatch(data);
}

void UiCitizenNotesServer::OnClose(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(citizennotes_server_sequence_);
  auto it = connections_.find(connection_id);
  if (it == connections_.end())
    return;
  UiCitizenNotesClient* client = it->second;
  client->Disconnect();
  connections_.erase(it);

  if (connections_.empty() && on_session_ended_) {
    // The call stack might have callbacks which still have the pointer of
    // `this` and `on_session_ended_` may destroy this. Ensure
    // `on_session_ended_` is called in a dedicated task.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_session_ended_));
  }
}

}  // namespace ui_citizennotes
