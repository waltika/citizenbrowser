// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizennotes_window.h"

#include <memory>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/citizen_x/chrome_citizennotes_manager_delegate.h"
#include "chrome/browser/citizen_x/citizennotes_eye_dropper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/webui/citizen_x/citizennotes_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/common/url_constants.h"
#include "net/cert/x509_certificate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

using blink::WebInputEvent;
using content::BrowserThread;
using content::CitizenNotesAgentHost;
using content::WebContents;

namespace {

typedef std::vector<CitizenNotesWindow*> CitizenNotesWindows;
base::LazyInstance<CitizenNotesWindows>::Leaky g_citizennotes_window_instances =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<
    std::vector<base::RepeatingCallback<void(CitizenNotesWindow*)>>>::Leaky
    g_creation_callbacks = LAZY_INSTANCE_INITIALIZER;

static const char kKeyUpEventName[] = "keyup";
static const char kKeyDownEventName[] = "keydown";
static const char kDefaultFrontendURL[] =
    "citizen_x://citizen_x/bundled/citizennotes_app.html";
static const char kNodeFrontendURL[] =
    "citizen_x://citizen_x/bundled/node_app.html";
static const char kWorkerFrontendURL[] =
    "citizen_x://citizen_x/bundled/worker_app.html";
static const char kJSFrontendURL[] = "citizen_x://citizen_x/bundled/js_app.html";
static const char kFallbackFrontendURL[] =
    "citizen_x://citizen_x/bundled/inspector.html";

bool FindInspectedBrowserAndTabIndex(
    WebContents* inspected_web_contents, Browser** browser, int* tab) {
  if (!inspected_web_contents)
    return false;

  for (auto* b : *BrowserList::GetInstance()) {
    int tab_index =
        b->tab_strip_model()->GetIndexOfWebContents(inspected_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      *browser = b;
      *tab = tab_index;
      return true;
    }
  }
  return false;
}

void SetPreferencesFromJson(Profile* profile, const std::string& json) {
  absl::optional<base::Value> parsed = base::JSONReader::Read(json);
  if (!parsed || !parsed->is_dict())
    return;
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kCitizenNotesPreferences);
  for (auto dict_value : parsed->GetDict()) {
    if (!dict_value.second.is_string())
      continue;
    update->Set(dict_value.first, std::move(dict_value.second));
  }
}

// CitizenNotesToolboxDelegate ----------------------------------------------------

class CitizenNotesToolboxDelegate
    : public content::WebContentsObserver,
      public content::WebContentsDelegate {
 public:
  CitizenNotesToolboxDelegate(WebContents* toolbox_contents,
                          base::WeakPtr<WebContents> inspected_web_contents);

  CitizenNotesToolboxDelegate(const CitizenNotesToolboxDelegate&) = delete;
  CitizenNotesToolboxDelegate& operator=(const CitizenNotesToolboxDelegate&) = delete;

  ~CitizenNotesToolboxDelegate() override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void WebContentsDestroyed() override;

 private:
  BrowserWindow* GetInspectedBrowserWindow();
  base::WeakPtr<content::WebContents> inspected_web_contents_;
};

CitizenNotesToolboxDelegate::CitizenNotesToolboxDelegate(
    WebContents* toolbox_contents,
    base::WeakPtr<WebContents> web_contents)
    : WebContentsObserver(toolbox_contents),
      inspected_web_contents_(web_contents) {}

CitizenNotesToolboxDelegate::~CitizenNotesToolboxDelegate() {
}

content::WebContents* CitizenNotesToolboxDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == web_contents());
  if (!params.url.SchemeIs(content::kChromeCitizenNotesScheme))
    return nullptr;
  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return source;
}

content::KeyboardEventProcessingResult
CitizenNotesToolboxDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->PreHandleKeyboardEvent(event);
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool CitizenNotesToolboxDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return false;
  }
  BrowserWindow* window = GetInspectedBrowserWindow();
  return window && window->HandleKeyboardEvent(event);
}

void CitizenNotesToolboxDelegate::WebContentsDestroyed() {
  delete this;
}

BrowserWindow* CitizenNotesToolboxDelegate::GetInspectedBrowserWindow() {
  if (!inspected_web_contents_)
    return nullptr;
  Browser* browser = nullptr;
  int tab = 0;
  if (FindInspectedBrowserAndTabIndex(inspected_web_contents_.get(), &browser,
                                      &tab))
    return browser->window();
  return nullptr;
}

// static
GURL DecorateFrontendURL(const GURL& base_url) {
  std::string frontend_url = base_url.spec();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kCitizenNotesFlags)) {
    frontend_url = frontend_url +
                   ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
                   command_line->GetSwitchValueASCII(switches::kCitizenNotesFlags);
  }

  if (command_line->HasSwitch(switches::kCustomCitizenNotesFrontend)) {
    frontend_url = frontend_url +
                   ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
                   "debugFrontend=true";
  }

  return GURL(frontend_url);
}

}  // namespace

// CitizenNotesEventForwarder -----------------------------------------------------

class CitizenNotesEventForwarder {
 public:
  explicit CitizenNotesEventForwarder(CitizenNotesWindow* window)
     : citizennotes_window_(window) {}

  CitizenNotesEventForwarder(const CitizenNotesEventForwarder&) = delete;
  CitizenNotesEventForwarder& operator=(const CitizenNotesEventForwarder&) = delete;

  // Registers whitelisted shortcuts with the forwarder.
  // Only registered keys will be forwarded to the CitizenNotes frontend.
  void SetWhitelistedShortcuts(const std::string& message);

  // Forwards a keyboard event to the CitizenNotes frontend if it is whitelisted.
  // Returns |true| if the event has been forwarded, |false| otherwise.
  bool ForwardEvent(const content::NativeWebKeyboardEvent& event);

 private:
  static bool KeyWhitelistingAllowed(int key_code, int modifiers);
  static int CombineKeyCodeAndModifiers(int key_code, int modifiers);

  RAW_PTR_EXCLUSION CitizenNotesWindow* citizennotes_window_;
  std::set<int> whitelisted_keys_;
};

void CitizenNotesEventForwarder::SetWhitelistedShortcuts(
    const std::string& message) {
  absl::optional<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message || !parsed_message->is_list())
    return;
  for (const auto& list_item : parsed_message->GetList()) {
    if (!list_item.is_dict())
      continue;
    int key_code = list_item.GetDict().FindInt("keyCode").value_or(0);
    if (key_code == 0)
      continue;
    int modifiers = list_item.GetDict().FindInt("modifiers").value_or(0);
    if (!KeyWhitelistingAllowed(key_code, modifiers)) {
      LOG(WARNING) << "Key whitelisting forbidden: "
                   << "(" << key_code << "," << modifiers << ")";
      continue;
    }
    whitelisted_keys_.insert(CombineKeyCodeAndModifiers(key_code, modifiers));
  }
}

