// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cntarget_handler.h"

#include <memory>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/citizen_x/browser_citizennotes_agent_host.h"
#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/citizennotes_manager.h"
#include "content/browser/citizen_x/protocol/cntarget_auto_attacher.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/citizen_x/web_contents_citizennotes_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/citizennotes_agent_host_client.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/url_constants.h"

namespace content::protocol {

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kSettingsProxyConfigTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("citizennotes_proxy_config", R"(
      semantics {
        sender: "Proxy Configuration over Citizen Notes"
        description:
          "Used to fetch HTTP/HTTPS/SOCKS5/PAC proxy configuration when "
          "proxy is configured by CitizenNotes. It is equivalent to the one "
          "configured via the --proxy-server command line flag. "
          "When proxy implies automatic configuration, it can send network "
          "requests in the scope of this annotation."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other: "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with '--remote-debugging-*' switches "
          "and does not explicitly send this data over Chrome remote debugging."
        policy_exception_justification:
          "Not implemented, only used in CitizenNotes and is behind a switch."
      })");

static const char kNotAllowedError[] = "Not allowed";
static const char kMethod[] = "method";
static const char kResumeMethod[] = "Runtime.runIfWaitingForDebugger";

static const char kInitializerScript[] = R"(
  (function() {
    const bindingName = "%s";
    const binding = window[bindingName];
    delete window[bindingName];
    if (window.self === window.top) {
      window[bindingName] = {
        onmessage: () => {},
        send: binding
      };
    }
  })();
)";

static const char kTargetNotFound[] = "No target with given id found";

std::unique_ptr<Target::TargetInfo> BuildTargetInfo(
    CitizenNotesAgentHost* agent_host) {
  auto* host = static_cast<CitizenNotesAgentHostImpl*>(agent_host);
  std::unique_ptr<Target::TargetInfo> target_info =
      Target::TargetInfo::Create()
          .SetTargetId(host->GetId())
          .SetTitle(host->GetTitle())
          .SetUrl(host->GetURL().spec())
          .SetType(host->GetType())
          .SetAttached(host->IsAttached())
          .SetCanAccessOpener(host->CanAccessOpener())
          .Build();
  if (!host->GetOpenerId().empty())
    target_info->SetOpenerId(host->GetOpenerId());
  if (!host->GetOpenerFrameId().empty())
    target_info->SetOpenerFrameId(host->GetOpenerFrameId());
  if (host->GetBrowserContext())
    target_info->SetBrowserContextId(host->GetBrowserContext()->UniqueId());
  std::string subtype = host->GetSubtype();
  if (!subtype.empty())
    target_info->SetSubtype(subtype);
  return target_info;
}

static std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return "abnormal";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "crashed";
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "still running";
#if BUILDFLAG(IS_CHROMEOS)
    // Used for the case when oom-killer kills a process on ChromeOS.
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return "oom killed";
#endif
#if BUILDFLAG(IS_ANDROID)
    // On Android processes are spawned from the system Zygote and we do not get
    // the termination status.  We can't know if the termination was a crash or
    // an oom kill for sure: but we can use status of the strong process
    // bindings as a hint.
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      return "oom protected";
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return "failed to launch";
    case base::TERMINATION_STATUS_OOM:
      return "oom";
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
      return "integrity failure";
#endif
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

class BrowserToPageConnector;

class BrowserToPageConnector {
 public:
  class BrowserConnectorHostClient : public CitizenNotesAgentHostClient {
   public:
    BrowserConnectorHostClient(BrowserToPageConnector* connector,
                               CitizenNotesAgentHost* host)
        : connector_(connector) {
      // TODO(dgozman): handle return value of AttachClient.
      host->AttachClient(this);
    }

    BrowserConnectorHostClient(const BrowserConnectorHostClient&) = delete;
    BrowserConnectorHostClient& operator=(const BrowserConnectorHostClient&) =
        delete;

    void DispatchProtocolMessage(CitizenNotesAgentHost* agent_host,
                                 base::span<const uint8_t> message) override {
      connector_->DispatchProtocolMessage(agent_host, message);
    }
    void AgentHostClosed(CitizenNotesAgentHost* agent_host) override {
      connector_->AgentHostClosed(agent_host);
    }

   private:
    RAW_PTR_EXCLUSION BrowserToPageConnector* connector_;
  };

  BrowserToPageConnector(const std::string& binding_name,
                         CitizenNotesAgentHost* page_host)
      : binding_name_(binding_name), page_host_(page_host) {
    browser_host_ = BrowserCitizenNotesAgentHost::CreateForDiscovery();
    browser_host_client_ =
        std::make_unique<BrowserConnectorHostClient>(this, browser_host_.get());
    page_host_client_ =
        std::make_unique<BrowserConnectorHostClient>(this, page_host_.get());

    SendProtocolMessageToPage("Page.enable", base::Value());
    SendProtocolMessageToPage("Runtime.enable", base::Value());

    base::Value::Dict add_binding_params;
    add_binding_params.Set("name", binding_name);
    SendProtocolMessageToPage("Runtime.addBinding",
                              base::Value(std::move(add_binding_params)));

    std::string initializer_script =
        base::StringPrintf(kInitializerScript, binding_name.c_str());

    base::Value::Dict params;
    params.Set("scriptSource", initializer_script);
    SendProtocolMessageToPage("Page.addScriptToEvaluateOnLoad",
                              base::Value(std::move(params)));

    base::Value::Dict evaluate_params;
    evaluate_params.Set("expression", initializer_script);
    SendProtocolMessageToPage("Runtime.evaluate",
                              base::Value(std::move(evaluate_params)));
    GetInstanceMap()[page_host_.get()].reset(this);
  }

  BrowserToPageConnector(const BrowserToPageConnector&) = delete;
  BrowserToPageConnector& operator=(const BrowserToPageConnector&) = delete;

  using BrowserToPageConnectorMap =
      base::flat_map<CitizenNotesAgentHost*,
                     std::unique_ptr<BrowserToPageConnector>>;
  static BrowserToPageConnectorMap& GetInstanceMap() {
    static base::NoDestructor<BrowserToPageConnectorMap> map;
    return *map;
  }

 private:
  void SendProtocolMessageToPage(const char* method, base::Value params) {
    base::Value::Dict message_dict;
    message_dict.Set("id", page_message_id_++);
    message_dict.Set("method", method);
    message_dict.Set("params", std::move(params));
    base::Value message(std::move(message_dict));
    std::string json_message;
    base::JSONWriter::Write(message, &json_message);
    page_host_->DispatchProtocolMessage(
        page_host_client_.get(), base::as_bytes(base::make_span(json_message)));
  }

