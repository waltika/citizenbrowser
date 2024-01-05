// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/chrome_citizennotes_manager_delegate.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/citizen_x/chrome_citizennotes_session.h"
#include "chrome/browser/citizen_x/device/cnandroid_device_manager.h"
#include "chrome/browser/citizen_x/device/cntcp_device_provider.h"
#include "chrome/browser/citizen_x/citizennotes_browser_context_manager.h"
#include "chrome/browser/citizen_x/citizennotes_window.h"
#include "chrome/browser/citizen_x/protocol/cntarget_handler.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/citizennotes_agent_host_client_channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/switches.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

using content::CitizenNotesAgentHost;

const char ChromeCitizenNotesManagerDelegate::kTypeApp[] = "app";
const char ChromeCitizenNotesManagerDelegate::kTypeBackgroundPage[] =
    "background_page";
const char ChromeCitizenNotesManagerDelegate::kTypePage[] = "page";

namespace {

bool GetExtensionInfo(content::WebContents* wc,
                      std::string* name,
                      std::string* type) {
  auto* process_manager =
      extensions::ProcessManager::Get(wc->GetBrowserContext());
  if (!process_manager) {
    return false;
  }
  const extensions::Extension* extension =
      process_manager->GetExtensionForWebContents(wc);
  if (!extension)
    return false;
  extensions::ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension->id());
  if (extension_host && extension_host->host_contents() == wc) {
    *name = extension->name();
    *type = ChromeCitizenNotesManagerDelegate::kTypeBackgroundPage;
    return true;
  }
  if (extension->is_hosted_app() || extension->is_legacy_packaged_app() ||
      extension->is_platform_app()) {
    *name = extension->name();
    *type = ChromeCitizenNotesManagerDelegate::kTypeApp;
    return true;
  }

  auto view_type = extensions::GetViewType(wc);
  if (view_type == extensions::mojom::ViewType::kExtensionPopup ||
      view_type == extensions::mojom::ViewType::kExtensionSidePanel ||
      view_type == extensions::mojom::ViewType::kOffscreenDocument) {
    // Note that we are intentionally not setting name here, so that we can
    // construct a name based on the URL or page title in
    // RenderFrameCitizenNotesAgentHost::GetTitle()
    *type = ChromeCitizenNotesManagerDelegate::kTypePage;
    return true;
  }

  // Set type to other for extensions if not matched previously.
  *type = CitizenNotesAgentHost::kTypeOther;
  return true;
}

ChromeCitizenNotesManagerDelegate* g_instance;

}  // namespace

// static
ChromeCitizenNotesManagerDelegate* ChromeCitizenNotesManagerDelegate::GetInstance() {
  return g_instance;
}

ChromeCitizenNotesManagerDelegate::ChromeCitizenNotesManagerDelegate() {
  DCHECK(!g_instance);
  g_instance = this;

#if !BUILDFLAG(IS_CHROMEOS)
  // Only create and hold keep alive for automation test for non ChromeOS.
  // ChromeOS automation test (aka tast) manages chrome instance via session
  // manager daemon. The extra keep alive is not needed and makes ChromeOS
  // not able to shutdown chrome properly. See https://crbug.com/1174627.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if ((command_line->HasSwitch(switches::kNoStartupWindow) ||
       command_line->HasSwitch(switches::kHeadless)) &&
      (command_line->HasSwitch(switches::kRemoteDebuggingPipe) ||
       command_line->HasSwitch(switches::kRemoteDebuggingPort))) {
    // If running without a startup window with remote debugging,
    // we are controlled entirely by the automation process.
    // Keep the application running until explicit close through CitizenNotes
    // protocol.
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::REMOTE_DEBUGGING, KeepAliveRestartOption::DISABLED);

    // Also keep the initial profile alive so that CNTargetHandler::CreateTarget()
    // can retrieve it without risking disk access even when all pages are
    // closed. Keep-a-living the very first loaded profile looks like a
    // reasonable option.
    if (Profile* profile = ProfileManager::GetLastUsedProfile()) {
      if (profile->IsOffTheRecord()) {
        profile = profile->GetOriginalProfile();
      }
      profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
          profile, ProfileKeepAliveOrigin::kRemoteDebugging);
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

ChromeCitizenNotesManagerDelegate::~ChromeCitizenNotesManagerDelegate() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

void ChromeCitizenNotesManagerDelegate::Inspect(
    content::CitizenNotesAgentHost* agent_host) {
  CitizenNotesWindow::OpenCitizenNotesWindow(agent_host, nullptr,
                                     CitizenNotesOpenedByAction::kInspectLink);
}