bool CitizenNotesEventForwarder::ForwardEvent(
    const content::NativeWebKeyboardEvent& event) {
  std::string event_type;
  switch (event.GetType()) {
    case WebInputEvent::Type::kKeyDown:
    case WebInputEvent::Type::kRawKeyDown:
      event_type = kKeyDownEventName;
      break;
    case WebInputEvent::Type::kKeyUp:
      event_type = kKeyUpEventName;
      break;
    default:
      return false;
  }

  int key_code = ui::LocatedToNonLocatedKeyboardCode(
      static_cast<ui::KeyboardCode>(event.windows_key_code));
  int modifiers = event.GetModifiers() &
                  (WebInputEvent::kShiftKey | WebInputEvent::kControlKey |
                   WebInputEvent::kAltKey | WebInputEvent::kMetaKey);
  int key = CombineKeyCodeAndModifiers(key_code, modifiers);
  if (whitelisted_keys_.find(key) == whitelisted_keys_.end())
    return false;

  base::Value::Dict event_data;
  event_data.Set("type", event_type);
  event_data.Set("key", ui::KeycodeConverter::DomKeyToKeyString(
                            static_cast<ui::DomKey>(event.dom_key)));
  event_data.Set("code", ui::KeycodeConverter::DomCodeToCodeString(
                             static_cast<ui::DomCode>(event.dom_code)));
  event_data.Set("keyCode", key_code);
  event_data.Set("modifiers", modifiers);
  citizennotes_window_->bindings_->CallClientMethod(
      "CitizenNotesAPI", "keyEventUnhandled", base::Value(std::move(event_data)));
  return true;
}

int CitizenNotesEventForwarder::CombineKeyCodeAndModifiers(int key_code,
                                                       int modifiers) {
  return key_code | (modifiers << 16);
}

bool CitizenNotesEventForwarder::KeyWhitelistingAllowed(int key_code,
                                                    int modifiers) {
  return (ui::VKEY_F1 <= key_code && key_code <= ui::VKEY_F12) ||
      modifiers != 0;
}

void CitizenNotesWindow::OpenNodeFrontend() {
  CitizenNotesWindow::OpenNodeFrontendWindow(
      profile_, CitizenNotesOpenedByAction::kOpenForNodeFromAnotherTarget);
}

// CitizenNotesWindow::Throttle ------------------------------------------

class CitizenNotesWindow::Throttle : public content::NavigationThrottle {
 public:
  Throttle(content::NavigationHandle* navigation_handle,
           CitizenNotesWindow* citizennotes_window)
      : content::NavigationThrottle(navigation_handle),
        citizennotes_window_(citizennotes_window) {
    citizennotes_window_->throttle_ = this;
  }

  Throttle(const Throttle&) = delete;
  Throttle& operator=(const Throttle&) = delete;

  ~Throttle() override {
    if (citizennotes_window_)
      citizennotes_window_->throttle_ = nullptr;
  }

  // content::NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return DEFER;
  }

  const char* GetNameForLogging() override { return "CitizenNotesWindowThrottle"; }

  void ResumeThrottle() {
    if (citizennotes_window_) {
      citizennotes_window_->throttle_ = nullptr;
      citizennotes_window_ = nullptr;
    }
    Resume();
  }

 private:
  RAW_PTR_EXCLUSION CitizenNotesWindow* citizennotes_window_;
};

// Helper class that holds the owned main WebContents for the docked
// citizennotes window and maintains a keepalive object that keeps the browser
// main loop alive long enough for the WebContents to clean up properly.
class CitizenNotesWindow::OwnedMainWebContents {
 public:
  explicit OwnedMainWebContents(
      std::unique_ptr<content::WebContents> web_contents)
      : keep_alive_(KeepAliveOrigin::CITIZENNOTES_WINDOW,
                    KeepAliveRestartOption::DISABLED),
        web_contents_(std::move(web_contents)) {
    Profile* profile = GetProfileForCitizenNotesWindow(web_contents_.get());
    DCHECK(profile);
    if (!profile->IsOffTheRecord()) {
      // ScopedProfileKeepAlive does not support OTR profiles.
      profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
          profile, ProfileKeepAliveOrigin::kCitizenNotesWindow);
    }
  }

  static std::unique_ptr<content::WebContents> TakeWebContents(
      std::unique_ptr<OwnedMainWebContents> instance) {
    return std::move(instance->web_contents_);
  }

 private:
  ScopedKeepAlive keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// CitizenNotesWindow -------------------------------------------------------------

const char CitizenNotesWindow::kCitizenNotesApp[] = "CitizenNotesApp";

// static
void CitizenNotesWindow::AddCreationCallbackForTest(
    const CreationCallback& callback) {
  g_creation_callbacks.Get().push_back(callback);
}

// static
void CitizenNotesWindow::RemoveCreationCallbackForTest(
    const CreationCallback& callback) {
  for (size_t i = 0; i < g_creation_callbacks.Get().size(); ++i) {
    if (g_creation_callbacks.Get().at(i) == callback) {
      g_creation_callbacks.Get().erase(g_creation_callbacks.Get().begin() + i);
      return;
    }
  }
}

CitizenNotesWindow::~CitizenNotesWindow() {
  if (throttle_)
    throttle_->ResumeThrottle();

  life_stage_ = kClosing;

  base::RecordAction(base::UserMetricsAction("CitizenNotes_Close"));

  UpdateBrowserWindow();
  UpdateBrowserToolbar();

  capture_handle_.RunAndReset();
  owned_toolbox_web_contents_.reset();

  CitizenNotesWindows* instances = g_citizennotes_window_instances.Pointer();
  auto it = base::ranges::find(*instances, this);
  DCHECK(it != instances->end());
  instances->erase(it);

  if (!close_callback_.is_null())
    std::move(close_callback_).Run();
  // Defer deletion of the main web contents, since we could get here
  // via RenderFrameHostImpl method that expects WebContents to live
  // for some time. See http://crbug.com/997299 for details.
  if (owned_main_web_contents_) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(owned_main_web_contents_));
  }

  // This should be run after we remove |this| from
  // |g_citizennotes_window_instances| as |reattach_complete_callback| may try to
  // access it.
  if (reattach_complete_callback_) {
    std::move(reattach_complete_callback_).Run();
  }

  // If window gets destroyed during a test run, need to stop the test.
  if (!ready_for_test_callback_.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(ready_for_test_callback_));
  }
}

// static
void CitizenNotesWindow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kCitizenNotesEditedFiles);
  registry->RegisterDictionaryPref(prefs::kCitizenNotesFileSystemPaths);
  registry->RegisterStringPref(prefs::kCitizenNotesAdbKey, std::string());

  registry->RegisterBooleanPref(prefs::kCitizenNotesDiscoverUsbDevicesEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kCitizenNotesPortForwardingEnabled, false);
  registry->RegisterBooleanPref(prefs::kCitizenNotesPortForwardingDefaultSet,
                                false);
  registry->RegisterDictionaryPref(prefs::kCitizenNotesPortForwardingConfig);
  registry->RegisterBooleanPref(prefs::kCitizenNotesDiscoverTCPTargetsEnabled,
                                true);
  registry->RegisterListPref(prefs::kCitizenNotesTCPDiscoveryConfig);
  registry->RegisterDictionaryPref(prefs::kCitizenNotesPreferences);
  registry->RegisterBooleanPref(
      prefs::kCitizenNotesSyncPreferences,
      CitizenNotesSettings::kSyncCitizenNotesPreferencesDefault,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCitizenNotesSyncedPreferencesSyncEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kCitizenNotesSyncedPreferencesSyncDisabled);
}

// static
content::WebContents* CitizenNotesWindow::GetInTabWebContents(
    WebContents* inspected_web_contents,
    CitizenNotesContentsResizingStrategy* out_strategy) {
  CitizenNotesWindow* window = GetInstanceForInspectedWebContents(
      inspected_web_contents);
  if (!window || window->life_stage_ == kClosing)
    return nullptr;

  // Not yet loaded window is treated as docked, but we should not present it
  // until we decided on docking.
  bool is_docked_set = window->life_stage_ == kLoadCompleted ||
      window->life_stage_ == kIsDockedSet;
  if (!is_docked_set)
    return nullptr;

  // Undocked window should have toolbox web contents.
  if (!window->is_docked_ && !window->toolbox_web_contents_)
    return nullptr;

  if (out_strategy)
    out_strategy->CopyFrom(window->contents_resizing_strategy_);

  return window->is_docked_ ? window->main_web_contents_ :
      window->toolbox_web_contents_;
}