  void DispatchProtocolMessage(CitizenNotesAgentHost* agent_host,
                               base::span<const uint8_t> message) {
    base::StringPiece message_sp(reinterpret_cast<const char*>(message.data()),
                                 message.size());
    if (agent_host == page_host_.get()) {
      absl::optional<base::Value> value = base::JSONReader::Read(message_sp);
      if (!value || !value->is_dict()) {
        return;
      }

      const base::Value::Dict& dict = value->GetDict();
      // Make sure this is a binding call.
      const std::string* method = dict.FindString("method");
      if (!method || *method != "Runtime.bindingCalled") {
        return;
      }

      const base::Value::Dict* params = dict.FindDict("params");
      if (!params) {
        return;
      }

      const std::string* name = params->FindString("name");
      if (!name || *name != binding_name_) {
        return;
      }

      const std::string* payload = params->FindString("payload");
      if (!payload) {
        return;
      }
      browser_host_->DispatchProtocolMessage(
          browser_host_client_.get(),
          base::as_bytes(base::make_span(*payload)));
      return;
    }
    DCHECK(agent_host == browser_host_.get());

    std::string encoded;
    encoded = base::Base64Encode(message_sp);
    std::string eval_code =
        "try { window." + binding_name_ + ".onmessage(atob(\"";
    std::string eval_suffix = "\")); } catch(e) { console.error(e); }";
    eval_code.reserve(eval_code.size() + encoded.size() + eval_suffix.size());
    eval_code.append(encoded);
    eval_code.append(eval_suffix);

    base::Value::Dict params;
    params.Set("expression", std::move(eval_code));
    SendProtocolMessageToPage("Runtime.evaluate",
                              base::Value(std::move(params)));
  }

  void AgentHostClosed(CitizenNotesAgentHost* agent_host) {
    if (agent_host == browser_host_.get()) {
      page_host_->DetachClient(page_host_client_.get());
    } else {
      DCHECK(agent_host == page_host_.get());
      browser_host_->DetachClient(browser_host_client_.get());
    }
    GetInstanceMap().erase(page_host_.get());
  }

  std::string binding_name_;
  scoped_refptr<CitizenNotesAgentHost> browser_host_;
  scoped_refptr<CitizenNotesAgentHost> page_host_;
  std::unique_ptr<BrowserConnectorHostClient> browser_host_client_;
  std::unique_ptr<BrowserConnectorHostClient> page_host_client_;
  int page_message_id_ = 0;
};

}  // namespace

// Throttle is owned externally by the navigation subsystem.
class CNTargetHandler::Throttle : public content::NavigationThrottle {
 public:
  Throttle(const Throttle&) = delete;
  Throttle& operator=(const Throttle&) = delete;

  ~Throttle() override { CleanupPointers(); }
  CNTargetAutoAttacher* auto_attacher() const { return auto_attacher_; }
  void Clear();
  // content::NavigationThrottle implementation:
  const char* GetNameForLogging() override;

 protected:
  Throttle(base::WeakPtr<protocol::CNTargetHandler> target_handler,
           CNTargetAutoAttacher* auto_attacher,
           content::NavigationHandle* navigation_handle)
      : content::NavigationThrottle(navigation_handle),
        target_handler_(target_handler),
        auto_attacher_(auto_attacher) {
    target_handler->throttles_.insert(this);
  }
  void SetThrottledAgentHost(CitizenNotesAgentHost* agent_host);

  bool is_deferring_ = false;
  scoped_refptr<CitizenNotesAgentHost> agent_host_;
  base::WeakPtr<protocol::CNTargetHandler> target_handler_;

 private:
  void CleanupPointers();
  RAW_PTR_EXCLUSION CNTargetAutoAttacher* auto_attacher_;
};

class CNTargetHandler::ResponseThrottle : public CNTargetHandler::Throttle {
 public:
  ResponseThrottle(base::WeakPtr<protocol::CNTargetHandler> target_handler,
                   CNTargetAutoAttacher* auto_attacher,
                   content::NavigationHandle* navigation_handle)
      : Throttle(target_handler, auto_attacher, navigation_handle) {}
  ~ResponseThrottle() override = default;

 private:
  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillProcessResponse() override { return MaybeThrottle(); }

  ThrottleCheckResult WillFailRequest() override { return MaybeThrottle(); }

  ThrottleCheckResult MaybeThrottle() {
    if (target_handler_ && auto_attacher()) {
      NavigationRequest* request = NavigationRequest::From(navigation_handle());
      const bool wait_for_debugger_on_start =
          target_handler_->ShouldWaitForDebuggerOnStart(request);
      scoped_refptr<RenderFrameCitizenNotesAgentHost> new_host =
          auto_attacher()->HandleNavigation(request,
                                            wait_for_debugger_on_start);
      if (new_host &&
          target_handler_->AutoAttach(auto_attacher(), new_host.get(),
                                      wait_for_debugger_on_start) &&
          wait_for_debugger_on_start) {
        SetThrottledAgentHost(new_host.get());
      } else {
        SetThrottledAgentHost(nullptr);
      }
    }
    is_deferring_ = !!agent_host_;
    return is_deferring_ ? DEFER : PROCEED;
  }
};

class CNTargetHandler::RequestThrottle : public CNTargetHandler::Throttle {
 public:
  RequestThrottle(base::WeakPtr<protocol::CNTargetHandler> target_handler,
                  content::NavigationHandle* navigation_handle,
                  CitizenNotesAgentHost* throttled_agent_host)
      : Throttle(target_handler,
                 target_handler->auto_attacher_,
                 navigation_handle) {
    SetThrottledAgentHost(throttled_agent_host);
  }
  ~RequestThrottle() override = default;

 private:
  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override {
    is_deferring_ = !!agent_host_;
    return is_deferring_ ? DEFER : PROCEED;
  }
};

