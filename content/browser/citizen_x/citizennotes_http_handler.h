// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_HTTP_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_HTTP_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Thread;
}

namespace content {
class CitizenNotesManagerDelegate;
class CitizenNotesSocketFactory;
}

namespace net {
class IPEndPoint;
class HttpServerRequestInfo;
}

namespace content {

class CitizenNotesAgentHostClientImpl;
class CNServerWrapper;

// This class is used for managing CitizenNotes remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class CitizenNotesHttpHandler {
 public:
  // Takes ownership over |socket_factory|.
  // |delegate| is only accessed on UI thread.
  // If |active_port_output_directory| is non-empty, it is assumed the
  // socket_factory was initialized with an ephemeral port (0). The
  // port selected by the OS will be written to a well-known file in
  // the output directory.
  CitizenNotesHttpHandler(
      CitizenNotesManagerDelegate* delegate,
      std::unique_ptr<CitizenNotesSocketFactory> server_socket_factory,
      const base::FilePath& active_port_output_directory,
      const base::FilePath& debug_frontend_dir);

  CitizenNotesHttpHandler(const CitizenNotesHttpHandler&) = delete;
  CitizenNotesHttpHandler& operator=(const CitizenNotesHttpHandler&) = delete;

  ~CitizenNotesHttpHandler();

 private:
  friend class CNServerWrapper;
  friend void ServerStartedOnUI(
      base::WeakPtr<CitizenNotesHttpHandler> handler,
      base::Thread* thread,
      CNServerWrapper* server_wrapper,
      CitizenNotesSocketFactory* socket_factory,
      std::unique_ptr<net::IPEndPoint> ip_address);

  void OnJsonRequest(int connection_id,
                     const net::HttpServerRequestInfo& info);
  void RespondToJsonList(int connection_id,
                         const std::string& host,
                         CitizenNotesAgentHost::List agent_hosts,
                         bool for_tab);
  void OnDiscoveryPageRequest(int connection_id);
  void OnFrontendResourceRequest(int connection_id, const std::string& path);
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info);
  void OnWebSocketMessage(int connection_id, std::string data);
  void OnClose(int connection_id);

  void ServerStarted(std::unique_ptr<base::Thread> thread,
                     std::unique_ptr<CNServerWrapper> server_wrapper,
                     std::unique_ptr<CitizenNotesSocketFactory> socket_factory,
                     std::unique_ptr<net::IPEndPoint> ip_address);

  void SendJson(int connection_id,
                net::HttpStatusCode status_code,
                absl::optional<base::ValueView> value,
                const std::string& message);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send403(int connection_id, const std::string& message);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);

  void DecompressAndSendJsonProtocol(int connection_id);

  // Returns the front end url without the host at the beginning.
  std::string GetFrontendURLInternal(
      scoped_refptr<CitizenNotesAgentHost> agent_host,
      const std::string& target_id,
      const std::string& host);

  base::Value::Dict SerializeDescriptor(
      scoped_refptr<CitizenNotesAgentHost> agent_host,
      const std::string& host);

  std::set<std::string> remote_allow_origins_;
  // The thread used by the citizennotes handler to run server socket.
  std::unique_ptr<base::Thread> thread_;
  std::string browser_guid_;
  std::unique_ptr<CNServerWrapper> server_wrapper_;
  std::unique_ptr<net::IPEndPoint> server_ip_address_;
  using ConnectionToClientMap =
      std::map<int, std::unique_ptr<CitizenNotesAgentHostClientImpl>>;
  ConnectionToClientMap connection_to_client_;
  RAW_PTR_EXCLUSION CitizenNotesManagerDelegate* delegate_;
  std::unique_ptr<CitizenNotesSocketFactory> socket_factory_;
  base::WeakPtrFactory<CitizenNotesHttpHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_HTTP_HANDLER_H_
