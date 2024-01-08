// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_citizennotes_controller.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr_set.h>
#include <lib/sys/cpp/component_context.h>
#include <vector>

#include <optional>
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/citizennotes_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/webengine/switches.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/socket/tcp_server_socket.h"

namespace {

using OnCitizenNotesPortChanged = base::OnceCallback<void(uint16_t)>;

class CitizenNotesSocketFactory : public content::CitizenNotesSocketFactory {
 public:
  CitizenNotesSocketFactory(OnCitizenNotesPortChanged on_citizennotes_port,
                        net::IPEndPoint ip_end_point)
      : on_citizennotes_port_(std::move(on_citizennotes_port)),
        ip_end_point_(std::move(ip_end_point)) {}
  ~CitizenNotesSocketFactory() override = default;

  CitizenNotesSocketFactory(const CitizenNotesSocketFactory&) = delete;
  CitizenNotesSocketFactory& operator=(const CitizenNotesSocketFactory&) = delete;

  // content::CitizenNotesSocketFactory implementation.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    const int kTcpListenBackLog = 5;
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    int error = socket->Listen(ip_end_point_, kTcpListenBackLog,
                               /*ipv6_only=*/std::nullopt);
    if (error != net::OK) {
      LOG(WARNING) << "Failed to start the HTTP debugger service. "
                   << net::ErrorToString(error);
      std::move(on_citizennotes_port_).Run(0);
      return nullptr;
    }

    net::IPEndPoint end_point;
    socket->GetLocalAddress(&end_point);
    std::move(on_citizennotes_port_).Run(end_point.port());
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

 private:
  OnCitizenNotesPortChanged on_citizennotes_port_;
  net::IPEndPoint ip_end_point_;
};

void StartCitizenNotesServer(OnCitizenNotesPortChanged on_citizennotes_port,
                                net::IPEndPoint ip_end_point) {
  const base::FilePath kDisableActivePortOutputDirectory{};
  const base::FilePath kDisableDebugOutput{};

  content::CitizenNotesAgentHost::StartCitizenNotesServer(
      std::make_unique<CitizenNotesSocketFactory>(std::move(on_citizennotes_port),
                                              ip_end_point),
      kDisableActivePortOutputDirectory, kDisableDebugOutput);
}

class NoopController : public WebEngineCitizenNotesController {
 public:
  NoopController() = default;
  ~NoopController() override = default;

  NoopController(const NoopController&) = delete;
  NoopController& operator=(const NoopController&) = delete;

  // WebEngineCitizenNotesController implementation:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return !user_debugging;
  }
  void OnFrameLoaded(content::WebContents* contents) override {}
  void OnFrameDestroyed(content::WebContents* contents) override {}
  content::CitizenNotesAgentHost::List CitizenNotesTargets() override {
    return {};
  }
  void GetCitizenNotesPort(base::OnceCallback<void(uint16_t)> callback) override {
    std::move(callback).Run(0);
  }
};

// "User-mode" makes CitizenNotes accessible to remote devices for Frames specified
// by the web_instance owner. The controller, which starts CitizenNotes when the
// first Frame is created, and shuts it down when no debuggable Frames remain.
class UserModeController : public WebEngineCitizenNotesController {
 public:
  explicit UserModeController(uint16_t server_port)
      : ip_endpoint_(net::IPAddress::IPv6AllZeros(), server_port) {}
  ~UserModeController() override {
    if (is_remote_debugging_started_) {
      content::CitizenNotesAgentHost::StopCitizenNotesServer();
    }
  }

  UserModeController(const UserModeController&) = delete;
  UserModeController& operator=(const UserModeController&) = delete;