class CNTargetHandler::Session : public CitizenNotesAgentHostClient {
 public:
  static std::string Attach(CNTargetHandler* handler,
                            CitizenNotesAgentHost* agent_host,
                            bool waiting_for_debugger,
                            bool flatten_protocol) {
    std::string id = base::UnguessableToken::Create().ToString();
    // We don't support or allow the non-flattened protocol when in binary mode.
    // So, we coerce the setting to true, as the non-flattened mode is
    // deprecated anyway.
    if (handler->root_session_->GetClient()->UsesBinaryProtocol())
      flatten_protocol = true;
    Session* session = new Session(handler, agent_host, id, flatten_protocol);
    handler->attached_sessions_[id].reset(session);
    CitizenNotesAgentHostImpl* agent_host_impl =
        static_cast<CitizenNotesAgentHostImpl*>(agent_host);
    if (flatten_protocol) {
      using Mode = CitizenNotesSession::Mode;
      const Mode mode =
          agent_host_impl->GetSessionMode() == Mode::kSupportsTabTarget
              ? Mode::kSupportsTabTarget
              : handler->session_mode_;

      base::OnceClosure resume_callback;
      if (waiting_for_debugger) {
        resume_callback = base::BindOnce(&Session::ResumeIfThrottled,
                                         base::Unretained(session));
      }
      CitizenNotesSession* citizennotes_session =
          handler->root_session_->AttachChildSession(
              id, agent_host_impl, session, mode, std::move(resume_callback));
      session->citizennotes_session_ = citizennotes_session;
    } else {
      agent_host_impl->AttachClient(session);
    }
    handler->frontend_->AttachedToTarget(id, BuildTargetInfo(agent_host),
                                         waiting_for_debugger);
    return id;
  }

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  ~Session() override {
    if (!agent_host_)
      return;
    if (flatten_protocol_)
      handler_->root_session_->DetachChildSession(id_);
    agent_host_->DetachClient(this);
  }

  std::string GetTypeForMetrics() override { return "CitizenNotes"; }

  void Detach(bool host_closed) {
    handler_->frontend_->DetachedFromTarget(id_, agent_host_->GetId());
    if (flatten_protocol_)
      handler_->root_session_->DetachChildSession(id_);
    if (!host_closed)
      agent_host_->DetachClient(this);
    handler_->auto_attached_sessions_.erase(agent_host_.get());
    citizennotes_session_ = nullptr;
    agent_host_ = nullptr;
    handler_->attached_sessions_.erase(id_);
  }

  bool IsWaitingForDebuggerOnStart() const {
    return citizennotes_session_ &&
           citizennotes_session_->IsWaitingForDebuggerOnStart();
  }

  void ResumeSendingMessagesToAgent() const {
    if (citizennotes_session_)
      citizennotes_session_->ResumeSendingMessagesToAgent();
  }

  void SetThrottle(Throttle* throttle) { throttle_ = throttle; }
  void SetWorkerThrottle(
      scoped_refptr<CitizenNotesThrottleHandle> worker_throttle) {
    worker_throttle_ = std::move(worker_throttle);
  }

  void ResumeIfThrottled() {
    if (throttle_)
      throttle_->Clear();
    worker_throttle_.reset();
  }

  void SendMessageToAgentHost(base::span<const uint8_t> message) {
    // This method is only used in the non-flat mode, it's the implementation
    // for Target.SendMessageToTarget. And since the binary mode implies
    // flatten_protocol_ (we force the flag to true), we can assume in this
    // method that |message| is JSON.
    DCHECK(!flatten_protocol_);

    if (throttle_ || worker_throttle_) {
      absl::optional<base::Value> value =
          base::JSONReader::Read(base::StringPiece(
              reinterpret_cast<const char*>(message.data()), message.size()));
      const std::string* method;
      if (value.has_value() && value->is_dict() &&
          (method = value->GetDict().FindString(kMethod)) &&
          *method == kResumeMethod) {
        ResumeIfThrottled();
      }
    }

    agent_host_->DispatchProtocolMessage(this, message);
  }

  bool IsAttachedTo(const std::string& target_id) {
    return agent_host_->GetId() == target_id;
  }

  bool UsesBinaryProtocol() override {
    return handler_->root_session_->GetClient()->UsesBinaryProtocol();
  }

 private:
  friend class CNTargetHandler;

  Session(CNTargetHandler* handler,
          CitizenNotesAgentHost* agent_host,
          const std::string& id,
          bool flatten_protocol)
      : handler_(handler),
        agent_host_(agent_host),
        id_(id),
        flatten_protocol_(flatten_protocol) {}

  // CitizenNotesAgentHostClient implementation.
  void DispatchProtocolMessage(CitizenNotesAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    DCHECK(agent_host == agent_host_.get());
    if (flatten_protocol_) {
      // TODO(johannes): It's not clear that this check is useful, but
      // a similar check has been in the code ever since the flattened protocol
      // was introduced. Try a DCHECK instead and possibly remove the check.
      if (!handler_->root_session_->HasChildSession(id_))
        return;
      GetRootClient()->DispatchProtocolMessage(
          handler_->root_session_->GetAgentHost(), message);
      return;
    }
    // TODO(johannes): For now, We need to copy here because
    // ReceivedMessageFromTarget is generated code and we're using const
    // std::string& for such parameters. Perhaps we should switch this to
    // base::StringPiece?
    std::string message_copy(message.begin(), message.end());
    handler_->frontend_->ReceivedMessageFromTarget(id_, message_copy,
                                                   agent_host_->GetId());
  }

  void AgentHostClosed(CitizenNotesAgentHost* agent_host) override {
    DCHECK(agent_host == agent_host_.get());
    Detach(true);
  }

  bool MayAttachToURL(const GURL& url, bool is_webui) override {
    return GetRootClient()->MayAttachToURL(url, is_webui);
  }

  bool IsTrusted() override { return GetRootClient()->IsTrusted(); }

  bool MayReadLocalFiles() override {
    return GetRootClient()->MayReadLocalFiles();
  }

  bool MayWriteLocalFiles() override {
    return GetRootClient()->MayWriteLocalFiles();
  }

  bool AllowUnsafeOperations() override {
    return GetRootClient()->AllowUnsafeOperations();
  }

  content::CitizenNotesAgentHostClient* GetRootClient() {
    return handler_->root_session_->GetClient();
  }

  RAW_PTR_EXCLUSION CNTargetHandler* handler_;
  scoped_refptr<CitizenNotesAgentHost> agent_host_;
  std::string id_;
  bool flatten_protocol_;
  RAW_PTR_EXCLUSION CitizenNotesSession* citizennotes_session_ = nullptr;
  RAW_PTR_EXCLUSION Throttle* throttle_ = nullptr;
  scoped_refptr<CitizenNotesThrottleHandle> worker_throttle_;
  // This is needed to identify sessions associated with given
  // AutoAttacher to properly support SetAttachedTargetsOfType()
  // for a CNTargetHandler that serves as a client to multiple
  // different TargetAttachers. We don't want a pointer here,
  // because a session may survive the source AutoAttacher.
  uintptr_t auto_attacher_id_ = 0;
};