// static
CitizenNotesWindow* CitizenNotesWindow::GetInstanceForInspectedWebContents(
    WebContents* inspected_web_contents) {
  if (!inspected_web_contents || !g_citizennotes_window_instances.IsCreated())
    return nullptr;
  CitizenNotesWindows* instances = g_citizennotes_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->GetInspectedWebContents() == inspected_web_contents)
      return *it;
  }
  return nullptr;
}

// static
bool CitizenNotesWindow::IsCitizenNotesWindow(content::WebContents* web_contents) {
  if (!web_contents || !g_citizennotes_window_instances.IsCreated())
    return false;
  CitizenNotesWindows* instances = g_citizennotes_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->main_web_contents_ == web_contents ||
        (*it)->toolbox_web_contents_ == web_contents)
      return true;
  }
  return false;
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindowForWorker(
    Profile* profile,
    const scoped_refptr<CitizenNotesAgentHost>& worker_agent,
    CitizenNotesOpenedByAction opened_by) {
  CitizenNotesWindow* window = FindCitizenNotesWindow(worker_agent.get());
  if (!window) {
    base::RecordAction(base::UserMetricsAction("CitizenNotes_InspectWorker"));
    window = Create(profile, nullptr, kFrontendWorker, std::string(), false, "",
                    "", worker_agent->IsAttached(),
                    /* browser_connection */ true, opened_by);
    if (!window)
      return;
    window->bindings_->AttachViaBrowserTarget(worker_agent);
  }
  window->ScheduleShow(CitizenNotesToggleAction::Show());
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindow(
    content::WebContents* inspected_web_contents,
    CitizenNotesOpenedByAction opened_by) {
  ToggleCitizenNotesWindow(inspected_web_contents, nullptr, true,
                       CitizenNotesToggleAction::Show(), "", opened_by);
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindow(
    content::WebContents* inspected_web_contents,
    Profile* profile,
    CitizenNotesOpenedByAction opened_by) {
  ToggleCitizenNotesWindow(inspected_web_contents, profile, true,
                       CitizenNotesToggleAction::Show(), "");
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindow(
    scoped_refptr<content::CitizenNotesAgentHost> agent_host,
    Profile* profile,
    CitizenNotesOpenedByAction opened_by) {
  OpenCitizenNotesWindow(agent_host, profile, false /* use_bundled_frontend */,
                     opened_by);
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindowWithBundledFrontend(
    scoped_refptr<content::CitizenNotesAgentHost> agent_host,
    Profile* profile,
    CitizenNotesOpenedByAction opened_by) {
  OpenCitizenNotesWindow(agent_host, profile, true /* use_bundled_frontend */,
                     opened_by);
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindow(
    scoped_refptr<content::CitizenNotesAgentHost> agent_host,
    Profile* profile,
    bool use_bundled_frontend,
    CitizenNotesOpenedByAction opened_by) {
  if (!profile)
    profile = Profile::FromBrowserContext(agent_host->GetBrowserContext());

  if (!profile)
    return;

  std::string type = agent_host->GetType();

  bool is_worker = type == CitizenNotesAgentHost::kTypeServiceWorker ||
                   type == CitizenNotesAgentHost::kTypeSharedWorker ||
                   type == CitizenNotesAgentHost::kTypeSharedStorageWorklet;

  if (!agent_host->GetFrontendURL().empty()) {
    CitizenNotesWindow::OpenExternalFrontend(profile, agent_host->GetFrontendURL(),
                                         agent_host, use_bundled_frontend,
                                         opened_by);
    return;
  }

  if (is_worker) {
    CitizenNotesWindow::OpenCitizenNotesWindowForWorker(profile, agent_host, opened_by);
    return;
  }

  if (type == content::CitizenNotesAgentHost::kTypeFrame) {
    CitizenNotesWindow::OpenCitizenNotesWindowForFrame(profile, agent_host, opened_by);
    return;
  }

  content::WebContents* web_contents = agent_host->GetWebContents();
  if (web_contents)
    CitizenNotesWindow::OpenCitizenNotesWindow(web_contents, profile, opened_by);
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindow(
    content::WebContents* inspected_web_contents,
    const CitizenNotesToggleAction& action,
    CitizenNotesOpenedByAction opened_by) {
  ToggleCitizenNotesWindow(inspected_web_contents, nullptr, true, action, "",
                       opened_by);
}

// static
void CitizenNotesWindow::OpenCitizenNotesWindowForFrame(
    Profile* profile,
    const scoped_refptr<content::CitizenNotesAgentHost>& agent_host,
    CitizenNotesOpenedByAction opened_by) {
  CitizenNotesWindow* window = FindCitizenNotesWindow(agent_host.get());
  if (!window) {
    window = CitizenNotesWindow::Create(
        profile, nullptr, kFrontendDefault, std::string(), false, std::string(),
        std::string(), agent_host->IsAttached(), false, opened_by);
    if (!window)
      return;
    window->bindings_->AttachTo(agent_host);
  }
  window->ScheduleShow(CitizenNotesToggleAction::Show());
}

// static
void CitizenNotesWindow::ToggleCitizenNotesWindow(Browser* browser,
                                          const CitizenNotesToggleAction& action,
                                          CitizenNotesOpenedByAction opened_by) {
  if (action.type() == CitizenNotesToggleAction::kToggle &&
      browser->is_type_citizennotes()) {
    browser->tab_strip_model()->CloseAllTabs();
    return;
  }

  ToggleCitizenNotesWindow(browser->tab_strip_model()->GetActiveWebContents(),
                       nullptr, action.type() == CitizenNotesToggleAction::kInspect,
                       action, "", opened_by);
}

// static
void CitizenNotesWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    const scoped_refptr<content::CitizenNotesAgentHost>& agent_host,
    bool use_bundled_frontend,
    CitizenNotesOpenedByAction opened_by) {
  CitizenNotesWindow* window = FindCitizenNotesWindow(agent_host.get());
  if (window) {
    window->ScheduleShow(CitizenNotesToggleAction::Show());
    return;
  }

  std::string type = agent_host->GetType();
  if (type == "node") {
    // Direct node targets will always open using ToT front-end.
    window = Create(profile, nullptr, kFrontendV8, std::string(), false,
                    std::string(), std::string(), agent_host->IsAttached(),
                    /* browser_connection */ false, opened_by);
  } else {
    bool is_worker = type == CitizenNotesAgentHost::kTypeServiceWorker ||
                     type == CitizenNotesAgentHost::kTypeSharedWorker ||
                     type == CitizenNotesAgentHost::kTypeSharedStorageWorklet;

    FrontendType frontend_type =
        is_worker ? kFrontendRemoteWorker : kFrontendRemote;
    std::string effective_frontend_url =
        use_bundled_frontend ? kFallbackFrontendURL
                             : CitizenNotesUI::GetProxyURL(frontend_url).spec();
    if (type == "tab") {
      if (effective_frontend_url.find("?") == std::string::npos) {
        effective_frontend_url += "?targetType=tab";
      } else {
        effective_frontend_url += "&targetType=tab";
      }
    }
    window =
        Create(profile, nullptr, frontend_type, effective_frontend_url, false,
               std::string(), std::string(), agent_host->IsAttached(),
               /* browser_connection */ false, opened_by);
  }
  if (!window)
    return;
  window->bindings_->AttachTo(agent_host);
  window->close_on_detach_ = false;
  window->ScheduleShow(CitizenNotesToggleAction::Show());
}

// static
CitizenNotesWindow* CitizenNotesWindow::OpenNodeFrontendWindow(
    Profile* profile,
    CitizenNotesOpenedByAction opened_by) {
  for (CitizenNotesWindow* window : g_citizennotes_window_instances.Get()) {
    if (window->frontend_type_ == kFrontendNode) {
      window->ActivateWindow();
      return window;
    }
  }

  CitizenNotesWindow* window = Create(
      profile, nullptr, kFrontendNode, std::string(), false, std::string(),
      std::string(), false, /* browser_connection */ false, opened_by);
  if (!window)
    return nullptr;
  window->bindings_->AttachTo(CitizenNotesAgentHost::CreateForDiscovery());
  window->ScheduleShow(CitizenNotesToggleAction::Show());
  return window;
}

// static
Profile* CitizenNotesWindow::GetProfileForCitizenNotesWindow(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsPrimaryOTRProfile()) {
    return profile;
  }
  return profile->GetOriginalProfile();
}

namespace {

scoped_refptr<CitizenNotesAgentHost> GetOrCreateCitizenNotesHostForWebContents(
    WebContents* wc) {
  return base::FeatureList::IsEnabled(::features::kCitizenNotesTabTarget)
             ? CitizenNotesAgentHost::GetOrCreateForTab(wc)
             : CitizenNotesAgentHost::GetOrCreateFor(wc);
}

}  // namespace

// static
void CitizenNotesWindow::ToggleCitizenNotesWindow(
    content::WebContents* inspected_web_contents,
    Profile* profile,
    bool force_open,
    const CitizenNotesToggleAction& action,
    const std::string& settings,
    CitizenNotesOpenedByAction opened_by) {
  scoped_refptr<CitizenNotesAgentHost> agent(
      GetOrCreateCitizenNotesHostForWebContents(inspected_web_contents));
  CitizenNotesWindow* window = FindCitizenNotesWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    if (!profile)
      profile = GetProfileForCitizenNotesWindow(inspected_web_contents);
    base::RecordAction(base::UserMetricsAction("CitizenNotes_InspectRenderer"));
    std::string panel;
    switch (action.type()) {
      case CitizenNotesToggleAction::kInspect:
      case CitizenNotesToggleAction::kShowElementsPanel:
        panel = "elements";
        break;
      case CitizenNotesToggleAction::kShowConsolePanel:
        panel = "console";
        break;
      case CitizenNotesToggleAction::kPauseInDebugger:
        panel = "sources";
        break;
      case CitizenNotesToggleAction::kShow:
      case CitizenNotesToggleAction::kToggle:
      case CitizenNotesToggleAction::kReveal:
      case CitizenNotesToggleAction::kNoOp:
        break;
    }
    window = Create(profile, inspected_web_contents, kFrontendDefault,
                    std::string(), true, settings, panel, agent->IsAttached(),
                    /* browser_connection */ false, opened_by);
    if (!window)
      return;
    window->bindings_->AttachTo(agent.get());
    do_open = true;
    if (opened_by != CitizenNotesOpenedByAction::kUnknown) {
      LogCitizenNotesOpenedByAction(opened_by);
      LogCitizenNotesOpenedUKM(inspected_web_contents);
    }
  }

  // Update toolbar to reflect CitizenNotes changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it.
  if (!window->is_docked_ || do_open)
    window->ScheduleShow(action);
  else
    window->CloseWindow();
}

// static
void CitizenNotesWindow::InspectElement(
    content::RenderFrameHost* inspected_frame_host,
    int x,
    int y) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(inspected_frame_host);
  scoped_refptr<CitizenNotesAgentHost> agent(
      GetOrCreateCitizenNotesHostForWebContents(web_contents));
  agent->InspectElement(inspected_frame_host, x, y);
  bool should_measure_time = !FindCitizenNotesWindow(agent.get());
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(loislo): we should initiate CitizenNotes window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  OpenCitizenNotesWindow(web_contents, CitizenNotesToggleAction::ShowElementsPanel(),
                     CitizenNotesOpenedByAction::kContextMenuInspect);
  CitizenNotesWindow* window = FindCitizenNotesWindow(agent.get());
  if (window && should_measure_time)
    window->inspect_element_start_time_ = start_time;
}

// static
void CitizenNotesWindow::LogCitizenNotesOpenedByAction(
    CitizenNotesOpenedByAction opened_by) {
  base::UmaHistogramEnumeration("CitizenNotes.OpenedByAction", opened_by);
}

// static
void CitizenNotesWindow::LogCitizenNotesOpenedUKM(content::WebContents* web_contents) {
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::CitizenNotes_Opened(source_id).SetHasOccurred(true).Record(
      ukm::UkmRecorder::Get());
}

// static
std::unique_ptr<content::NavigationThrottle>
CitizenNotesWindow::MaybeCreateNavigationThrottle(
    content::NavigationHandle* handle) {
  WebContents* web_contents = handle->GetWebContents();
  if (!web_contents || !web_contents->HasLiveOriginalOpenerChain() ||
      !web_contents->GetController()
           .GetLastCommittedEntry()
           ->IsInitialEntry()) {
    return nullptr;
  }

  WebContents* opener =
      handle->GetWebContents()->GetFirstWebContentsInLiveOriginalOpenerChain();
  CitizenNotesWindow* window = GetInstanceForInspectedWebContents(opener);
  if (!window || !window->open_new_window_for_popups_ ||
      GetInstanceForInspectedWebContents(web_contents))
    return nullptr;

  CitizenNotesWindow::OpenCitizenNotesWindow(
      web_contents, CitizenNotesOpenedByAction::kAutomaticForNewTarget);
  window = GetInstanceForInspectedWebContents(web_contents);
  if (!window)
    return nullptr;

  return std::make_unique<Throttle>(handle, window);
}

// TODO(caseq): this method should be removed once we switch to
// using tab target, so we don't currently use tab target here.
void CitizenNotesWindow::UpdateInspectedWebContents(
    content::WebContents* new_web_contents,
    base::OnceCallback<void()> callback) {
  DCHECK(!reattach_complete_callback_);
  reattach_complete_callback_ = std::move(callback);

  inspected_web_contents_ = new_web_contents->GetWeakPtr();
  bindings_->AttachTo(
      content::CitizenNotesAgentHost::GetOrCreateFor(new_web_contents));
  bindings_->CallClientMethod(
      "CitizenNotesAPI", "reattachMainTarget", {}, {}, {},
      base::BindOnce(&CitizenNotesWindow::OnReattachMainTargetComplete,
                     base::Unretained(this)));
}

void CitizenNotesWindow::ScheduleShow(const CitizenNotesToggleAction& action) {
  if (life_stage_ == kLoadCompleted) {
    Show(action);
    return;
  }

  // Action will be done only after load completed.
  action_on_load_ = action;

  if (!can_dock_) {
    // No harm to show always-undocked window right away.
    is_docked_ = false;
    Show(CitizenNotesToggleAction::Show());
  }
}

void CitizenNotesWindow::Show(const CitizenNotesToggleAction& action) {
  if (life_stage_ == kClosing)
    return;

  if (action.type() == CitizenNotesToggleAction::kNoOp)
    return;
  if (is_docked_) {
    DCHECK(can_dock_);
    Browser* inspected_browser = nullptr;
    int inspected_tab_index = -1;
    FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                    &inspected_browser,
                                    &inspected_tab_index);
    DCHECK(inspected_browser);
    DCHECK_NE(-1, inspected_tab_index);

    RegisterModalDialogManager(inspected_browser);

    // Tell inspected browser to update splitter and switch to inspected panel.
    BrowserWindow* inspected_window = inspected_browser->window();
    main_web_contents_->SetDelegate(this);

    TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
    tab_strip_model->ActivateTabAt(
        inspected_tab_index,
        TabStripUserGestureDetails(
            TabStripUserGestureDetails::GestureType::kOther));

    inspected_window->UpdateCitizenNotes();
    main_web_contents_->SetInitialFocus();
    inspected_window->Show();
    // On Aura, focusing once is not enough. Do it again.
    // Note that focusing only here but not before isn't enough either. We just
    // need to focus twice.
    main_web_contents_->SetInitialFocus();

    PrefsTabHelper::CreateForWebContents(main_web_contents_);
    OverrideAndSyncCitizenNotesRendererPrefs();

    DoAction(action);
    return;
  }

  // Avoid consecutive window switching if the citizennotes window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || (action.type() != CitizenNotesToggleAction::kInspect);

  if (!browser_)
    CreateCitizenNotesBrowser();

  // Ignore action if browser does not exist and could not be created.
  if (!browser_)
    return;

  RegisterModalDialogManager(browser_);

  if (should_show_window) {
    browser_->window()->Show();
    main_web_contents_->SetInitialFocus();
  }
  if (toolbox_web_contents_)
    UpdateBrowserWindow();

  DoAction(action);
}

// static
bool CitizenNotesWindow::HandleBeforeUnload(WebContents* frontend_contents,
    bool proceed, bool* proceed_to_fire_unload) {
  CitizenNotesWindow* window = AsCitizenNotesWindow(frontend_contents);
  if (!window)
    return false;
  if (!window->intercepted_page_beforeunload_)
    return false;
  window->BeforeUnloadFired(frontend_contents, proceed,
      proceed_to_fire_unload);
  return true;
}

// static
bool CitizenNotesWindow::InterceptPageBeforeUnload(WebContents* contents) {
  CitizenNotesWindow* window =
      CitizenNotesWindow::GetInstanceForInspectedWebContents(contents);
  if (!window || window->intercepted_page_beforeunload_)
    return false;

  // Not yet loaded frontend will not handle beforeunload.
  if (window->life_stage_ != kLoadCompleted)
    return false;

  window->intercepted_page_beforeunload_ = true;
  // Handle case of citizennotes inspecting another citizennotes instance by passing
  // the call up to the inspecting citizennotes instance.
  // TODO(chrisha): Make citizennotes handle |auto_cancel=false| unload handler
  // dispatches; otherwise, discarding queries can cause unload dialogs to
  // pop-up for tabs with an attached citizennotes.
  if (!CitizenNotesWindow::InterceptPageBeforeUnload(window->main_web_contents_)) {
    window->main_web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  }
  return true;
}

// static
bool CitizenNotesWindow::NeedsToInterceptBeforeUnload(
    WebContents* contents) {
  CitizenNotesWindow* window =
      CitizenNotesWindow::GetInstanceForInspectedWebContents(contents);
  return window && !window->intercepted_page_beforeunload_ &&
         window->life_stage_ == kLoadCompleted;
}

// static
bool CitizenNotesWindow::HasFiredBeforeUnloadEventForCitizenNotesBrowser(
    Browser* browser) {
  DCHECK(browser->is_type_citizennotes());
  // When FastUnloadController is used, citizennotes frontend will be detached
  // from the browser window at this point which means we've already fired
  // beforeunload.
  if (browser->tab_strip_model()->empty())
    return true;
  CitizenNotesWindow* window = AsCitizenNotesWindow(browser);
  if (!window)
    return false;
  return window->intercepted_page_beforeunload_;
}

// static
void CitizenNotesWindow::OnPageCloseCanceled(WebContents* contents) {
  CitizenNotesWindow* window =
      CitizenNotesWindow::GetInstanceForInspectedWebContents(contents);
  if (!window)
    return;
  window->intercepted_page_beforeunload_ = false;
  // Propagate to citizennotes opened on citizennotes if any.
  CitizenNotesWindow::OnPageCloseCanceled(window->main_web_contents_);
}

CitizenNotesWindow::CitizenNotesWindow(FrontendType frontend_type,
                               Profile* profile,
                               std::unique_ptr<WebContents> main_web_contents,
                               CitizenNotesUIBindings* bindings,
                               WebContents* inspected_web_contents,
                               bool can_dock,
                               CitizenNotesOpenedByAction opened_by)
    : frontend_type_(frontend_type),
      profile_(profile),
      main_web_contents_(main_web_contents.get()),
      toolbox_web_contents_(nullptr),
      bindings_(bindings),
      browser_(nullptr),
      is_docked_(true),
      owned_main_web_contents_(
          std::make_unique<OwnedMainWebContents>(std::move(main_web_contents))),
      can_dock_(can_dock),
      close_on_detach_(true),
      // This initialization allows external front-end to work without changes.
      // We don't wait for docking call, but instead immediately show undocked.
      life_stage_(can_dock ? kNotLoaded : kIsDockedSet),
      action_on_load_(CitizenNotesToggleAction::NoOp()),
      intercepted_page_beforeunload_(false),
      ready_for_test_(false),
      opened_by_(opened_by) {
  // Set up delegate, so we get fully-functional window immediately.
  // It will not appear in UI though until |life_stage_ == kLoadCompleted|.
  main_web_contents_->SetDelegate(this);
  // Bindings take ownership over citizennotes as its delegate.
  bindings_->SetDelegate(this);
  // CitizenNotes uses PageZoom::Zoom(), so main_web_contents_ requires a
  // ZoomController.
  zoom::ZoomController::CreateForWebContents(main_web_contents_);
  zoom::ZoomController::FromWebContents(main_web_contents_)
      ->SetShowsNotificationBubble(false);

  g_citizennotes_window_instances.Get().push_back(this);

  // There is no inspected_web_contents in case of various workers.
  if (inspected_web_contents)
    inspected_web_contents_ = inspected_web_contents->GetWeakPtr();

  // Initialize docked page to be of the right size.
  if (can_dock_ && inspected_web_contents) {
    content::RenderWidgetHostView* inspected_view =
        inspected_web_contents->GetRenderWidgetHostView();
    if (inspected_view && main_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size = inspected_view->GetViewBounds().size();
      main_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
  }

  event_forwarder_ = std::make_unique<CitizenNotesEventForwarder>(this);

  // Tag the CitizenNotes main WebContents with its TaskManager specific UserData
  // so that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForCitizenNotesContents(main_web_contents_);

  std::vector<base::RepeatingCallback<void(CitizenNotesWindow*)>> copy(
      g_creation_callbacks.Get());
  for (const auto& callback : copy)
    callback.Run(this);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      language::prefs::kAcceptLanguages,
      base::BindRepeating(&CitizenNotesWindow::OnLocaleChanged,
                          base::Unretained(this)));
}

// static
bool CitizenNotesWindow::AllowCitizenNotesFor(Profile* profile,
                                      content::WebContents* web_contents) {
  // Don't allow CitizenNotes UI in kiosk mode, because the CitizenNotes UI would be
  // broken there. See https://crbug.com/514551 for context.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return false;

  return ChromeCitizenNotesManagerDelegate::AllowInspection(profile, web_contents);
}

// static
CitizenNotesWindow* CitizenNotesWindow::Create(
    Profile* profile,
    content::WebContents* inspected_web_contents,
    FrontendType frontend_type,
    const std::string& frontend_url,
    bool can_dock,
    const std::string& settings,
    const std::string& panel,
    bool has_other_clients,
    bool browser_connection,
    CitizenNotesOpenedByAction opened_by) {
  if (!AllowCitizenNotesFor(profile, inspected_web_contents))
    return nullptr;

  if (inspected_web_contents) {
    // Check for a place to dock.
    Browser* browser = nullptr;
    int tab;
    if (!FindInspectedBrowserAndTabIndex(inspected_web_contents, &browser,
                                         &tab) ||
        !browser->is_type_normal()) {
      can_dock = false;
    }
  }

  // Create WebContents with citizennotes.
  GURL url(GetCitizenNotesURL(profile, frontend_type, frontend_url, can_dock, panel,
                          has_other_clients, browser_connection));
  std::unique_ptr<WebContents> main_web_contents =
      WebContents::Create(WebContents::CreateParams(profile));
  main_web_contents->GetController().LoadURL(
      DecorateFrontendURL(url), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  CitizenNotesUIBindings* bindings =
      CitizenNotesUIBindings::ForWebContents(main_web_contents.get());

  if (!bindings)
    return nullptr;
  if (!settings.empty())
    SetPreferencesFromJson(profile, settings);
  return new CitizenNotesWindow(frontend_type, profile,
                            std::move(main_web_contents), bindings,
                            inspected_web_contents, can_dock, opened_by);
}

// static
GURL CitizenNotesWindow::GetCitizenNotesURL(Profile* profile,
                                    FrontendType frontend_type,
                                    const std::string& frontend_url,
                                    bool can_dock,
                                    const std::string& panel,
                                    bool has_other_clients,
                                    bool browser_connection) {
  std::string url;

  std::string remote_base =
      "?remoteBase=" + CitizenNotesUI::GetRemoteBaseURL().spec();

  const std::string valid_frontend =
      frontend_url.empty() ? chrome::kChromeUICitizenNotesURL : frontend_url;

  // remoteFrontend is here for backwards compatibility only.
  std::string remote_frontend =
      valid_frontend + ((valid_frontend.find("?") == std::string::npos)
                            ? "?remoteFrontend=true"
                            : "&remoteFrontend=true");
  switch (frontend_type) {
    case kFrontendDefault:
      url = kDefaultFrontendURL + remote_base;
      if (can_dock)
        url += "&can_dock=true";
      if (!panel.empty())
        url += "&panel=" + panel;
      if (base::FeatureList::IsEnabled(::features::kCitizenNotesTabTarget)) {
        url += "&targetType=tab";
      }
      if (base::FeatureList::IsEnabled(::features::kCitizenNotesVeLogging)) {
        url += "&veLogging=true";
      }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      if (base::FeatureList::IsEnabled(::features::kCitizenNotesConsoleInsights)) {
        url += "&enableAida=true&aidaApiKey=" +
               features::kCitizenNotesConsoleInsightsApiKey.Get();
      }
#endif
      break;
    case kFrontendWorker:
      url = kWorkerFrontendURL + remote_base;
      break;
    case kFrontendV8:
      url = kJSFrontendURL + remote_base;
      break;
    case kFrontendNode:
      url = kNodeFrontendURL + remote_base;
      break;
    case kFrontendRemote:
      url = remote_frontend;
      break;
    case kFrontendRemoteWorker:
      // isSharedWorker is here for backwards compatibility only.
      url = remote_frontend + "&isSharedWorker=true";
      break;
  }

  if (has_other_clients)
    url += "&hasOtherClients=true";
  if (browser_connection)
    url += "&browserConnection=true";

#if BUILDFLAG(CHROME_FOR_TESTING)
  url += "&isChromeForTesting=true";
#endif

  return CitizenNotesUIBindings::SanitizeFrontendURL(GURL(url));
}

// static
CitizenNotesWindow* CitizenNotesWindow::FindCitizenNotesWindow(
    CitizenNotesAgentHost* agent_host) {
  if (!agent_host || !g_citizennotes_window_instances.IsCreated())
    return nullptr;
  CitizenNotesWindows* instances = g_citizennotes_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->bindings_->IsAttachedTo(agent_host))
      return *it;
  }
  return nullptr;
}

