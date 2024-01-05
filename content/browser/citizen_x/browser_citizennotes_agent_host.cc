// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/browser_citizennotes_agent_host.h"

#include "base/clang_profiling_buildflags.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "components/viz/common/buildflags.h"
#include "content/browser/citizen_x/citizennotes_session.h"
#include "content/browser/citizen_x/protocol/cnbrowser_handler.h"
#include "content/browser/citizen_x/protocol/cnfetch_handler.h"
#include "content/browser/citizen_x/protocol/cnio_handler.h"
#include "content/browser/citizen_x/protocol/cnmemory_handler.h"
#include "content/browser/citizen_x/protocol/protocol.h"
#include "content/browser/citizen_x/protocol/cnsecurity_handler.h"
#include "content/browser/citizen_x/protocol/cnstorage_handler.h"
#include "content/browser/citizen_x/protocol/cnsystem_info_handler.h"
#include "content/browser/citizen_x/protocol/cntarget_handler.h"
#include "content/browser/citizen_x/protocol/cntethering_handler.h"
#include "content/browser/citizen_x/protocol/cntracing_handler.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/citizen_x/service_worker_citizennotes_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"

#if BUILDFLAG(USE_VIZ_DEBUGGER)
#include "content/browser/citizen_x/protocol/cnvisual_debugger_handler.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
#include "content/browser/citizen_x/protocol/native_profiling_handler.h"
#endif