void CNTargetHandler::Throttle::CleanupPointers() {
  if (target_handler_ && agent_host_) {
    auto it = target_handler_->auto_attached_sessions_.find(agent_host_.get());
    if (it != target_handler_->auto_attached_sessions_.end())
      it->second->SetThrottle(nullptr);
  }
  if (target_handler_) {
    target_handler_->throttles_.erase(this);
    target_handler_ = nullptr;
  }
}

void CNTargetHandler::Throttle::SetThrottledAgentHost(
    CitizenNotesAgentHost* agent_host) {
  agent_host_ = agent_host;
  if (agent_host_) {
    target_handler_->auto_attached_sessions_[agent_host_.get()]->SetThrottle(
        this);
  }
}

const char* CNTargetHandler::Throttle::GetNameForLogging() {
  return "CitizenNotesTargetNavigationThrottle";
}

void CNTargetHandler::Throttle::Clear() {
  CleanupPointers();
  agent_host_ = nullptr;
  auto_attacher_ = nullptr;
  if (is_deferring_) {
    is_deferring_ = false;
    Resume();
    // DO NOT ADD CODE after this. The callback above might have destroyed the
    // NavigationHandle that owns this NavigationThrottle.
  }
}

class CNTargetHandler::TargetFilter {
 public:
  using Filter = std::vector<std::unique_ptr<protocol::Target::FilterEntry>>;

  static std::unique_ptr<TargetFilter> CreateDefault() {
    Filter default_filter;
    // - Exclude `browser`.
    default_filter.push_back(protocol::Target::FilterEntry::Create()
                                 .SetExclude(true)
                                 .SetType(CitizenNotesAgentHost::kTypeBrowser)
                                 .Build());
    // - Exclude `tab`.
    default_filter.push_back(protocol::Target::FilterEntry::Create()
                                 .SetExclude(true)
                                 .SetType(CitizenNotesAgentHost::kTypeTab)
                                 .Build());
    // - Allow everything else.
    default_filter.push_back(protocol::Target::FilterEntry::Create().Build());
    return base::WrapUnique(new TargetFilter(std::move(default_filter)));
  }
  static std::unique_ptr<TargetFilter> Create(Maybe<Filter> filter) {
    if (!filter.has_value()) {
      return CreateDefault();
    }
    return base::WrapUnique(new TargetFilter(std::move(filter.value())));
  }

  bool Match(CitizenNotesAgentHost& host) const { return Match(host.GetType()); }

  bool Match(base::StringPiece type) const {
    for (const auto& entry : entries_) {
      if (!entry->HasType() || entry->GetType("") == type) {
        return !entry->GetExclude(false);
      }
    }
    return false;
  }

 private:
  explicit TargetFilter(Filter entries) : entries_(std::move(entries)) {}

  const Filter entries_;
};

CNTargetHandler::CNTargetHandler(AccessMode access_mode,
                             const std::string& owner_target_id,
                             CNTargetAutoAttacher* auto_attacher,
                             CitizenNotesSession* session)
    : CitizenNotesDomainHandler(Target::Metainfo::domainName),
      access_mode_(access_mode),
      owner_target_id_(owner_target_id),
      session_mode_(session->session_mode()),
      root_session_(session->GetRootSession()),
      auto_attacher_(auto_attacher) {}

CNTargetHandler::~CNTargetHandler() = default;

// static
std::vector<CNTargetHandler*> CNTargetHandler::ForAgentHost(
    CitizenNotesAgentHostImpl* host) {
  return host->HandlersByName<CNTargetHandler>(Target::Metainfo::domainName);
}

void CNTargetHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Target::Frontend>(dispatcher->channel());
  Target::Dispatcher::wire(dispatcher, this);
}

Response CNTargetHandler::Disable() {
  SetAutoAttachInternal(false, false, false, base::DoNothing());
  SetDiscoverTargets(false, {});
  auto_attached_sessions_.clear();
  attached_sessions_.clear();

  CitizenNotesManagerDelegate* delegate =
      CitizenNotesManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Success();

  if (dispose_on_detach_context_ids_.size()) {
    for (auto* context : delegate->GetBrowserContexts()) {
      if (!dispose_on_detach_context_ids_.contains(context->UniqueId()))
        continue;
      delegate->DisposeBrowserContext(context, base::DoNothing());
    }
    dispose_on_detach_context_ids_.clear();
  }
  contexts_with_overridden_proxy_.clear();
  return Response::Success();
}

std::unique_ptr<NavigationThrottle> CNTargetHandler::CreateThrottleForNavigation(
    CNTargetAutoAttacher* auto_attacher,
    NavigationHandle* navigation_handle) {
  DCHECK(auto_attach_ || !auto_attach_related_targets_.empty());
  FrameTreeNode* frame_tree_node =
      NavigationRequest::From(navigation_handle)->frame_tree_node();
  DCHECK(access_mode_ != AccessMode::kBrowser ||
         !auto_attach_related_targets_.empty() || !frame_tree_node->parent());
  // All child frames start navigating with their parent settings applied and
  // are only throttled at response where we know if they require a new host.
  // Note that fenced frames start as remote frames right away and get a RFDTAH
  // of their own, so they require a RequestThrottle rather than a Response one.
  if (!frame_tree_node->IsMainFrame()) {
    return std::make_unique<ResponseThrottle>(weak_factory_.GetWeakPtr(),
                                              auto_attacher, navigation_handle);
  }
  // If we got here for main frame, it must be either browser or tab target.
  DCHECK(auto_attacher == auto_attacher_);
  CitizenNotesAgentHost* host =
      RenderFrameCitizenNotesAgentHost::GetFor(frame_tree_node);
  CNTargetHandler::Session* waiting_session = FindWaitingSession(host);
  if (waiting_session) {
    // RFDTAHs created during auto-attach had no renderer allocated originally,
    // and hence have messages paused, but with navigation we're supposed to
    // have a live host, so we can send messages to renderer now.
    DCHECK(frame_tree_node->current_frame_host()->IsRenderFrameLive());
    // Only resume sending messages to frame agents (i.e. skip for WebContents
    // ones).
    waiting_session->ResumeSendingMessagesToAgent();
  } else {
    // Currently, either RFDTAH or WCDTAH may be waiting for debugger (when
    // `waitForDebuggerOnStart` is honored for the Tab target, it is ignored
    // for the Page target), so in case no Page-level sessions are waiting,
    // also check the tab target.
    host = WebContentsCitizenNotesAgentHost::GetFor(
        WebContentsImpl::FromFrameTreeNode(frame_tree_node));
    waiting_session = FindWaitingSession(host);
    if (!waiting_session) {
      return nullptr;
    }
  }
  // window.open() navigations are throttled on the renderer side and the main
  // request will not be sent until runIfWaitingForDebugger is received from
  // the client, so there is no need to throttle the navigation in the
  // browser.
  //
  // New window navigations (such as ctrl+click) should be throttled before
  // the main request is sent to apply user agent and other overrides.
  if (frame_tree_node->opener()) {
    return nullptr;
  }
  return std::make_unique<RequestThrottle>(weak_factory_.GetWeakPtr(),
                                           navigation_handle, host);
}