// static
CitizenNotesWindow* CitizenNotesWindow::AsCitizenNotesWindow(
    content::WebContents* web_contents) {
  if (!web_contents || !g_citizennotes_window_instances.IsCreated())
    return nullptr;
  CitizenNotesWindows* instances = g_citizennotes_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->main_web_contents_ == web_contents)
      return *it;
  }
  return nullptr;
}

// static
CitizenNotesWindow* CitizenNotesWindow::AsCitizenNotesWindow(Browser* browser) {
  DCHECK(browser->is_type_citizennotes());
  if (browser->tab_strip_model()->empty())
    return nullptr;
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(0);
  return AsCitizenNotesWindow(contents);
}

WebContents* CitizenNotesWindow::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == main_web_contents_);
  if (!params.url.SchemeIs(content::kChromeCitizenNotesScheme)) {
    return OpenURLFromInspectedTab(params);
  }
  main_web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                             false);
  return main_web_contents_;
}

WebContents* CitizenNotesWindow::OpenURLFromInspectedTab(
    const content::OpenURLParams& params) {
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents)
    return nullptr;
  content::OpenURLParams modified = params;
  modified.referrer = content::Referrer();
  return inspected_web_contents->OpenURL(modified);
}

void CitizenNotesWindow::ActivateContents(WebContents* contents) {
  if (is_docked_) {
    WebContents* inspected_tab = GetInspectedWebContents();
    if (inspected_tab)
      inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else if (browser_) {
    browser_->window()->Activate();
  }
}

void CitizenNotesWindow::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (new_contents.get() == toolbox_web_contents_) {
    owned_toolbox_web_contents_ = std::move(new_contents);
    owned_toolbox_web_contents_->SetOwnerLocationForDebug(FROM_HERE);

    toolbox_web_contents_->SetDelegate(new CitizenNotesToolboxDelegate(
        toolbox_web_contents_, inspected_web_contents_));
    if (main_web_contents_->GetRenderWidgetHostView() &&
        toolbox_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size =
          main_web_contents_->GetRenderWidgetHostView()->GetViewBounds().size();
      toolbox_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
    UpdateBrowserWindow();
    return;
  }

  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, std::move(new_contents), target_url, disposition,
        window_features, user_gesture, was_blocked);
  }
}

