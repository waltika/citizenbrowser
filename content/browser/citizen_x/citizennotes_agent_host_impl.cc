// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "content/browser/citizen_x/auction_worklet_citizennotes_agent_host.h"
#include "content/browser/citizen_x/citizennotes_http_handler.h"
#include "content/browser/citizen_x/citizennotes_manager.h"
#include "content/browser/citizen_x/citizennotes_pipe_handler.h"
#include "content/browser/citizen_x/citizennotes_stream_file.h"
#include "content/browser/citizen_x/cnforwarding_agent_host.h"
#include "content/browser/citizen_x/mojom_citizennotes_agent_host.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/citizen_x/service_worker_citizennotes_agent_host.h"
#include "content/browser/citizen_x/service_worker_citizennotes_manager.h"
#include "content/browser/citizen_x/shared_storage_worklet_citizennotes_manager.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_agent_host.h"
#include "content/browser/citizen_x/shared_worker_citizennotes_manager.h"
#include "content/browser/citizen_x/web_contents_citizennotes_agent_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/citizennotes_external_agent_proxy_delegate.h"
#include "content/public/browser/citizennotes_socket_factory.h"
#include "content/public/browser/mojom_citizennotes_agent_host_delegate.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(IS_WIN)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace content {

namespace {

typedef std::map<std::string, CitizenNotesAgentHostImpl*> CitizenNotesMap;
CitizenNotesMap& GetCitizennotesInstances() {
  static base::NoDestructor<CitizenNotesMap> instance;
  return *instance;
}

base::ObserverList<CitizenNotesAgentHostObserver>::Unchecked&
GetCitizennotesObservers() {
  static base::NoDestructor<
      base::ObserverList<CitizenNotesAgentHostObserver>::Unchecked>
      instance;
  return *instance;
}

void SetCitizenNotesHttpHandler(std::unique_ptr<CitizenNotesHttpHandler> handler) {
  static base::NoDestructor<std::unique_ptr<CitizenNotesHttpHandler>> instance;
  *instance = std::move(handler);
}

void SetCitizenNotesPipeHandler(std::unique_ptr<CitizenNotesPipeHandler> handler) {
  static base::NoDestructor<std::unique_ptr<CitizenNotesPipeHandler>> instance;
  *instance = std::move(handler);
}

#if BUILDFLAG(IS_WIN)
// Map handle to file descriptor
int AdoptHandle(const std::string& serialized_pipe, int flags) {
  // Deserialize the handle.
  // We use the fact that inherited handles in the child process have the same
  // value and access rights as in the parent process.
  // See:
  // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
  uint32_t handle_as_uint32;
  if (!base::StringToUint(serialized_pipe, &handle_as_uint32)) {
    return -1;
  }
  HANDLE handle = base::win::Uint32ToHandle(handle_as_uint32);
  if (GetFileType(handle) != FILE_TYPE_PIPE) {
    return -1;
  }
  // Map the handle to the file descriptor
  return _open_osfhandle(reinterpret_cast<intptr_t>(handle), flags);
}

// Transform the --remote-debugging-io-pipes switch value to file descriptors.
bool AdoptPipes(const std::string& io_pipes, int& read_fd, int& write_fd) {
  // The parent process is expected to serialize the input and the output pipe
  // handles as unsigned integers, concatenate them with comma and pass this
  // string to the browser via the --remote-debugging-io-pipes argument.
  std::vector<std::string> pipe_names = base::SplitString(
      io_pipes, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pipe_names.size() != 2) {
    return false;
  }
  const std::string& in_pipe = pipe_names[0];
  const std::string& out_pipe = pipe_names[1];
  // If adoption of the read_fd fails the already adopted write_fd signalizing
  // the remote end that the session is over.
  int tmp_write_fd = AdoptHandle(out_pipe, 0);
  if (tmp_write_fd < 0) {
    return false;
  }
  int tmp_read_fd = AdoptHandle(in_pipe, _O_RDONLY);
  if (tmp_read_fd < 0) {
    _close(tmp_write_fd);
    return false;
  }
  read_fd = tmp_read_fd;
  write_fd = tmp_write_fd;
  return true;
}
#endif

}  // namespace

const char CitizenNotesAgentHost::kTypeTab[] = "tab";
const char CitizenNotesAgentHost::kTypePage[] = "page";
const char CitizenNotesAgentHost::kTypeFrame[] = "iframe";
const char CitizenNotesAgentHost::kTypeDedicatedWorker[] = "worker";
const char CitizenNotesAgentHost::kTypeSharedWorker[] = "shared_worker";
const char CitizenNotesAgentHost::kTypeServiceWorker[] = "service_worker";
const char CitizenNotesAgentHost::kTypeSharedStorageWorklet[] =
    "shared_storage_worklet";