CNTargetHandler::Session* CNTargetHandler::FindWaitingSession(
    CitizenNotesAgentHost* host) {
  if (!host) {
    return nullptr;
  }
  auto it = auto_attached_sessions_.find(host);
  if (it == auto_attached_sessions_.end()) {
    return nullptr;
  }
  if (!it->second->IsWaitingForDebuggerOnStart()) {
    return nullptr;
  }
  return it->second;
}

void CNTargetHandler::ClearThrottles() {
  base::flat_set<Throttle*> copy(throttles_);
  for (Throttle* throttle : copy)
    throttle->Clear();
  throttles_.clear();
}

void CNTargetHandler::SetAutoAttachInternal(bool auto_attach,
                                          bool wait_for_debugger_on_start,
                                          bool flatten,
                                          base::OnceClosure callback) {
  for (auto& entry : auto_attach_related_targets_)
    entry.first->RemoveClient(this);
  auto_attach_related_targets_.clear();
  flatten_auto_attach_ = flatten;
  if (auto_attach_)
    auto_attacher_->RemoveClient(this);
  auto_attach_ = auto_attach;
  wait_for_debugger_on_start_ = wait_for_debugger_on_start;
  if (auto_attach_) {
    auto_attacher_->AddClient(this, wait_for_debugger_on_start,
                              std::move(callback));
  } else {
    while (!auto_attached_sessions_.empty())
      auto_attached_sessions_.begin()->second->Detach(false);
    ClearThrottles();
    auto_attach_target_filter_.reset();
    std::move(callback).Run();
  }
}

void CNTargetHandler::UpdateAgentHostObserver() {
  if (discover() == observing_agent_hosts_)
    return;
  observing_agent_hosts_ = discover();
  if (observing_agent_hosts_)
    CitizenNotesAgentHost::AddObserver(this);
  else
    CitizenNotesAgentHost::RemoveObserver(this);
}

bool CNTargetHandler::AutoAttach(CNTargetAutoAttacher* source,
                               CitizenNotesAgentHost* host,
                               bool waiting_for_debugger) {
  DCHECK(host);
  DCHECK(auto_attach_target_filter_);
  if (!auto_attach_target_filter_->Match(*host))
    return false;
  if (base::Contains(auto_attached_sessions_, host)) {
    return false;
  }
  if (!auto_attach_service_workers_ &&
      host->GetType() == CitizenNotesAgentHost::kTypeServiceWorker) {
    return false;
  }
  std::string session_id =
      Session::Attach(this, host, waiting_for_debugger, flatten_auto_attach_);
  Session* session = attached_sessions_[session_id].get();
  session->auto_attacher_id_ = reinterpret_cast<uintptr_t>(source);
  auto_attached_sessions_[host] = session;
  return true;
}

void CNTargetHandler::AutoDetach(CNTargetAutoAttacher* source,
                               CitizenNotesAgentHost* host) {
  auto it = auto_attached_sessions_.find(host);
  if (it == auto_attached_sessions_.end())
    return;
  it->second->Detach(false);
}

void CNTargetHandler::SetAttachedTargetsOfType(
    CNTargetAutoAttacher* source,
    const base::flat_set<scoped_refptr<CitizenNotesAgentHost>>& new_hosts,
    const std::string& type) {
  DCHECK(!type.empty());
  // Ignore page targets coming from frame auto-attachers when client has
  // opted into supporting the tab targets. These are portals and are now
  // reported via the tab target.
  if (!auto_attach_portals_ && type == CitizenNotesAgentHost::kTypePage) {
    DCHECK(source == auto_attacher_);
    return;
  }
  auto old_sessions = auto_attached_sessions_;
  for (auto& entry : old_sessions) {
    scoped_refptr<CitizenNotesAgentHost> host(entry.first);
    if (host->GetType() == type &&
        entry.second->auto_attacher_id_ ==
            reinterpret_cast<uintptr_t>(source) &&
        !base::Contains(new_hosts, host)) {
      AutoDetach(source, host.get());
    }
  }
  for (auto& host : new_hosts) {
    if (!base::Contains(old_sessions, host.get())) {
      AutoAttach(source, host.get(), false);
    }
  }
}

void CNTargetHandler::TargetInfoChanged(CitizenNotesAgentHost* host) {
  // Only send target info for targets we reported in any way.
  if (!base::Contains(reported_hosts_, host) &&
      auto_attached_sessions_.find(host) == auto_attached_sessions_.end()) {
    return;
  }
  frontend_->TargetInfoChanged(BuildTargetInfo(host));
}

void CNTargetHandler::AutoAttacherDestroyed(CNTargetAutoAttacher* auto_attacher) {
  auto throttles = throttles_;
  for (auto* throttle : throttles_) {
    if (throttle->auto_attacher() == auto_attacher)
      throttle->Clear();
  }
  for (auto& entry : auto_attached_sessions_) {
    if (entry.second->auto_attacher_id_ ==
        reinterpret_cast<uintptr_t>(auto_attacher)) {
      entry.second->auto_attacher_id_ = 0;
    }
  }
  auto_attach_related_targets_.erase(auto_attacher);
}

bool CNTargetHandler::ShouldWaitForDebuggerOnStart(
    NavigationRequest* navigation_request) const {
  if (auto_attach_)
    return wait_for_debugger_on_start_;
  DCHECK(!auto_attach_related_targets_.empty());
  auto* host = RenderFrameCitizenNotesAgentHost::GetFor(
      navigation_request->frame_tree_node());
  if (!host)
    return false;
  auto it = auto_attach_related_targets_.find(host->auto_attacher());
  return it != auto_attach_related_targets_.end() && it->second;
}

bool CNTargetHandler::ShouldThrottlePopups() const {
  return auto_attach_;
}