void CitizenNotesWindow::WebContentsCreated(WebContents* source_contents,
                                        int opener_render_process_id,
                                        int opener_render_frame_id,
                                        const std::string& frame_name,
                                        const GURL& target_url,
                                        WebContents* new_contents) {
  if (target_url.SchemeIs(content::kChromeCitizenNotesScheme) &&
      target_url.path().rfind("device_mode_emulation_frame.html") !=
          std::string::npos) {
    CHECK(can_dock_);

    // Ownership will be passed in CitizenNotesWindow::AddNewContents.
    capture_handle_.RunAndReset();
    if (owned_toolbox_web_contents_)
      owned_toolbox_web_contents_.reset();
    toolbox_web_contents_ = new_contents;

    // Tag the CitizenNotes toolbox WebContents with its TaskManager specific
    // UserData so that it shows up in the task manager.
    task_manager::WebContentsTags::CreateForCitizenNotesContents(
        toolbox_web_contents_);

    // The toolbox holds a placeholder for the inspected WebContents. When the
    // placeholder is resized, a frame is requested. The inspected WebContents
    // is resized when the frame is rendered. Force rendering of the toolbox at
    // all times, to make sure that a frame can be rendered even when the
    // inspected WebContents fully covers the toolbox. https://crbug.com/828307
    capture_handle_ =
        toolbox_web_contents_->IncrementCapturerCount(gfx::Size(),
                                                      /*stay_hidden=*/false,
                                                      /*stay_awake=*/false);
  }
}