namespace content {

scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::CreateForBrowser(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback) {
  return new BrowserCitizenNotesAgentHost(
      tethering_task_runner, socket_callback, false);
}

scoped_refptr<CitizenNotesAgentHost> CitizenNotesAgentHost::CreateForDiscovery() {
  CreateServerSocketCallback null_callback;
  return new BrowserCitizenNotesAgentHost(nullptr, std::move(null_callback), true);
}

namespace {
std::set<BrowserCitizenNotesAgentHost*>& BrowserCitizenNotesAgentHostInstances() {
  static base::NoDestructor<std::set<BrowserCitizenNotesAgentHost*>> instances;
  return *instances;
}

}  // namespace

class BrowserCitizenNotesAgentHost::BrowserAutoAttacher final
    : public protocol::CNTargetAutoAttacher,
      public ServiceWorkerCitizenNotesManager::Observer,
      public CitizenNotesAgentHostObserver {
 public:
  BrowserAutoAttacher() = default;
  ~BrowserAutoAttacher() override = default;

 protected:
  // ServiceWorkerCitizenNotesManager::Observer implementation.
  void WorkerCreated(ServiceWorkerCitizenNotesAgentHost* host,
                     bool* should_pause_on_start) override {
    *should_pause_on_start = wait_for_debugger_on_start();
    DispatchAutoAttach(host, *should_pause_on_start);
  }

  void WorkerDestroyed(ServiceWorkerCitizenNotesAgentHost* host) override {
    DispatchAutoDetach(host);
  }

  void ReattachServiceWorkers() {
    DCHECK(auto_attach());
    ServiceWorkerCitizenNotesAgentHost::List agent_hosts;
    ServiceWorkerCitizenNotesManager::GetInstance()->AddAllAgentHosts(&agent_hosts);
    Hosts new_hosts(agent_hosts.begin(), agent_hosts.end());
    DispatchSetAttachedTargetsOfType(new_hosts,
                                     CitizenNotesAgentHost::kTypeServiceWorker);
  }

  void UpdateAutoAttach(base::OnceClosure callback) override {
    if (auto_attach()) {
      base::AutoReset<bool> auto_reset(&processing_existent_targets_, true);
      if (!have_observers_) {
        ServiceWorkerCitizenNotesManager::GetInstance()->AddObserver(this);
        // CitizenNotesAgentHost's observer immediately notifies about all existing
        // ones.
        CitizenNotesAgentHost::AddObserver(this);
      } else {
        // Manually collect existing hosts to update the list.
        CitizenNotesAgentHost::List hosts;
        RenderFrameCitizenNotesAgentHost::AddAllAgentHosts(&hosts);
        for (auto& host : hosts)
          CitizenNotesAgentHostCreated(host.get());
      }
      ReattachServiceWorkers();
    } else {
      if (have_observers_) {
        CitizenNotesAgentHost::RemoveObserver(this);
        ServiceWorkerCitizenNotesManager::GetInstance()->RemoveObserver(this);
      }
    }
    have_observers_ = auto_attach();
    std::move(callback).Run();
  }

  // CitizenNotesAgentHostObserver overrides.
  void CitizenNotesAgentHostCreated(CitizenNotesAgentHost* host) override {
    DCHECK(auto_attach());
    // In the top level target handler, auto-attach to pages as soon as they
    // are created, otherwise if they don't incur any network activity we'll
    // never get a chance to throttle them (and auto-attach there).
    if (ShouldAttachToTarget(host)) {
      DispatchAutoAttach(
          host, wait_for_debugger_on_start() && !processing_existent_targets_);
    }
  }

  bool ShouldForceCitizenNotesAgentHostCreation() override { return true; }

  static bool ShouldAttachToTarget(CitizenNotesAgentHost* host) {
    if (host->GetType() == CitizenNotesAgentHost::kTypeSharedWorker) {
      return true;
    }
    if (host->GetType() == CitizenNotesAgentHost::kTypeSharedStorageWorklet) {
      return true;
    }
    if (host->GetType() == CitizenNotesAgentHost::kTypeTab) {
      return true;
    }
    return IsMainFrameHost(host);
  }

  static bool IsMainFrameHost(CitizenNotesAgentHost* host) {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(host->GetWebContents());
    if (!web_contents)
      return false;
    FrameTreeNode* frame_tree_node = web_contents->GetPrimaryFrameTree().root();
    if (!frame_tree_node)
      return false;
    return host == RenderFrameCitizenNotesAgentHost::GetFor(frame_tree_node);
  }

  bool processing_existent_targets_ = false;
  bool have_observers_ = false;
};

// static
const std::set<BrowserCitizenNotesAgentHost*>&
BrowserCitizenNotesAgentHost::Instances() {
  return BrowserCitizenNotesAgentHostInstances();
}

BrowserCitizenNotesAgentHost::BrowserCitizenNotesAgentHost(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback,
    bool only_discovery)
    : CitizenNotesAgentHostImpl(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      auto_attacher_(std::make_unique<BrowserAutoAttacher>()),
      tethering_task_runner_(tethering_task_runner),
      socket_callback_(socket_callback),
      only_discovery_(only_discovery) {
  NotifyCreated();
  BrowserCitizenNotesAgentHostInstances().insert(this);
}

BrowserCitizenNotesAgentHost::~BrowserCitizenNotesAgentHost() {
  BrowserCitizenNotesAgentHostInstances().erase(this);
}

bool BrowserCitizenNotesAgentHost::AttachSession(CitizenNotesSession* session,
                                             bool acquire_wake_lock) {
  if (!session->GetClient()->IsTrusted())
    return false;

  session->SetBrowserOnly(true);
  session->CreateAndAddHandler<protocol::CNTargetHandler>(
      protocol::CNTargetHandler::AccessMode::kBrowser, GetId(),
      auto_attacher_.get(), session);
  if (only_discovery_)
    return true;

  session->CreateAndAddHandler<protocol::CNBrowserHandler>(
      session->GetClient()->MayWriteLocalFiles());
#if BUILDFLAG(USE_VIZ_DEBUGGER)
  session->CreateAndAddHandler<protocol::CNVisualDebuggerHandler>();
#endif
  session->CreateAndAddHandler<protocol::CNIOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::CNFetchHandler>(
      GetIOContext(),
      base::BindRepeating([](base::OnceClosure cb) { std::move(cb).Run(); }));
  session->CreateAndAddHandler<protocol::CNMemoryHandler>();
  session->CreateAndAddHandler<protocol::CNSecurityHandler>();
  session->CreateAndAddHandler<protocol::CNStorageHandler>(
      session->GetClient()->IsTrusted());
  session->CreateAndAddHandler<protocol::CNSystemInfoHandler>(
      /* is_browser_sessoin= */ true);
  if (tethering_task_runner_) {
    session->CreateAndAddHandler<protocol::CNTetheringHandler>(
        socket_callback_, tethering_task_runner_);
  }
  session->CreateAndAddHandler<protocol::CNTracingHandler>(
      this, GetIOContext(), /* root_session */ nullptr);

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX) && BUILDFLAG(CLANG_PGO)
  session->CreateAndAddHandler<protocol::CNNativeProfilingHandler>();
#endif

  return true;
}

void BrowserCitizenNotesAgentHost::DetachSession(CitizenNotesSession* session) {
}

protocol::CNTargetAutoAttacher* BrowserCitizenNotesAgentHost::auto_attacher() {
  return auto_attacher_.get();
}

std::string BrowserCitizenNotesAgentHost::GetType() {
  return kTypeBrowser;
}

std::string BrowserCitizenNotesAgentHost::GetTitle() {
  return "";
}

GURL BrowserCitizenNotesAgentHost::GetURL() {
  return GURL();
}

bool BrowserCitizenNotesAgentHost::Activate() {
  return false;
}

bool BrowserCitizenNotesAgentHost::Close() {
  return false;
}

void BrowserCitizenNotesAgentHost::Reload() {
}

}  // content