const char CitizenNotesAgentHost::kTypeBrowser[] = "browser";
const char CitizenNotesAgentHost::kTypeGuest[] = "webview";
const char CitizenNotesAgentHost::kTypeOther[] = "other";
const char CitizenNotesAgentHost::kTypeAuctionWorklet[] = "auction_worklet";
const char CitizenNotesAgentHost::kTypeAssistiveTechnology[] =
    "assistive_technology";
int CitizenNotesAgentHostImpl::s_force_creation_count_ = 0;

// static
std::string CitizenNotesAgentHost::GetProtocolVersion() {
  // TODO(dgozman): generate this.
  return "1.3";
}

// static
bool CitizenNotesAgentHost::IsSupportedProtocolVersion(const std::string& version) {
  // TODO(dgozman): generate this.
  return version == "1.0" || version == "1.1" || version == "1.2" ||
         version == "1.3";
}

// static
CitizenNotesAgentHost::List CitizenNotesAgentHost::GetAll() {
  CitizenNotesAgentHost::List result;
  for (auto& instance : GetCitizennotesInstances()) {
    result.push_back(instance.second);
  }
  return result;
}

// static
CitizenNotesAgentHost::List CitizenNotesAgentHost::GetOrCreateAll() {
  List result;
  SharedWorkerCitizenNotesAgentHost::List shared_list;
  // SharedWorkerCitizenNotesManager::GetInstance()->AddAllAgentHosts(&shared_list);
  //TODO: Adapt
  for (const auto& host : shared_list)
    result.push_back(host);

  ServiceWorkerCitizenNotesAgentHost::List service_list;
  ServiceWorkerCitizenNotesManager::GetInstance()->AddAllAgentHosts(&service_list);
  for (const auto& host : service_list)
    result.push_back(host);

  //SharedStorageWorkletCitizenNotesManager::GetInstance()->AddAllAgentHosts(&result);
  //TODO: Adapt
    
  // TODO(dgozman): we should add dedicated workers here, but clients are not
  // ready.
  //RenderFrameCitizenNotesAgentHost::AddAllAgentHosts(&result);
  //WebContentsCitizenNotesAgentHost::AddAllAgentHosts(&result);
  // TODO: Adapt
    
  AuctionWorkletCitizenNotesAgentHostManager::GetInstance().GetAll(&result);
  MojomCitizenNotesAgentHost::GetAll(&result);

#if DCHECK_IS_ON()
  for (auto it : result) {
    CitizenNotesAgentHostImpl* host = static_cast<CitizenNotesAgentHostImpl*>(it.get());
    DCHECK(base::Contains(GetCitizennotesInstances(), host->id_));
  }
#endif

  return result;
}

// static
void CitizenNotesAgentHost::StartRemoteDebuggingServer(
    std::unique_ptr<CitizenNotesSocketFactory> server_socket_factory,
    const base::FilePath& active_port_output_directory,
    const base::FilePath& debug_frontend_dir) {
  CitizenNotesManagerDelegate* delegate =
      CitizenNotesManager::GetInstance()->delegate();
  CHECK(delegate);
  SetCitizenNotesHttpHandler(std::make_unique<CitizenNotesHttpHandler>(
      delegate, std::move(server_socket_factory), active_port_output_directory,
      debug_frontend_dir));
}

// static
void CitizenNotesAgentHost::StartRemoteDebuggingPipeHandler(
    base::OnceClosure on_disconnect) {
  int read_fd = kReadFD;
  int write_fd = kWriteFD;
#if BUILDFLAG(IS_WIN)
  std::string io_pipes =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRemoteDebuggingIoPipes);
  if (!io_pipes.empty() && !AdoptPipes(io_pipes, read_fd, write_fd)) {
    std::move(on_disconnect).Run();
    return;
  }
#endif
  SetCitizenNotesPipeHandler(std::make_unique<CitizenNotesPipeHandler>(
      read_fd, write_fd, std::move(on_disconnect)));
}

// static
void CitizenNotesAgentHost::StopRemoteDebuggingServer() {
  SetCitizenNotesHttpHandler(nullptr);
}

// static
void CitizenNotesAgentHost::StopRemoteDebuggingPipeHandler() {
  SetCitizenNotesPipeHandler(nullptr);
}

CitizenNotesAgentHostImpl::CitizenNotesAgentHostImpl(const std::string& id)
    : id_(id), renderer_channel_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CitizenNotesAgentHostImpl::~CitizenNotesAgentHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyDestroyed();
}