void CitizenNotesWindow::CloseContents(WebContents* source) {
  // We shouldn't get here as long as we're owned by the browser.
  CHECK(!browser_);
  life_stage_ = kClosing;
  UpdateBrowserWindow();
  // In case of docked main_web_contents_, we own it so delete here.
  // Embedding CitizenNotes window will be deleted as a result of
  // CitizenNotesUIBindings destruction.
  CHECK(owned_main_web_contents_);
  owned_main_web_contents_.reset();
}

void CitizenNotesWindow::ContentsZoomChange(bool zoom_in) {
  DCHECK(is_docked_);
  zoom::PageZoom::Zoom(main_web_contents_, zoom_in ? content::PAGE_ZOOM_IN
                                                   : content::PAGE_ZOOM_OUT);
}

void CitizenNotesWindow::BeforeUnloadFired(WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (!intercepted_page_beforeunload_) {
    // Docked citizennotes window closed directly.
    if (proceed)
      bindings_->Detach();
    *proceed_to_fire_unload = proceed;
  } else {
    // Inspected page is attempting to close.
    WebContents* inspected_web_contents = GetInspectedWebContents();
    if (proceed) {
      inspected_web_contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      bool should_proceed;
      inspected_web_contents->GetDelegate()->BeforeUnloadFired(
          inspected_web_contents, false, &should_proceed);
      DCHECK(!should_proceed);
    }
    *proceed_to_fire_unload = false;
  }
}

