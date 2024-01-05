// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_SESSION_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_SESSION_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <type_traits>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/browser/citizen_x/protocol/protocol.h"
#include "content/public/browser/citizennotes_agent_host_client_channel.h"
#include "content/public/browser/citizennotes_external_agent_proxy.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_agent.mojom.h"

namespace content {

class CitizenNotesAgentHostClient;
class CitizenNotesAgentHostImpl;
class CitizenNotesExternalAgentProxyDelegate;

namespace protocol {
class CitizenNotesDomainHandler;
class CNAuditsHandler;
class CNDOMHandler;
class CNEmulationHandler;
class CNInputHandler;
class CNInspectorHandler;
class CNIOHandler;
class CNOverlayHandler;
class CNNetworkHandler;
class CNFetchHandler;
class CNStorageHandler;
class CNTargetHandler;
class CNPageHandler;
class CNTracingHandler;
class CNLogHandler;
class CNWebAuthnHandler;
}

class CitizenNotesSession : public protocol::FrontendChannel,
                        public blink::mojom::CitizenNotesSessionHost,
                        public CitizenNotesExternalAgentProxy,
                        public content::CitizenNotesAgentHostClientChannel {
 public:
  // For sessions attached to the Tab target, the mode is set to TabTarget.
  // For other sessions, the mode is inherited from the parent.
  // This is used as an indication that the client has opted in into using
  // tab targets and lets backend use the new logic of target discovery:
  //  - auto-attach on frame targets will not auto-attach subframes (these
  //      will be instead attached to the Tab targets)
  //  - certain features that depend on MPArch support will not be disabled
  //      (BFCache, Prerender)
  enum class Mode {
    kSupportsTabTarget,
    kDoesNotSupportTabTarget,
  };

  class ChildObserver : public base::CheckedObserver {
   public:
    virtual void SessionAttached(CitizenNotesSession& session) = 0;
  };

  // For root sessions (see also private constructor for children).
  CitizenNotesSession(CitizenNotesAgentHostClient* client, Mode mode);
  ~CitizenNotesSession() override;

  void SetAgentHost(CitizenNotesAgentHostImpl* agent_host);
  void SetRuntimeResumeCallback(base::OnceClosure runtime_resume);
  bool IsWaitingForDebuggerOnStart() const;
  void Dispose();

  // content::CitizenNotesAgentHostClientChannel implementation.
  content::CitizenNotesAgentHost* GetAgentHost() override;
  content::CitizenNotesAgentHostClient* GetClient() override;

  CitizenNotesSession* GetRootSession();

  // Browser-only sessions do not talk to mojom::CitizenNotesAgent, but instead
  // handle all protocol messages locally in the browser process.
  void SetBrowserOnly(bool browser_only);
  template <typename Handler, typename... Args>
  Handler* CreateAndAddHandler(Args&&... args) {
    if (!IsDomainAvailableToUntrustedClient<Handler>() &&
        !client_->IsTrusted()) {
      return nullptr;
    }
    auto handler = std::make_unique<Handler>(std::forward<Args>(args)...);
    Handler* handler_ptr = handler.get();
    AddHandler(std::move(handler));
    return handler_ptr;
  }

  void TurnIntoExternalProxy(CitizenNotesExternalAgentProxyDelegate* delegate);

  void AttachToAgent(blink::mojom::CitizenNotesAgent* agent,
                     bool force_using_io_session);
  void DispatchProtocolMessage(base::span<const uint8_t> message);
  void SuspendSendingMessagesToAgent();
  void ResumeSendingMessagesToAgent();
  void ClearPendingMessages(bool did_crash);

  using HandlersMap =
      base::flat_map<std::string,
                     std::unique_ptr<protocol::CitizenNotesDomainHandler>>;
  HandlersMap& handlers() { return handlers_; }

  CitizenNotesSession* AttachChildSession(const std::string& session_id,
                                      CitizenNotesAgentHostImpl* agent_host,
                                      CitizenNotesAgentHostClient* client,
                                      Mode mode,
                                      base::OnceClosure resume_callback);
  void DetachChildSession(const std::string& session_id);
  bool HasChildSession(const std::string& session_id);
  Mode session_mode() const { return mode_; }

  void AddObserver(ChildObserver* obs);
  void RemoveObserver(ChildObserver* obs);

 private:
  struct PendingMessage {
    int call_id;
    std::string method;
    std::vector<uint8_t> payload;

    PendingMessage() = delete;
    PendingMessage(const PendingMessage&) = delete;
    PendingMessage& operator=(const PendingMessage&) = delete;

    PendingMessage(PendingMessage&&);
    PendingMessage(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> payload);
    ~PendingMessage();
  };

  // For child sessions.
  CitizenNotesSession(CitizenNotesAgentHostClient* client,
                  const std::string& session_id,
                  CitizenNotesSession* parent,
                  Mode mode);

  void MojoConnectionDestroyed();
  void DispatchToAgent(const PendingMessage& message);
  void HandleCommand(base::span<const uint8_t> message);
  void HandleCommandInternal(crdtp::Dispatchable dispatchable,
                             base::span<const uint8_t> message);
  void DispatchProtocolMessageInternal(crdtp::Dispatchable dispatchable,
                                       base::span<const uint8_t> message);

  // protocol::FrontendChannel implementation.
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FlushProtocolNotifications() override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  // content::CitizenNotesAgentHostClientChannel implementation.
  void DispatchProtocolMessageToClient(std::vector<uint8_t> message) override;

  // blink::mojom::CitizenNotesSessionHost implementation.
  void DispatchProtocolResponse(
      blink::mojom::CitizenNotesMessagePtr message,
      int call_id,
      blink::mojom::CitizenNotesSessionStatePtr updates) override;
  void DispatchProtocolNotification(
      blink::mojom::CitizenNotesMessagePtr message,
      blink::mojom::CitizenNotesSessionStatePtr updates) override;

  // CitizenNotesExternalAgentProxy implementation.
  void DispatchOnClientHost(base::span<const uint8_t> message) override;
  void ConnectionClosed() override;

  // Merges the |updates| received from the renderer into session_state_cookie_.
  void ApplySessionStateUpdates(blink::mojom::CitizenNotesSessionStatePtr updates);

  template <typename T>
  bool IsDomainAvailableToUntrustedClient() {
    return std::disjunction_v<std::is_same<T, protocol::CNAuditsHandler>,
                              std::is_same<T, protocol::CNDOMHandler>,
                              std::is_same<T, protocol::CNEmulationHandler>,
                              std::is_same<T, protocol::CNInputHandler>,
                              std::is_same<T, protocol::CNInspectorHandler>,
                              std::is_same<T, protocol::CNIOHandler>,
                              std::is_same<T, protocol::CNOverlayHandler>,
                              std::is_same<T, protocol::CNNetworkHandler>,
                              std::is_same<T, protocol::CNFetchHandler>,
                              std::is_same<T, protocol::CNStorageHandler>,
                              std::is_same<T, protocol::CNTargetHandler>,
                              std::is_same<T, protocol::CNPageHandler>,
                              std::is_same<T, protocol::CNTracingHandler>,
                              std::is_same<T, protocol::CNLogHandler>,
                              std::is_same<T, protocol::CNWebAuthnHandler>>;
  }
  void AddHandler(std::unique_ptr<protocol::CitizenNotesDomainHandler> handler);

  RAW_PTR_EXCLUSION CitizenNotesAgentHostClient* const client_;
  RAW_PTR_EXCLUSION CitizenNotesSession* const root_session_ = nullptr;
  const std::string session_id_;  // empty if this is the root session.
  const Mode mode_;

  mojo::AssociatedReceiver<blink::mojom::CitizenNotesSessionHost> receiver_{this};
  mojo::AssociatedRemote<blink::mojom::CitizenNotesSession> session_;
  mojo::Remote<blink::mojom::CitizenNotesSession> io_session_;
  bool use_io_session_{false};
  RAW_PTR_EXCLUSION CitizenNotesAgentHostImpl* agent_host_ = nullptr;
  bool browser_only_ = false;
  HandlersMap handlers_;
  std::unique_ptr<protocol::UberDispatcher> dispatcher_{
      new protocol::UberDispatcher(this)};

  bool suspended_sending_messages_to_agent_ = false;

  // Messages that were sent to the agent or queued after suspending.
  std::list<PendingMessage> pending_messages_;
  // Pending messages that were sent and are thus waiting for a response.
  base::flat_map<int, std::list<PendingMessage>::iterator>
      waiting_for_response_;

  // |session_state_cookie_| always corresponds to a state before
  // any of the waiting for response messages have been handled.
  // |session_state_cookie_| is nullptr before first attach.
  blink::mojom::CitizenNotesSessionStatePtr session_state_cookie_;

  base::flat_map<std::string, CitizenNotesSession*> child_sessions_;
  base::OnceClosure runtime_resume_;
  RAW_PTR_EXCLUSION CitizenNotesExternalAgentProxyDelegate* proxy_delegate_ = nullptr;
  base::ObserverList<ChildObserver, true, false> child_observers_;

  base::WeakPtrFactory<CitizenNotesSession> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_SESSION_H_