// static
scoped_refptr<CitizenNotesAgentHostImpl> CitizenNotesAgentHostImpl::GetForId(
    const std::string& id) {
  auto it = GetCitizennotesInstances().find(id);
  if (it == GetCitizennotesInstances().end())
    return nullptr;
  return it->second;
}

// static
scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::GetForId(
    const std::string& id) {
  return CitizenNotesAgentHostImpl::GetForId(id);
}

// static
scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::Forward(
    const std::string& id,
    std::unique_ptr<CitizenNotesExternalAgentProxyDelegate> delegate) {
  scoped_refptr<CitizenNotesAgentHost> result = CitizenNotesAgentHost::GetForId(id);
  if (result)
    return result;
  return new CNForwardingAgentHost(id, std::move(delegate));
}

// static
scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::CreateForMojomDelegate(
    const std::string& id,
    std::unique_ptr<MojomCitizenNotesAgentHostDelegate> delegate) {
  scoped_refptr<CitizenNotesAgentHost> result = CitizenNotesAgentHost::GetForId(id);
  if (result) {
    return result;
  }
  return new MojomCitizenNotesAgentHost(id, std::move(delegate));
}

CitizenNotesSession* CitizenNotesAgentHostImpl::SessionByClient(
    CitizenNotesAgentHostClient* client) {
  auto it = session_by_client_.find(client);
  return it == session_by_client_.end() ? nullptr : it->second.get();
}

bool CitizenNotesAgentHostImpl::AttachInternal(
    std::unique_ptr<CitizenNotesSession> session_owned) {
  return AttachInternal(std::move(session_owned), true);
}

bool CitizenNotesAgentHostImpl::AttachInternal(
    std::unique_ptr<CitizenNotesSession> session_owned,
    bool acquire_wake_lock) {
  scoped_refptr<CitizenNotesAgentHostImpl> protect(this);
  CitizenNotesSession* session = session_owned.get();
  session->SetAgentHost(this);
  if (!AttachSession(session, acquire_wake_lock))
    return false;
  renderer_channel_.AttachSession(session);
  sessions_.push_back(session);
  DCHECK(!base::Contains(session_by_client_, session->GetClient()));
  session_by_client_.emplace(session->GetClient(), std::move(session_owned));
  if (sessions_.size() == 1)
    NotifyAttached();
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->ClientAttached(session);
  return true;
}

bool CitizenNotesAgentHostImpl::AttachClient(CitizenNotesAgentHostClient* client) {
  if (SessionByClient(client))
    return false;
  return AttachInternal(
      std::make_unique<CitizenNotesSession>(client, GetSessionMode()),
      /*acquire_wake_lock=*/true);
}

bool CitizenNotesAgentHostImpl::AttachClientWithoutWakeLock(
    content::CitizenNotesAgentHostClient* client) {
  if (SessionByClient(client))
    return false;
  return AttachInternal(
      std::make_unique<CitizenNotesSession>(client, GetSessionMode()),
      /*acquire_wake_lock=*/false);
}

bool CitizenNotesAgentHostImpl::DetachClient(CitizenNotesAgentHostClient* client) {
  CitizenNotesSession* session = SessionByClient(client);
  if (!session)
    return false;
  scoped_refptr<CitizenNotesAgentHostImpl> protect(this);
  DetachInternal(session);
  return true;
}

void CitizenNotesAgentHostImpl::DispatchProtocolMessage(
    CitizenNotesAgentHostClient* client,
    base::span<const uint8_t> message) {
  CitizenNotesSession* session = SessionByClient(client);
  if (session)
    session->DispatchProtocolMessage(message);
}

void CitizenNotesAgentHostImpl::DetachInternal(CitizenNotesSession* session) {
  std::unique_ptr<CitizenNotesSession> session_owned =
      std::move(session_by_client_[session->GetClient()]);
  DCHECK_EQ(session, session_owned.get());
  // Make sure we dispose session prior to reporting it to the host.
  session->Dispose();
  base::Erase(sessions_, session);
  session_by_client_.erase(session->GetClient());
  DetachSession(session);
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->ClientDetached(session);
  if (sessions_.empty()) {
    io_context_.DiscardAllStreams();
    NotifyDetached();
  }
}

bool CitizenNotesAgentHostImpl::IsAttached() {
  return !sessions_.empty();
}

void CitizenNotesAgentHostImpl::InspectElement(RenderFrameHost* frame_host,
                                           int x,
                                           int y) {}