void ChromeCitizenNotesManagerDelegate::HandleCommand(
    content::CitizenNotesAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  auto it = sessions_.find(channel);
  if (it == sessions_.end()) {
    std::move(callback).Run(message);
    // This should not happen, but happens. NOTREACHED tries to get
    // a repro in some test.
    NOTREACHED();
    return;
  }
  it->second->HandleCommand(message, std::move(callback));
}

std::string ChromeCitizenNotesManagerDelegate::GetTargetType(
    content::WebContents* web_contents) {
  if (base::Contains(AllTabContentses(), web_contents))
    return CitizenNotesAgentHost::kTypePage;

  std::string extension_name;
  std::string extension_type;
  if (GetExtensionInfo(web_contents, &extension_name, &extension_type)) {
    return extension_type;
  }

  if (views::WebView::IsWebViewContents(web_contents)) {
    return CitizenNotesAgentHost::kTypePage;
  }

  return CitizenNotesAgentHost::kTypeOther;
}

std::string ChromeCitizenNotesManagerDelegate::GetTargetTitle(
    content::WebContents* web_contents) {
  std::string extension_name;
  std::string extension_type;
  if (!GetExtensionInfo(web_contents, &extension_name, &extension_type))
    return std::string();
  return extension_name;
}

bool ChromeCitizenNotesManagerDelegate::AllowInspectingRenderFrameHost(
    content::RenderFrameHost* rfh) {
  Profile* profile =
      Profile::FromBrowserContext(rfh->GetProcess()->GetBrowserContext());
  auto* process_manager = extensions::ProcessManager::Get(profile);
  auto* extension = process_manager
                        ? process_manager->GetExtensionForRenderFrameHost(rfh)
                        : nullptr;
  if (extension || !web_app::AreWebAppsEnabled(profile)) {
    return AllowInspection(profile, extension);
  }

  if (auto* web_app_provider =
          web_app::WebAppProvider::GetForWebApps(profile)) {
    absl::optional<webapps::AppId> app_id =
        web_app_provider->registrar_unsafe().FindAppWithUrlInScope(
            rfh->GetMainFrame()->GetLastCommittedURL());
    if (app_id) {
      const auto* web_app =
          web_app_provider->registrar_unsafe().GetAppById(app_id.value());
      return AllowInspection(profile, web_app);
    }
  }
  // |extension| is always nullptr here.
  return AllowInspection(profile, extension);
}

// static
bool ChromeCitizenNotesManagerDelegate::AllowInspection(
    Profile* profile,
    content::WebContents* web_contents) {
  const extensions::Extension* extension = nullptr;
  if (web_contents) {
    if (auto* process_manager = extensions::ProcessManager::Get(
            web_contents->GetBrowserContext())) {
      extension = process_manager->GetExtensionForWebContents(web_contents);
    }
    if (extension || !web_app::AreWebAppsEnabled(profile)) {
      return AllowInspection(profile, extension);
    }

    const webapps::AppId* app_id =
        web_app::WebAppTabHelper::GetAppId(web_contents);
    auto* web_app_provider =
        web_app::WebAppProvider::GetForWebContents(web_contents);
    if (app_id && web_app_provider) {
      const web_app::WebApp* web_app =
          web_app_provider->registrar_unsafe().GetAppById(*app_id);
      return AllowInspection(profile, web_app);
    }
  }
  // |extension| is always nullptr here.
  return AllowInspection(profile, extension);
}

// static
bool ChromeCitizenNotesManagerDelegate::AllowInspection(
    Profile* profile,
    const extensions::Extension* extension) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
      if (!extension) {
        return true;
      }
      if (extensions::Manifest::IsPolicyLocation(extension->location())) {
        return false;
      }
      // We also disallow inspecting component extensions, but only for managed
      // profiles.
      if (extensions::Manifest::IsComponentLocation(extension->location()) &&
          profile->GetProfilePolicyConnector()->IsManaged()) {
        return false;
      }
      return true;
    default:
      NOTREACHED() << "Unknown developer tools policy";
      return true;
  }
}

// static
bool ChromeCitizenNotesManagerDelegate::AllowInspection(
    Profile* profile,
    const web_app::WebApp* web_app) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
      return !web_app || !web_app->IsKioskInstalledApp();
    default:
      NOTREACHED() << "Unknown developer tools policy";
      return true;
  }
}

void ChromeCitizenNotesManagerDelegate::ClientAttached(
    content::CitizenNotesAgentHostClientChannel* channel) {
  DCHECK(sessions_.find(channel) == sessions_.end());
  sessions_.emplace(channel, std::make_unique<ChromeCitizenNotesSession>(channel));
}