void CNTargetHandler::DisableAutoAttachOfServiceWorkers() {
  auto_attach_service_workers_ = false;
}

void CNTargetHandler::DisableAutoAttachOfPortals() {
  DCHECK(access_mode_ != AccessMode::kBrowser);
  auto_attach_portals_ = false;
}

Response CNTargetHandler::FindSession(Maybe<std::string> session_id,
                                    Maybe<std::string> target_id,
                                    Session** session) {
  *session = nullptr;
  if (session_id.has_value()) {
    auto it = attached_sessions_.find(session_id.value());
    if (it == attached_sessions_.end())
      return Response::InvalidParams("No session with given id");
    *session = it->second.get();
    return Response::Success();
  }
  if (target_id.has_value()) {
    for (auto& it : attached_sessions_) {
      if (it.second->IsAttachedTo(target_id.value())) {
        if (*session)
          return Response::ServerError(
              "Multiple sessions attached, specify id.");
        *session = it.second.get();
      }
    }
    if (!*session)
      return Response::InvalidParams("No session for given target id");
    return Response::Success();
  }
  return Response::InvalidParams("Session id must be specified");
}

// ----------------- Protocol ----------------------

Response CNTargetHandler::SetDiscoverTargets(
    bool discover,
    Maybe<protocol::Array<protocol::Target::FilterEntry>> filter) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  if (!discover && filter && !filter->empty()) {
    return Response::InvalidParams(
        "Filter should not be present with `discover` is off");
  }
  const bool old_discover = CNTargetHandler::discover();
  discover_target_filter_ =
      discover ? TargetFilter::Create(std::move(filter)) : nullptr;
  if (old_discover == discover) {
    // Report the newly matching targets that were not yet reported.
    if (discover) {
      for (const auto& target : CitizenNotesAgentHost::GetOrCreateAll())
        CitizenNotesAgentHostCreated(target.get());
    }
    return Response::Success();
  }
  UpdateAgentHostObserver();
  if (!CNTargetHandler::discover())
    reported_hosts_.clear();
  return Response::Success();
}

void CNTargetHandler::SetAutoAttach(
    bool auto_attach,
    bool wait_for_debugger_on_start,
    Maybe<bool> flatten,
    Maybe<protocol::Array<protocol::Target::FilterEntry>> filter,
    std::unique_ptr<SetAutoAttachCallback> callback) {
  if (access_mode_ == AccessMode::kBrowser && !flatten.value_or(false)) {
    callback->sendFailure(Response::InvalidParams(
        "Only flatten protocol is supported with browser level auto-attach"));
    return;
  }
  if (!auto_attach && filter && !filter->empty()) {
    callback->sendFailure(Response::InvalidParams(
        "Target filter should be empty whien disabling auto-attach"));
    return;
  }
  auto_attach_target_filter_ =
      auto_attach ? TargetFilter::Create(std::move(filter)) : nullptr;
  if (auto_attach_target_filter_ && access_mode_ == AccessMode::kBrowser &&
      auto_attach_target_filter_->Match(CitizenNotesAgentHost::kTypeTab) &&
      auto_attach_target_filter_->Match(CitizenNotesAgentHost::kTypePage)) {
    callback->sendFailure(Response::InvalidParams(
        "Filter should not simultaneously allow \"tab\" and \"page\", "
        "page targets are attached via tab targets"));
    return;
  }
  SetAutoAttachInternal(
      auto_attach, wait_for_debugger_on_start, flatten.value_or(false),
      base::BindOnce(&SetAutoAttachCallback::sendSuccess, std::move(callback)));
}

void CNTargetHandler::AutoAttachRelated(
    const std::string& targetId,
    bool wait_for_debugger_on_start,
    Maybe<protocol::Array<protocol::Target::FilterEntry>> filter,
    std::unique_ptr<AutoAttachRelatedCallback> callback) {
  if (access_mode_ != AccessMode::kBrowser) {
    callback->sendFailure(Response::ServerError(
        "Target.autoAttachRelated is only supported on the Browser target"));
    return;
  }
  scoped_refptr<CitizenNotesAgentHostImpl> host =
      CitizenNotesAgentHostImpl::GetForId(targetId);
  if (!host) {
    callback->sendFailure(Response::InvalidParams(kTargetNotFound));
    return;
  }
  CNTargetAutoAttacher* auto_attacher = host->auto_attacher();
  if (!auto_attacher) {
    callback->sendFailure(
        Response::InvalidParams("Target does not support auto-attaching"));
    return;
  }
  if (auto_attach_) {
    DCHECK(auto_attach_related_targets_.empty());
    SetAutoAttachInternal(false, false, true, base::DoNothing());
  }
  flatten_auto_attach_ = true;
  auto_attach_target_filter_ = TargetFilter::Create(std::move(filter));
  AutoAttach(auto_attacher_, host.get(), false);
  auto inserted = auto_attach_related_targets_.insert(
      std::make_pair(auto_attacher, wait_for_debugger_on_start));
  if (!inserted.second) {
    auto_attacher->UpdateWaitForDebuggerOnStart(
        this, wait_for_debugger_on_start,
        base::BindOnce(&AutoAttachRelatedCallback::sendSuccess,
                       std::move(callback)));
    inserted.first->second = wait_for_debugger_on_start;
    return;
  }
  auto_attacher->AddClient(
      this, wait_for_debugger_on_start,
      base::BindOnce(&AutoAttachRelatedCallback::sendSuccess,
                     std::move(callback)));
}

Response CNTargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<Target::RemoteLocation>>) {
  return Response::ServerError("Not supported");
}

Response CNTargetHandler::AttachToTarget(const std::string& target_id,
                                       Maybe<bool> flatten,
                                       std::string* out_session_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<CitizenNotesAgentHost> agent_host =
      CitizenNotesAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams(kTargetNotFound);
  *out_session_id =
      Session::Attach(this, agent_host.get(), false, flatten.value_or(false));
  return Response::Success();
}

Response CNTargetHandler::AttachToBrowserTarget(std::string* out_session_id) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::ServerError(kNotAllowedError);
  scoped_refptr<CitizenNotesAgentHost> agent_host =
      CitizenNotesAgentHost::CreateForBrowser(
          nullptr, CitizenNotesAgentHost::CreateServerSocketCallback());
  *out_session_id = Session::Attach(this, agent_host.get(), false, true);
  return Response::Success();
}