void CitizenNotesAgentHostImpl::GetUniqueFormControlId(
    int node_id,
    GetUniqueFormControlIdCallback callback) {
  NOTREACHED();
}

std::string CitizenNotesAgentHostImpl::GetId() {
  return id_;
}

std::string CitizenNotesAgentHostImpl::CreateIOStreamFromData(
    scoped_refptr<base::RefCountedMemory> data) {
  scoped_refptr<CitizenNotesStreamFile> stream =
      CitizenNotesStreamFile::Create(GetIOContext(), true /* binary */);
  std::string text(reinterpret_cast<const char*>(data->front()), data->size());
  stream->Append(std::make_unique<std::string>(text));
  return stream->handle();
}

std::string CitizenNotesAgentHostImpl::GetParentId() {
  return std::string();
}

std::string CitizenNotesAgentHostImpl::GetOpenerId() {
  return std::string();
}

std::string CitizenNotesAgentHostImpl::GetOpenerFrameId() {
  return std::string();
}

bool CitizenNotesAgentHostImpl::CanAccessOpener() {
  return false;
}

std::string CitizenNotesAgentHostImpl::GetDescription() {
  return std::string();
}

GURL CitizenNotesAgentHostImpl::GetFaviconURL() {
  return GURL();
}

std::string CitizenNotesAgentHostImpl::GetFrontendURL() {
  return std::string();
}

base::TimeTicks CitizenNotesAgentHostImpl::GetLastActivityTime() {
  return base::TimeTicks();
}

BrowserContext* CitizenNotesAgentHostImpl::GetBrowserContext() {
  return nullptr;
}

WebContents* CitizenNotesAgentHostImpl::GetWebContents() {
  return nullptr;
}

void CitizenNotesAgentHostImpl::DisconnectWebContents() {
}

void CitizenNotesAgentHostImpl::ConnectWebContents(WebContents* wc) {
}

CitizenNotesSession::Mode CitizenNotesAgentHostImpl::GetSessionMode() {
  return CitizenNotesSession::Mode::kDoesNotSupportTabTarget;
}

bool CitizenNotesAgentHostImpl::Inspect() {
  CitizenNotesManager* manager = CitizenNotesManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Inspect(this);
    return true;
  }
  return false;
}

void CitizenNotesAgentHostImpl::ForceDetachAllSessions() {
  std::ignore = ForceDetachAllSessionsImpl();
}

scoped_refptr<CitizenNotesAgentHost>
CitizenNotesAgentHostImpl::ForceDetachAllSessionsImpl() {
  scoped_refptr<CitizenNotesAgentHost> retain_this(this);
  while (!sessions_.empty()) {
    CitizenNotesAgentHostClient* client = (*sessions_.begin())->GetClient();
    DetachClient(client);
    client->AgentHostClosed(this);
  }
  return retain_this;
}

void CitizenNotesAgentHostImpl::MainThreadDebuggerPaused() {}
void CitizenNotesAgentHostImpl::MainThreadDebuggerResumed() {}

void CitizenNotesAgentHostImpl::ForceDetachRestrictedSessions(
    const std::vector<CitizenNotesSession*>& restricted_sessions) {
  scoped_refptr<CitizenNotesAgentHostImpl> protect(this);

  for (CitizenNotesSession* session : restricted_sessions) {
    CitizenNotesAgentHostClient* client = session->GetClient();
    DetachClient(client);
    client->AgentHostClosed(this);
  }
}

bool CitizenNotesAgentHostImpl::AttachSession(CitizenNotesSession* session,
                                          bool acquire_wake_lock) {
  return false;
}

void CitizenNotesAgentHostImpl::DetachSession(CitizenNotesSession* session) {}

void CitizenNotesAgentHostImpl::UpdateRendererChannel(bool force) {}

// static
void CitizenNotesAgentHost::DetachAllClients() {
  // Make a copy, since detaching may lead to agent destruction, which
  // removes it from the instances.
  std::vector<scoped_refptr<CitizenNotesAgentHostImpl>> copy;
  for (auto& instance : GetCitizennotesInstances()) {
    copy.push_back(instance.second);
  }
  for (auto& instance : copy) {
    instance->ForceDetachAllSessions();
  }
}

// static
void CitizenNotesAgentHost::AddObserver(CitizenNotesAgentHostObserver* observer) {
  if (observer->ShouldForceCitizenNotesAgentHostCreation()) {
    if (!CitizenNotesAgentHostImpl::s_force_creation_count_) {
      // Force all agent hosts when first observer is added.
      CitizenNotesAgentHost::GetOrCreateAll();
    }
    CitizenNotesAgentHostImpl::s_force_creation_count_++;
  }

  GetCitizennotesObservers().AddObserver(observer);
  for (const auto& id_host : GetCitizennotesInstances())
    observer->CitizenNotesAgentHostCreated(id_host.second);
}