content::KeyboardEventProcessingResult CitizenNotesWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    return inspected_window->PreHandleKeyboardEvent(event);
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool CitizenNotesWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return true;
  }
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  return inspected_window && inspected_window->HandleKeyboardEvent(event);
}

content::JavaScriptDialogManager* CitizenNotesWindow::GetJavaScriptDialogManager(
    WebContents* source) {
  return javascript_dialogs::AppModalDialogManager::GetInstance();
}

std::unique_ptr<content::EyeDropper> CitizenNotesWindow::OpenEyeDropper(
    content::RenderFrameHost* render_frame_host,
    content::EyeDropperListener* listener) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->OpenEyeDropper(render_frame_host, listener);
  return nullptr;
}

void CitizenNotesWindow::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool CitizenNotesWindow::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void CitizenNotesWindow::ActivateWindow() {
  if (life_stage_ != kLoadCompleted)
    return;
  if (is_docked_ && GetInspectedBrowserWindow())
    main_web_contents_->Focus();
  else if (!is_docked_ && browser_ && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void CitizenNotesWindow::CloseWindow() {
  DCHECK(is_docked_);
  life_stage_ = kClosing;
  main_web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
}

void CitizenNotesWindow::Inspect(scoped_refptr<content::CitizenNotesAgentHost> host) {
  CitizenNotesWindow::OpenCitizenNotesWindow(host,
                                             profile_,
                                             CitizenNotesOpenedByAction::kUnknown);
}

void CitizenNotesWindow::SetInspectedPageBounds(const gfx::Rect& rect) {
  CitizenNotesContentsResizingStrategy strategy(rect);
  if (contents_resizing_strategy_.Equals(strategy))
    return;

  contents_resizing_strategy_.CopyFrom(strategy);
  UpdateBrowserWindow();
}

void CitizenNotesWindow::InspectElementCompleted() {
  if (!inspect_element_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("CitizeNotes.InspectElement",
        base::TimeTicks::Now() - inspect_element_start_time_);
    inspect_element_start_time_ = base::TimeTicks();
  }
}

void CitizenNotesWindow::SetIsDocked(bool dock_requested) {
  if (life_stage_ == kClosing)
    return;

  DCHECK(can_dock_ || !dock_requested);
  if (!can_dock_)
    dock_requested = false;

  bool was_docked = is_docked_;
  is_docked_ = dock_requested;

  if (life_stage_ != kLoadCompleted) {
    // This is a first time call we waited for to initialize.
    life_stage_ = life_stage_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    if (life_stage_ == kLoadCompleted)
      LoadCompleted();
    return;
  }

  if (dock_requested == was_docked)
    return;

  if (dock_requested && !was_docked && browser_) {
    // Detach window from the external citizennotes browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    DCHECK(!owned_main_web_contents_);

    // Removing the only WebContents from the tab strip of browser_ will
    // eventually lead to the destruction of browser_ as well, which is why it's
    // okay to just null the raw pointer here.
    browser_ = nullptr;

    // TODO(crbug.com/1221967): WebContents should be removed with a reason
    // other than kInsertedIntoOtherTabStrip, it's not getting reinserted into
    // another tab strip.
    owned_main_web_contents_ = std::make_unique<OwnedMainWebContents>(
        tab_strip_model->DetachWebContentsAtForInsertion(
            tab_strip_model->GetIndexOfWebContents(main_web_contents_)));
  } else if (!dock_requested && was_docked) {
    UpdateBrowserWindow();
  }

  Show(CitizenNotesToggleAction::Show());
}

void CitizenNotesWindow::OpenInNewTab(const std::string& url) {
  GURL fixed_url(url);
  WebContents* inspected_web_contents = GetInspectedWebContents();
  int child_id = content::ChildProcessHost::kInvalidUniqueID;
  if (inspected_web_contents) {
    content::RenderViewHost* render_view_host =
        inspected_web_contents->GetPrimaryMainFrame()->GetRenderViewHost();
    if (render_view_host)
      child_id = render_view_host->GetProcess()->GetID();
  }
  // Use about:blank instead of an empty GURL. The browser treats an empty GURL
  // as navigating to the home page, which may be privileged (chrome://newtab/).
  if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          child_id, fixed_url))
    fixed_url = GURL(url::kAboutBlankURL);

  content::OpenURLParams params(fixed_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  if (!inspected_web_contents || !inspected_web_contents->OpenURL(params)) {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    chrome::AddSelectedTabWithURL(displayer.browser(), fixed_url,
                                  ui::PAGE_TRANSITION_LINK);
  }
}

void CitizenNotesWindow::SetWhitelistedShortcuts(
    const std::string& message) {
  event_forwarder_->SetWhitelistedShortcuts(message);
}

void CitizenNotesWindow::SetEyeDropperActive(bool active) {
  WebContents* web_contents = GetInspectedWebContents();
  if (!web_contents)
    return;
  if (active) {
    eye_dropper_ = std::make_unique<CitizenNotesEyeDropper>(
        web_contents,
        base::BindRepeating(&CitizenNotesWindow::ColorPickedInEyeDropper,
                            base::Unretained(this)));
  } else {
    eye_dropper_.reset();
  }
}

void CitizenNotesWindow::ColorPickedInEyeDropper(int r, int g, int b, int a) {
  base::Value::Dict color;
  color.Set("r", r);
  color.Set("g", g);
  color.Set("b", b);
  color.Set("a", a);
  bindings_->CallClientMethod("CitizenNotesAPI", "eyeDropperPickedColor",
                              base::Value(std::move(color)));
}

void CitizenNotesWindow::InspectedContentsClosing() {
  if (!close_on_detach_)
    return;
  intercepted_page_beforeunload_ = false;
  life_stage_ = kClosing;
  main_web_contents_->ClosePage();
}

infobars::ContentInfoBarManager* CitizenNotesWindow::GetInfoBarManager() {
  return is_docked_ ? infobars::ContentInfoBarManager::FromWebContents(
                          GetInspectedWebContents())
                    : infobars::ContentInfoBarManager::FromWebContents(
                          main_web_contents_);
}

void CitizenNotesWindow::RenderProcessGone(bool crashed) {
  // Docked CitizenNotesWindow owns its main_web_contents_ and must delete it.
  // Undocked main_web_contents_ are owned and handled by browser.
  // see crbug.com/369932
  if (is_docked_) {
    CloseContents(main_web_contents_);
  } else if (browser_ && crashed) {
    browser_->window()->Close();
  }
}

void CitizenNotesWindow::ShowCertificateViewer(const std::string& cert_chain) {
  absl::optional<base::Value> value = base::JSONReader::Read(cert_chain);
  CHECK(value && value->is_list());
  std::vector<std::string> decoded;
  for (const auto& item : value->GetList()) {
    CHECK(item.is_string());
    std::string temp;
    CHECK(base::Base64Decode(item.GetString(), &temp));
    decoded.push_back(std::move(temp));
  }

  std::vector<base::StringPiece> cert_string_piece;
  for (const auto& str : decoded)
    cert_string_piece.push_back(str);
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromDERCertChain(cert_string_piece);
  CHECK(cert);

  WebContents* inspected_contents =
      is_docked_ ? GetInspectedWebContents() : main_web_contents_;
  Browser* browser = nullptr;
  int tab = 0;
  if (!FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return;
  gfx::NativeWindow parent = browser->window()->GetNativeWindow();
  ::ShowCertificateViewer(inspected_contents, parent, cert.get());
}

void CitizenNotesWindow::OnLoadCompleted() {
  // First seed inspected tab id for extension APIs.
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      bindings_->CallClientMethod(
          "CitizenNotesAPI", "setInspectedTabId",
          base::Value(session_tab_helper->session_id().id()));
    }
  }

  if (life_stage_ == kClosing)
    return;

  // We could be in kLoadCompleted state already if frontend reloads itself.
  if (life_stage_ != kLoadCompleted) {
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Here we set kOnLoadFired.
    life_stage_ = life_stage_ == kIsDockedSet ? kLoadCompleted : kOnLoadFired;
  }
  if (life_stage_ == kLoadCompleted)
    LoadCompleted();
}