Response CNTargetHandler::DetachFromTarget(Maybe<std::string> session_id,
                                         Maybe<std::string> target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  Session* session = nullptr;
  Response response =
      FindSession(std::move(session_id), std::move(target_id), &session);
  if (!response.IsSuccess())
    return response;
  session->Detach(false);
  return Response::Success();
}

Response CNTargetHandler::SendMessageToTarget(const std::string& message,
                                            Maybe<std::string> session_id,
                                            Maybe<std::string> target_id) {
  Session* session = nullptr;
  Response response =
      FindSession(std::move(session_id), std::move(target_id), &session);
  if (!response.IsSuccess())
    return response;
  if (session->flatten_protocol_) {
    return Response::ServerError(
        "When using flat protocol, messages are routed to the target "
        "via the sessionId attribute.");
  }
  session->SendMessageToAgentHost(base::as_bytes(base::make_span(message)));
  return Response::Success();
}

Response CNTargetHandler::GetTargetInfo(
    Maybe<std::string> maybe_target_id,
    std::unique_ptr<Target::TargetInfo>* target_info) {
  const std::string& target_id = maybe_target_id.value_or(owner_target_id_);
  if (access_mode_ == AccessMode::kAutoAttachOnly &&
      target_id != owner_target_id_) {
    return Response::ServerError(kNotAllowedError);
  }
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<CitizenNotesAgentHost> agent_host(
      CitizenNotesAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams(kTargetNotFound);
  *target_info = BuildTargetInfo(agent_host.get());
  return Response::Success();
}

Response CNTargetHandler::ActivateTarget(const std::string& target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<CitizenNotesAgentHost> agent_host(
      CitizenNotesAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams(kTargetNotFound);
  agent_host->Activate();
  return Response::Success();
}

Response CNTargetHandler::CloseTarget(const std::string& target_id,
                                    bool* out_success) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  scoped_refptr<CitizenNotesAgentHost> agent_host =
      CitizenNotesAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams(kTargetNotFound);
  if (!agent_host->Close())
    return Response::InvalidParams("Specified target doesn't support closing");
  *out_success = true;
  return Response::Success();
}

Response CNTargetHandler::ExposeDevToolsProtocol(
    const std::string& target_id,
    Maybe<std::string> binding_name) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::InvalidParams(kNotAllowedError);
  scoped_refptr<CitizenNotesAgentHost> agent_host =
      CitizenNotesAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams(kTargetNotFound);

  if (BrowserToPageConnector::GetInstanceMap()[agent_host.get()]) {
    return Response::ServerError(base::StringPrintf(
        "Target with id %s is already granted remote debugging bindings.",
        target_id.c_str()));
  }
  if (!agent_host->GetWebContents()) {
    return Response::ServerError(
        "RemoteDebuggingBinding can be granted only to page targets");
  }

  new BrowserToPageConnector(binding_name.value_or("cdp"), agent_host.get());
  return Response::Success();
}

Response CNTargetHandler::CreateTarget(const std::string& url,
                                     Maybe<int> width,
                                     Maybe<int> height,
                                     Maybe<std::string> context_id,
                                     Maybe<bool> enable_begin_frame_control,
                                     Maybe<bool> new_window,
                                     Maybe<bool> background,
                                     Maybe<bool> for_tab,
                                     std::string* out_target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  CitizenNotesManagerDelegate* delegate =
      CitizenNotesManager::GetInstance()->delegate();
  if (!delegate)
    return Response::ServerError("Not supported");
  GURL gurl(url);
  if (gurl.is_empty()) {
    gurl = GURL(url::kAboutBlankURL);
  }
  content::CitizenNotesManagerDelegate::TargetType target_type =
      for_tab.value_or(session_mode_ ==
                       CitizenNotesSession::Mode::kSupportsTabTarget)
          ? content::CitizenNotesManagerDelegate::kTab
          : content::CitizenNotesManagerDelegate::kFrame;
  scoped_refptr<content::CitizenNotesAgentHost> agent_host =
      delegate->CreateNewTarget(gurl, target_type);
  if (!agent_host)
    return Response::ServerError("Not supported");
  *out_target_id = agent_host->GetId();
  return Response::Success();
}

Response CNTargetHandler::GetTargets(
    Maybe<protocol::Array<protocol::Target::FilterEntry>> filter,
    std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  std::unique_ptr<TargetFilter> passed_filter =
      filter.has_value() || !discover_target_filter_
          ? TargetFilter::Create(std::move(filter))
          : nullptr;
  const TargetFilter* effective_filter =
      passed_filter ? passed_filter.get() : discover_target_filter_.get();
  DCHECK(effective_filter);
  *target_infos = std::make_unique<protocol::Array<Target::TargetInfo>>();
  for (const auto& host : CitizenNotesAgentHost::GetOrCreateAll()) {
    if (effective_filter->Match(*host))
      (*target_infos)->emplace_back(BuildTargetInfo(host.get()));
  }
  return Response::Success();
}

// -------------- CitizenNotesAgentHostObserver -----------------

bool CNTargetHandler::ShouldForceCitizenNotesAgentHostCreation() {
  return true;
}

void CNTargetHandler::CitizenNotesAgentHostCreated(CitizenNotesAgentHost* host) {
  DCHECK(discover());
  DCHECK(host);
  if (!discover_target_filter_->Match(*host))
    return;
  // If we start discovering late, all existing agent hosts will be reported,
  // but we could have already attached to some.
  if (!base::Contains(reported_hosts_, host)) {
    frontend_->TargetCreated(BuildTargetInfo(host));
    reported_hosts_.insert(host);
  }
}

void CNTargetHandler::CitizenNotesAgentHostNavigated(CitizenNotesAgentHost* host) {
  TargetInfoChanged(host);
}

void CNTargetHandler::CitizenNotesAgentHostDestroyed(CitizenNotesAgentHost* host) {
  if (!base::Contains(reported_hosts_, host)) {
    return;
  }
  frontend_->TargetDestroyed(host->GetId());
  reported_hosts_.erase(host);
}

void CNTargetHandler::CitizenNotesAgentHostAttached(CitizenNotesAgentHost* host) {
  TargetInfoChanged(host);
}

void CNTargetHandler::CitizenNotesAgentHostDetached(CitizenNotesAgentHost* host) {
  TargetInfoChanged(host);
}

void CNTargetHandler::CitizenNotesAgentHostCrashed(CitizenNotesAgentHost* host,
                                             base::TerminationStatus status) {
  if (!base::Contains(reported_hosts_, host)) {
    return;
  }
  frontend_->TargetCrashed(host->GetId(), TerminationStatusToString(status),
                           host->GetWebContents()
                               ? host->GetWebContents()->GetCrashedErrorCode()
                               : 0);
}