void ChromeCitizenNotesManagerDelegate::ClientDetached(
    content::CitizenNotesAgentHostClientChannel* channel) {
  sessions_.erase(channel);
}

scoped_refptr<CitizenNotesAgentHost> ChromeCitizenNotesManagerDelegate::CreateNewTarget(
    const GURL& url,
    CitizenNotesManagerDelegate::TargetType target_type) {
  NavigateParams params(ProfileManager::GetLastUsedProfile(), url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return nullptr;
  return target_type == CitizenNotesManagerDelegate::kTab
             ? CitizenNotesAgentHost::GetOrCreateForTab(
                   params.navigated_or_inserted_contents)
             : CitizenNotesAgentHost::GetOrCreateFor(
                   params.navigated_or_inserted_contents);
}

std::vector<content::BrowserContext*>
ChromeCitizenNotesManagerDelegate::GetBrowserContexts() {
  return CitizenNotesBrowserContextManager::GetInstance().GetBrowserContexts();
}

content::BrowserContext*
ChromeCitizenNotesManagerDelegate::GetDefaultBrowserContext() {
  return CitizenNotesBrowserContextManager::GetInstance()
      .GetDefaultBrowserContext();
}

content::BrowserContext* ChromeCitizenNotesManagerDelegate::CreateBrowserContext() {
  return CitizenNotesBrowserContextManager::GetInstance().CreateBrowserContext();
}

void ChromeCitizenNotesManagerDelegate::DisposeBrowserContext(
    content::BrowserContext* context,
    DisposeCallback callback) {
  CitizenNotesBrowserContextManager::GetInstance().DisposeBrowserContext(
      context, std::move(callback));
}

bool ChromeCitizenNotesManagerDelegate::HasBundledFrontendResources() {
  return true;
}

void ChromeCitizenNotesManagerDelegate::DevicesAvailable(
    const CitizenNotesDeviceDiscovery::CompleteDevices& devices) {
  CitizenNotesAgentHost::List remote_targets;
  for (const auto& complete : devices) {
    for (const auto& browser : complete.second->browsers()) {
      for (const auto& page : browser->pages())
        remote_targets.push_back(page->CreateTarget());
    }
  }
  remote_agent_hosts_.swap(remote_targets);
}

void ChromeCitizenNotesManagerDelegate::UpdateDeviceDiscovery() {
  RemoteLocations remote_locations;
  for (const auto& it : sessions_) {
    CNTargetHandler* target_handler = it.second->target_handler();
    if (!target_handler)
      continue;
    RemoteLocations& locations = target_handler->remote_locations();
    remote_locations.insert(locations.begin(), locations.end());
  }

  bool equals = remote_locations.size() == remote_locations_.size();
  if (equals) {
    auto it1 = remote_locations.begin();
    auto it2 = remote_locations_.begin();
    while (it1 != remote_locations.end()) {
      DCHECK(it2 != remote_locations_.end());
      if (!(*it1).Equals(*it2))
        equals = false;
      ++it1;
      ++it2;
    }
    DCHECK(it2 == remote_locations_.end());
  }

  if (equals)
    return;

  if (remote_locations.empty()) {
    device_discovery_.reset();
    remote_agent_hosts_.clear();
  } else {
    if (!device_manager_)
      device_manager_ = CNAndroidDeviceManager::Create();

    CNAndroidDeviceManager::DeviceProviders providers;
    providers.push_back(new CNTCPDeviceProvider(remote_locations));
    device_manager_->SetDeviceProviders(providers);

    device_discovery_ = std::make_unique<CitizenNotesDeviceDiscovery>(
        device_manager_.get(),
        base::BindRepeating(&ChromeCitizenNotesManagerDelegate::DevicesAvailable,
                            base::Unretained(this)));
  }
  remote_locations_.swap(remote_locations);
}

void ChromeCitizenNotesManagerDelegate::ResetAndroidDeviceManagerForTesting() {
  device_manager_.reset();

  // We also need |device_discovery_| to go away because there may be a pending
  // task using a raw pointer to the DeviceManager we just deleted.
  device_discovery_.reset();
}

// static
void ChromeCitizenNotesManagerDelegate::CloseBrowserSoon() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        if (GetInstance()) {
          // Do not keep the application running anymore, we got an explicit
          // request to close.
          GetInstance()->keep_alive_.reset();
        }
        chrome::ExitIgnoreUnloadHandlers();
      }));
}