void CitizenNotesWindow::ReadyForTest() {
  ready_for_test_ = true;
  if (!ready_for_test_callback_.is_null())
    std::move(ready_for_test_callback_).Run();
}

void CitizenNotesWindow::ConnectionReady() {
  if (throttle_)
    throttle_->ResumeThrottle();
}

void CitizenNotesWindow::SetOpenNewWindowForPopups(bool value) {
  open_new_window_for_popups_ = value;
}

void CitizenNotesWindow::CreateCitizenNotesBrowser() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetDict(prefs::kAppWindowPlacement).Find(kCitizenNotesApp)) {
    // Ensure there is always a default size so that
    // BrowserFrame::InitBrowserFrame can retrieve it later.
    ScopedDictPrefUpdate update(prefs, prefs::kAppWindowPlacement);
    base::Value::Dict& wp_prefs = update.Get();
    base::Value::Dict dev_tools_defaults;
    dev_tools_defaults.Set("left", 100);
    dev_tools_defaults.Set("top", 100);
    dev_tools_defaults.Set("right", 740);
    dev_tools_defaults.Set("bottom", 740);
    dev_tools_defaults.Set("maximized", false);
    dev_tools_defaults.Set("always_on_top", false);
    wp_prefs.Set(kCitizenNotesApp, std::move(dev_tools_defaults));
  }

  if (Browser::GetCreationStatusForProfile(profile_) !=
      Browser::CreationStatus::kOk) {
    return;
  }
  browser_ =
      Browser::Create(Browser::CreateParams::CreateForCitizenNotes(profile_));
  browser_->tab_strip_model()->AddWebContents(
      OwnedMainWebContents::TakeWebContents(
          std::move(owned_main_web_contents_)),
      -1, ui::PAGE_TRANSITION_AUTO_TOPLEVEL, AddTabTypes::ADD_ACTIVE);
  OverrideAndSyncCitizenNotesRendererPrefs();
}

BrowserWindow* CitizenNotesWindow::GetInspectedBrowserWindow() {
  Browser* browser = nullptr;
  int tab;
  return FindInspectedBrowserAndTabIndex(GetInspectedWebContents(), &browser,
                                         &tab)
             ? browser->window()
             : nullptr;
}

void CitizenNotesWindow::DoAction(const CitizenNotesToggleAction& action) {
  switch (action.type()) {
    case CitizenNotesToggleAction::kInspect:
      bindings_->CallClientMethod("CitizenNotesAPI", "enterInspectElementMode");
      break;

    case CitizenNotesToggleAction::kShowElementsPanel:
    case CitizenNotesToggleAction::kPauseInDebugger:
    case CitizenNotesToggleAction::kShowConsolePanel:
    case CitizenNotesToggleAction::kShow:
    case CitizenNotesToggleAction::kToggle:
      // Do nothing.
      break;

    case CitizenNotesToggleAction::kReveal: {
      const CitizenNotesToggleAction::RevealParams* params =
          action.params();
      CHECK(params);
      bindings_->CallClientMethod(
          "CitizenNotesAPI", "revealSourceLine", base::Value(params->url),
          base::Value(static_cast<int>(params->line_number)),
          base::Value(static_cast<int>(params->column_number)));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void CitizenNotesWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(nullptr);
}

void CitizenNotesWindow::UpdateBrowserWindow() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateCitizenNotes();
}

WebContents* CitizenNotesWindow::GetInspectedWebContents() {
  return inspected_web_contents_.get();
}

void CitizenNotesWindow::LoadCompleted() {
  Show(action_on_load_);
  action_on_load_ = CitizenNotesToggleAction::NoOp();
  if (!load_completed_callback_.is_null()) {
    std::move(load_completed_callback_).Run();
  }
}

void CitizenNotesWindow::SetLoadCompletedCallback(base::OnceClosure closure) {
  if (life_stage_ == kLoadCompleted || life_stage_ == kClosing) {
    if (!closure.is_null())
      std::move(closure).Run();
    return;
  }
  load_completed_callback_ = std::move(closure);
}

bool CitizenNotesWindow::ForwardKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return event_forwarder_->ForwardEvent(event);
}

void CitizenNotesWindow::RegisterModalDialogManager(Browser* browser) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      main_web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(main_web_contents_)
      ->SetDelegate(browser);
}

void CitizenNotesWindow::OnReattachMainTargetComplete(base::Value) {
  std::move(reattach_complete_callback_).Run();
}

void CitizenNotesWindow::OnLocaleChanged() {
  OverrideAndSyncCitizenNotesRendererPrefs();
}

void CitizenNotesWindow::OverrideAndSyncCitizenNotesRendererPrefs() {
  main_web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;
  main_web_contents_->GetMutableRendererPrefs()->accept_languages =
      g_browser_process->GetApplicationLocale();
  main_web_contents_->SyncRendererPrefs();
}