// ----------------- More protocol methods -------------------

void CNTargetHandler::CreateBrowserContext(
    Maybe<bool> in_disposeOnDetach,
    Maybe<String> in_proxyServer,
    Maybe<String> in_proxyBypassList,
    Maybe<protocol::Array<String>> in_originsToGrantUniversalNetworkAccess,
    std::unique_ptr<CreateBrowserContextCallback> callback) {
  if (access_mode_ != AccessMode::kBrowser) {
    callback->sendFailure(Response::ServerError(kNotAllowedError));
    return;
  }
  CitizenNotesManagerDelegate* delegate =
      CitizenNotesManager::GetInstance()->delegate();
  if (!delegate) {
    callback->sendFailure(
        Response::ServerError("Browser context management is not supported."));
    return;
  }

  if (in_proxyServer.has_value()) {
    pending_proxy_config_ = net::ProxyConfig();
    pending_proxy_config_->proxy_rules().ParseFromString(
        in_proxyServer.value());
    if (in_proxyBypassList.has_value()) {
      pending_proxy_config_->proxy_rules().bypass_rules.ParseFromString(
          in_proxyBypassList.value());
    }
  }

  // Pre-process universal network access origins before actual context creation
  // in case we need to bail out with error.
  std::vector<url::Origin> originsToGrantUniversalNetworkAccess;
  if (in_originsToGrantUniversalNetworkAccess.has_value()) {
    for (const auto& origin_str :
         in_originsToGrantUniversalNetworkAccess.value()) {
      GURL url(origin_str);
      url::Origin origin = url::Origin::Create(url);
      if (!url.is_valid() || origin.opaque()) {
        callback->sendFailure(
            Response::InvalidParams("Invalid origin " + origin_str));
        return;
      }
      originsToGrantUniversalNetworkAccess.push_back(std::move(origin));
    }
  }

  BrowserContext* context = delegate->CreateBrowserContext();
  if (!context) {
    callback->sendFailure(
        Response::ServerError("Failed to create browser context."));
    pending_proxy_config_.reset();
    return;
  }

  for (const auto& origin : originsToGrantUniversalNetworkAccess) {
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns;
    allow_patterns.push_back(network::mojom::CorsOriginPattern::New());
    allow_patterns.back()->protocol = "http";
    allow_patterns.back()->priority =
        network::mojom::CorsOriginAccessMatchPriority::kMaxPriority;
    allow_patterns.push_back(network::mojom::CorsOriginPattern::New());
    allow_patterns.back()->protocol = "https";
    allow_patterns.back()->priority =
        network::mojom::CorsOriginAccessMatchPriority::kMaxPriority;

    // It's fine to not await the completion here -- this is implicitly
    // serialized with the actual URLLoaderFactory / URLLoader creation.
    content::CorsOriginPatternSetter::Set(
        context, origin, std::move(allow_patterns), {}, base::DoNothing());
  }

  if (pending_proxy_config_) {
    contexts_with_overridden_proxy_[context->UniqueId()] =
        std::move(*pending_proxy_config_);
    pending_proxy_config_.reset();
  }

  if (in_disposeOnDetach.value_or(false)) {
    dispose_on_detach_context_ids_.insert(context->UniqueId());
  }
  callback->sendSuccess(context->UniqueId());
}

protocol::Response CNTargetHandler::GetBrowserContexts(
    std::unique_ptr<protocol::Array<protocol::String>>* browser_context_ids) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::ServerError(kNotAllowedError);
  CitizenNotesManagerDelegate* delegate =
      CitizenNotesManager::GetInstance()->delegate();
  if (!delegate)
    return Response::ServerError(
        "Browser context management is not supported.");
  std::vector<content::BrowserContext*> contexts =
      delegate->GetBrowserContexts();
  *browser_context_ids = std::make_unique<protocol::Array<protocol::String>>();
  for (auto* context : contexts)
    (*browser_context_ids)->emplace_back(context->UniqueId());
  return Response::Success();
}

void CNTargetHandler::DisposeBrowserContext(
    const std::string& context_id,
    std::unique_ptr<DisposeBrowserContextCallback> callback) {
  if (access_mode_ != AccessMode::kBrowser) {
    callback->sendFailure(Response::ServerError(kNotAllowedError));
    return;
  }
  CitizenNotesManagerDelegate* delegate =
      CitizenNotesManager::GetInstance()->delegate();
  if (!delegate) {
    callback->sendFailure(
        Response::ServerError("Browser context management is not supported."));
    return;
  }
  std::vector<content::BrowserContext*> contexts =
      delegate->GetBrowserContexts();
  auto context_it = base::ranges::find(contexts, context_id,
                                       &content::BrowserContext::UniqueId);
  if (context_it == contexts.end()) {
    callback->sendFailure(
        Response::ServerError("Failed to find context with id " + context_id));
    return;
  }
  dispose_on_detach_context_ids_.erase(context_id);
  delegate->DisposeBrowserContext(
      *context_it,
      base::BindOnce(
          [](std::unique_ptr<DisposeBrowserContextCallback> callback,
             bool success, const std::string& error) {
            if (success)
              callback->sendSuccess();
            else
              callback->sendFailure(Response::ServerError(error));
          },
          std::move(callback)));
}

void CNTargetHandler::ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* context_params) {
  // Under certain conditions, storage partition is created synchronously for
  // the browser context. Account for this use case.
  if (pending_proxy_config_) {
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation(std::move(*pending_proxy_config_),
                                       kSettingsProxyConfigTrafficAnnotation);
    pending_proxy_config_.reset();
    return;
  }
  auto it = contexts_with_overridden_proxy_.find(browser_context->UniqueId());
  if (it != contexts_with_overridden_proxy_.end()) {
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        std::move(it->second), kSettingsProxyConfigTrafficAnnotation);
    contexts_with_overridden_proxy_.erase(browser_context->UniqueId());
  }
}

void CNTargetHandler::AddWorkerThrottle(
    CitizenNotesAgentHost* agent_host,
    scoped_refptr<CitizenNotesThrottleHandle> throttle_handle) {
  if (!agent_host)
    return;

  if (auto_attached_sessions_.count(agent_host)) {
    if (auto_attached_sessions_[agent_host]->IsWaitingForDebuggerOnStart()) {
      auto_attached_sessions_[agent_host]->SetWorkerThrottle(
          std::move(throttle_handle));
    }
  }
}

}  // namespace content::protocol