  // WebEngineCitizenNotesController implementation:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    if (user_debugging) {
      debuggable_contents_.insert(contents);
      UpdateCitizenNotesServer();
    }
    return true;
  }
  void OnFrameLoaded(content::WebContents* contents) override {}
  void OnFrameDestroyed(content::WebContents* contents) override {
    debuggable_contents_.erase(contents);
    UpdateCitizenNotesServer();
  }
  content::CitizenNotesAgentHost::List CitizenNotesTargets() override {
    DCHECK(is_remote_debugging_started_);

    content::CitizenNotesAgentHost::List enabled_hosts;
    for (content::WebContents* contents : debuggable_contents_) {
      enabled_hosts.push_back(
          content::CitizenNotesAgentHost::GetOrCreateFor(contents));
    }
    return enabled_hosts;
  }
  void GetCitizenNotesPort(base::OnceCallback<void(uint16_t)> callback) override {
    get_port_callbacks_.emplace_back(std::move(callback));
    MaybeNotifyGetPortCallbacks();
  }

 private:
  // Starts or stops the remote debugging server, if necessary
  void UpdateCitizenNotesServer() {
    bool need_remote_debugging = !debuggable_contents_.empty();
    if (need_remote_debugging == is_remote_debugging_started_)
      return;
    is_remote_debugging_started_ = need_remote_debugging;

    if (need_remote_debugging) {
      StartCitizenNotesServer(
          base::BindOnce(&UserModeController::OnCitizenNotesPortChanged,
                         base::Unretained(this)),
          ip_endpoint_);
    } else {
      content::CitizenNotesAgentHost::StopCitizenNotesServer();
      citizennotes_port_.reset();
    }
  }

  void OnCitizenNotesPortChanged(uint16_t port) {
    citizennotes_port_ = port;
    MaybeNotifyGetPortCallbacks();
  }

  void MaybeNotifyGetPortCallbacks() {
    if (!citizennotes_port_.has_value())
      return;
    for (auto& callback : get_port_callbacks_)
      std::move(callback).Run(citizennotes_port_.value());
    get_port_callbacks_.clear();
  }

  const net::IPEndPoint ip_endpoint_;

  // True if the remote debugging server is started.
  bool is_remote_debugging_started_ = false;

  // Currently active CitizenNotes port. Set to 0 on service startup error.
  std::optional<uint16_t> citizennotes_port_;

  // Set of Frames' content::WebContents which are remotely debuggable.
  base::flat_set<content::WebContents*> debuggable_contents_;

  std::vector<base::OnceCallback<void(uint16_t)>> get_port_callbacks_;
};

// "Debug-mode" is used for on-device testing, and makes all Frames available
// for debugging by clients on the same device. CitizenNotes is only reported when
// the first Frame finishes loading its main document, so that the
// CitizenNotesPerContextListeners can start interacting with it immediately.
class DebugModeController : public WebEngineCitizenNotesController,
                            public fuchsia::web::Debug {
 public:
  DebugModeController()
      : DebugModeController(
            net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)) {}
  ~DebugModeController() override {
    content::CitizenNotesAgentHost::StopCitizenNotesServer();
  }

  DebugModeController(const DebugModeController&) = delete;
  DebugModeController& operator=(const DebugModeController&) = delete;

  // CitizenNotesController implementation:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return !user_debugging;
  }
  void OnFrameLoaded(content::WebContents* contents) override {
    if (!frame_loaded_) {
      frame_loaded_ = true;
      MaybeSendCitizenNotesCallbacks();
    }
  }
  void OnFrameDestroyed(content::WebContents* contents) override {}
  content::CitizenNotesAgentHost::List CitizenNotesTargets() override {
    return content::CitizenNotesAgentHost::GetOrCreateAll();
  }
  void GetCitizenNotesPort(base::OnceCallback<void(uint16_t)> callback) override {
    std::move(callback).Run(0);
  }

 protected:
  explicit DebugModeController(net::IPEndPoint ip_endpoint)
      : ip_endpoint_(std::move(ip_endpoint)),
        binding_(base::ComponentContextForProcess()->outgoing().get(), this) {
    // Immediately start the service.
    StartCitizenNotesServer(
        base::BindOnce(&DebugModeController::OnCitizenNotesPortChanged,
                       base::Unretained(this)),
        ip_endpoint_);
  }

  virtual void OnCitizenNotesPortChanged(uint16_t port) {
    citizennotes_port_ = port;
    MaybeSendCitizenNotesCallbacks();
  }

  // Currently active CitizenNotes port. Set to 0 on service startup error.
  std::optional<uint16_t> citizennotes_port_;

 private:
  // fuchsia::web::Debug implementation.
  void EnableCitizenNotes(
      fidl::InterfaceHandle<fuchsia::web::CitizenNotesListener> listener_handle,
      EnableCitizenNotesCallback callback) override {
    // Each web-instance has a single CitizenNotes "context", so create a new
    // per-context channel, and pass it to |listener| immediately.
    fuchsia::web::CitizenNotesPerContextListenerPtr context_listener;
    auto listener = listener_handle.Bind();
    listener->OnContextCitizenNotesAvailable(context_listener.NewRequest());

    // Notify the per-context listener immediately, if the port is ready.
    if (frame_loaded_ && citizennotes_port_)
      context_listener->OnHttpPortOpen(citizennotes_port_.value());

    citizennotes_listeners_.AddInterfacePtr(std::move(context_listener));
  }

  void MaybeSendCitizenNotesCallbacks() {
    if (!frame_loaded_ || !citizennotes_port_)
      return;

    // If |citizennotes_port_| is zero then CitizenNotes failed to initialize, and
    // all listener connections should be closed to indicate failure.
    if (citizennotes_port_.value() == 0) {
      citizennotes_listeners_.CloseAll();
      return;
    }

    for (const auto& listener : citizennotes_listeners_.ptrs()) {
      listener->get()->OnHttpPortOpen(citizennotes_port_.value());
    }
  }

  const net::IPEndPoint ip_endpoint_;

  bool frame_loaded_ = false;

  fidl::InterfacePtrSet<fuchsia::web::CitizenNotesPerContextListener>
      citizennotes_listeners_;

  const base::ScopedServiceBinding<fuchsia::web::Debug> binding_;
};