// static
void CitizenNotesAgentHost::RemoveObserver(CitizenNotesAgentHostObserver* observer) {
  if (observer->ShouldForceCitizenNotesAgentHostCreation())
    CitizenNotesAgentHostImpl::s_force_creation_count_--;
  GetCitizennotesObservers().RemoveObserver(observer);
}

// static
bool CitizenNotesAgentHostImpl::ShouldForceCreation() {
  return !!s_force_creation_count_;
}

std::string CitizenNotesAgentHostImpl::GetSubtype() {
  return "";
}

void CitizenNotesAgentHostImpl::NotifyCreated() {
  DCHECK(!base::Contains(GetCitizennotesInstances(), id_));
  GetCitizennotesInstances()[id_] = this;
  for (auto& observer : GetCitizennotesObservers())
    observer.CitizenNotesAgentHostCreated(this);
}

void CitizenNotesAgentHostImpl::NotifyNavigated() {
  for (auto& observer : GetCitizennotesObservers())
    observer.CitizenNotesAgentHostNavigated(this);
}

void CitizenNotesAgentHostImpl::NotifyAttached() {
  for (auto& observer : GetCitizennotesObservers())
    observer.CitizenNotesAgentHostAttached(this);
}

void CitizenNotesAgentHostImpl::NotifyDetached() {
  for (auto& observer : GetCitizennotesObservers())
    observer.CitizenNotesAgentHostDetached(this);
}

void CitizenNotesAgentHostImpl::NotifyCrashed(base::TerminationStatus status) {
  for (auto& observer : GetCitizennotesObservers())
    observer.CitizenNotesAgentHostCrashed(this, status);
}

void CitizenNotesAgentHostImpl::NotifyDestroyed() {
  DCHECK(base::Contains(GetCitizennotesInstances(), id_));
  for (auto& observer : GetCitizennotesObservers())
    observer.CitizenNotesAgentHostDestroyed(this);
  GetCitizennotesInstances().erase(id_);
}

void CitizenNotesAgentHostImpl::ProcessHostChanged() {
  RenderProcessHost* host = GetProcessHost();
  if (!host) {
    return;
  }
  if (host->IsReady()) {
    SetProcessId(host->GetProcess().Pid());
  } else {
    host->PostTaskWhenProcessIsReady(base::BindOnce(
        &RenderFrameCitizenNotesAgentHost::ProcessHostChanged, this));
  }
}

void CitizenNotesAgentHostImpl::SetProcessId(base::ProcessId process_id) {
  CHECK_NE(process_id, base::kNullProcessId);
  if (process_id_ == process_id) {
    return;
  }
  process_id_ = process_id;
  for (auto& observer : GetCitizennotesObservers()) {
    observer.CitizenNotesAgentHostProcessChanged(this);
  }
}

CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    NetworkLoaderFactoryParamsAndInfo() = default;
CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    NetworkLoaderFactoryParamsAndInfo(
        url::Origin origin,
        net::SiteForCookies site_for_cookies,
        network::mojom::URLLoaderFactoryParamsPtr factory_params)
    : origin(std::move(origin)),
      site_for_cookies(std::move(site_for_cookies)),
      factory_params(std::move(factory_params)) {}
CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    NetworkLoaderFactoryParamsAndInfo(
        CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo&&) = default;
CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    ~NetworkLoaderFactoryParamsAndInfo() = default;

CitizenNotesAgentHostImpl::NetworkLoaderFactoryParamsAndInfo
CitizenNotesAgentHostImpl::CreateNetworkFactoryParamsForCitizenNotes() {
  return {};
}

RenderProcessHost* CitizenNotesAgentHostImpl::GetProcessHost() {
  return nullptr;
}

absl::optional<network::CrossOriginEmbedderPolicy>
CitizenNotesAgentHostImpl::cross_origin_embedder_policy(const std::string& id) {
  return absl::nullopt;
}

absl::optional<network::CrossOriginOpenerPolicy>
CitizenNotesAgentHostImpl::cross_origin_opener_policy(const std::string& id) {
  return absl::nullopt;
}

absl::optional<std::vector<network::mojom::ContentSecurityPolicyHeader>>
CitizenNotesAgentHostImpl::content_security_policy(const std::string& id) {
  return absl::nullopt;
}

protocol::CNTargetAutoAttacher* CitizenNotesAgentHostImpl::auto_attacher() {
  return nullptr;
}

}  // namespace content