// "Mixed-mode" is used when both user and debug remote debugging are active at
// the same time. The service is enabled for the lifespan of the web_instance.
class MixedModeController : public DebugModeController {
 public:
  explicit MixedModeController(uint16_t server_port)
      : DebugModeController(
            net::IPEndPoint(net::IPAddress::IPv6AllZeros(), server_port)) {}
  ~MixedModeController() override = default;

  // WebEngineCitizenNotesController overrides:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return true;
  }
  void GetCitizenNotesPort(base::OnceCallback<void(uint16_t)> callback) override {
    get_port_callbacks_.emplace_back(std::move(callback));
    MaybeNotifyGetPortCallbacks();
  }

  // DebugModeController overrides:
  void OnCitizenNotesPortChanged(uint16_t port) override {
    DebugModeController::OnCitizenNotesPortChanged(port);
    MaybeNotifyGetPortCallbacks();
  }

  void MaybeNotifyGetPortCallbacks() {
    if (!citizennotes_port_)
      return;
    for (auto& callback : get_port_callbacks_)
      std::move(callback).Run(citizennotes_port_.value());
    get_port_callbacks_.clear();
  }

  std::vector<base::OnceCallback<void(uint16_t)>> get_port_callbacks_;
};

}  //  namespace

// static
std::unique_ptr<WebEngineCitizenNotesController>
WebEngineCitizenNotesController::CreateFromCommandLine(
    const base::CommandLine& command_line) {
  std::optional<uint16_t> citizennotes_port;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    // Set up CitizenNotes to listen on all network routes on the command-line
    // provided port.
    std::string command_line_port_value =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int parsed_port = 0;

    // The command-line option can only be provided by the ContextProvider
    // process, it should not fail to parse to an int.
    bool parsed = base::StringToInt(command_line_port_value, &parsed_port);
    DCHECK(parsed);

    if (parsed_port != 0 &&
        (!net::IsPortValid(parsed_port) || net::IsWellKnownPort(parsed_port))) {
      LOG(WARNING) << "Invalid HTTP debugger service port number "
                   << command_line_port_value;
    } else {
      citizennotes_port = parsed_port;
    }
  }

  bool enable_debug_mode =
      command_line.HasSwitch(switches::kEnableRemoteDebugMode);
  if (citizennotes_port) {
    if (enable_debug_mode) {
      return std::make_unique<MixedModeController>(citizennotes_port.value());
    } else {
      return std::make_unique<UserModeController>(citizennotes_port.value());
    }
  } else if (enable_debug_mode) {
    return std::make_unique<DebugModeController>();
  } else {
    return std::make_unique<NoopController>();
  }
}
