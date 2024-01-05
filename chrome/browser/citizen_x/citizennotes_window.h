// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENOTES_CITIZENOTES_WINDOW_H_
#define CHROME_BROWSER_CITIZENOTES_CITIZENOTES_WINDOW_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/citizen_x/citizennotes_contents_resizing_strategy.h"
#include "chrome/browser/citizen_x/citizennotes_toggle_action.h"
#include "chrome/browser/citizen_x/citizennotes_ui_bindings.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;
class BrowserWindow;
class CitizenNotesEventForwarder;
class CitizenNotesEyeDropper;

namespace content {
class CitizenNotesAgentHost;
struct NativeWebKeyboardEvent;
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Values that represent different actions to open DevTools window.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class CitizenNotesOpenedByAction {
  kUnknown = 0,
  // Main menu -> More Tools -> Developer Tools
  // or Ctrl+Shift+I shortcut
  kMainMenuOrMainShortcut = 1,
  // Ctrl+Shift+J shortcut to jump to Console
  kConsoleShortcut = 2,
  // Context menu -> Inspect
  kContextMenuInspect = 3,
  // Ctrl+Shift+C shortcut to turn on inspect mode
  kInspectorModeShortcut = 4,
  // Toggle-open via F12
  kToggleShortcut = 5,
  // Link on chrome://inspect
  kInspectLink = 6,
  // Via --auto-open-devtools-for-tabs or "Auto-open DevTools for popups"
  kAutomaticForNewTarget = 7,
  // Re-open when some targets (like apps) reload
  kTargetReload = 8,
  // Open Node DevTools button in a regular app
  kOpenForNodeFromAnotherTarget = 9,
  // Add values above this line with a corresponding label in
  // tools/metrics/histograms/enums.xml
  kMaxValue = kOpenForNodeFromAnotherTarget,
};

class CitizenNotesWindow : public CitizenNotesUIBindings::Delegate,
                       public content::WebContentsDelegate {
 public:
  static const char kCitizenNotesApp[];

  CitizenNotesWindow(const CitizenNotesWindow&) = delete;
  CitizenNotesWindow& operator=(const CitizenNotesWindow&) = delete;

  ~CitizenNotesWindow() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns whether CitizenNotes are allowed for the specified
  // |profile| and |web_contents|. If |web_contents| is null,
  // only checks for |profile| in general.
  static bool AllowCitizenNotesFor(Profile* profile,
                               content::WebContents* web_contents);

  // Return the CitizenNotesWindow for the given WebContents if one exists,
  // otherwise nullptr.
  static CitizenNotesWindow* GetInstanceForInspectedWebContents(
      content::WebContents* inspected_web_contents);

  // Return the docked CitizenNotes WebContents for the given inspected WebContents
  // if one exists and should be shown in browser window, otherwise nullptr.
  // This method will return only fully initialized window ready to be
  // presented in UI.
  // If |out_strategy| is not nullptr, it will contain resizing strategy.
  // For immediately-ready-to-use but maybe not yet fully initialized CitizenNotes
  // use |GetInstanceForInspectedRenderViewHost| instead.
  static content::WebContents* GetInTabWebContents(
      content::WebContents* inspected_tab,
      CitizenNotesContentsResizingStrategy* out_strategy);

  static bool IsCitizenNotesWindow(content::WebContents* web_contents);
  static CitizenNotesWindow* AsCitizenNotesWindow(content::WebContents* web_contents);
  static CitizenNotesWindow* AsCitizenNotesWindow(Browser* browser);
  static CitizenNotesWindow* FindCitizenNotesWindow(content::CitizenNotesAgentHost*);

  // Open or reveal CitizenNotes window, and perform the specified action.
  // How to get pointer to the created window see comments for
  // ToggleCitizenNotesWindow().
  static void OpenCitizenNotesWindow(content::WebContents* inspected_web_contents,
                                 const CitizenNotesToggleAction& action,
                                 CitizenNotesOpenedByAction opened_by);

  // Open or reveal CitizenNotes window, with no special action.
  // How to get pointer to the created window see comments for
  // ToggleCitizenNotesWindow().
  static void OpenCitizenNotesWindow(content::WebContents* inspected_web_contents,
                                 CitizenNotesOpenedByAction opened_by);
  static void OpenCitizenNotesWindow(content::WebContents* inspected_web_contents,
                                 Profile* profile,
                                 CitizenNotesOpenedByAction opened_by);

  // Open or reveal CitizenNotes window, with no special action. Use |profile| to
  // open client window in, default to |host|'s profile if none given.
  static void OpenCitizenNotesWindow(scoped_refptr<content::CitizenNotesAgentHost> host,
                                 Profile* profile,
                                 CitizenNotesOpenedByAction opened_by);
  // Similar to previous one, but forces the bundled frontend to be used.
  static void OpenCitizenNotesWindowWithBundledFrontend(
      scoped_refptr<content::CitizenNotesAgentHost> host,
      Profile* profile,
      CitizenNotesOpenedByAction opened_by);

  // Perform specified action for current WebContents inside a |browser|.
  // This may close currently open CitizenNotes window.
  // If DeveloperToolsAvailability policy disallows developer tools for the
  // current WebContents, no CitizenNotes window created. In case if needed pointer
  // to the created window one should use CitizenNotesAgentHost and
  // CitizenNotesWindow::FindCitizenNotesWindow(). E.g.:
  //
  // scoped_refptr<content::CitizenNotesAgentHost> agent(
  //   content::CitizenNotesAgentHost::GetOrCreateFor(inspected_web_contents));
  // CitizenNotesWindow::ToggleCitizenNotesWindow(
  //   inspected_web_contents, CitizenNotesToggleAction::Show());
  // CitizenNotesWindow* window = CitizenNotesWindow::FindCitizenNotesWindow(agent.get());
  //
  static void ToggleCitizenNotesWindow(
      Browser* browser,
      const CitizenNotesToggleAction& action,
      CitizenNotesOpenedByAction opened_by = CitizenNotesOpenedByAction::kUnknown);

  // Node frontend is always undocked.
  static CitizenNotesWindow* OpenNodeFrontendWindow(
      Profile* profile,
      CitizenNotesOpenedByAction opened_by);

  static void InspectElement(content::RenderFrameHost* inspected_frame_host,
                             int x,
                             int y);

  static void LogCitizenNotesOpenedByAction(CitizenNotesOpenedByAction opened_by);

  // Logs UKM event when CitizenNotes is opened.
  static void LogCitizenNotesOpenedUKM(content::WebContents* web_contents);

  static std::unique_ptr<content::NavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* handle);

  // Updates the WebContents inspected by the CitizenNotesWindow by reattaching
  // the binding to |new_web_contents|. Called when swapping an outer
  // WebContents with its inner WebContents.
  void UpdateInspectedWebContents(content::WebContents* new_web_contents,
                                  base::OnceCallback<void()> callback);

  // Sets closure to be called after load is done. If already loaded, calls
  // closure immediately.
  void SetLoadCompletedCallback(base::OnceClosure closure);

  // Forwards an unhandled keyboard event to the CitizenNotes frontend.
  bool ForwardKeyboardEvent(const content::NativeWebKeyboardEvent& event);

  // Reloads inspected web contents as if it was triggered from CitizenNotes.
  // Returns true if it has successfully handled reload, false if the caller
  // is to proceed reload without CitizenNotes interception.
  bool ReloadInspectedWebContents(bool bypass_cache);

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

  content::WebContents* OpenURLFromInspectedTab(
      const content::OpenURLParams& params);

  // BeforeUnload interception ////////////////////////////////////////////////

  // In order to preserve any edits the user may have made in citizennotes, the
  // beforeunload event of the inspected page is hooked - citizennotes gets the
  // first shot at handling beforeunload and presents a dialog to the user. If
  // the user accepts the dialog then the script is given a chance to handle
  // it. This way 2 dialogs may be displayed: one from the citizennotes asking the
  // user to confirm that they're ok with their citizennotes edits going away and
  // another from the webpage as the result of its beforeunload handler.
  // The following set of methods handle beforeunload event flow through
  // citizennotes window. When the |contents| with citizennotes opened on them are
  // getting closed, the following sequence of calls takes place:
  // 1. |CitizenNotesWindow::InterceptPageBeforeUnload| is called and indicates
  //    whether citizennotes intercept the beforeunload event.
  //    If InterceptPageBeforeUnload() returns true then the following steps
  //    will take place; otherwise only step 4 will be reached and none of the
  //    corresponding functions in steps 2 & 3 will get called.
  // 2. |CitizenNotesWindow::InterceptPageBeforeUnload| fires beforeunload event
  //    for citizennotes frontend, which will asynchronously call
  //    |WebContentsDelegate::BeforeUnloadFired| method.
  //    In case of docked citizennotes window, citizennotes are set as a delegate for
  //    its frontend, so method |CitizenNotesWindow::BeforeUnloadFired| will be
  //    called directly.
  //    If citizennotes window is undocked it's not set as the delegate so the call
  //    to BeforeUnloadFired is proxied through HandleBeforeUnload() rather
  //    than getting called directly.
  // 3a. If |CitizenNotesWindow::BeforeUnloadFired| is called with |proceed|=false
  //     it calls through to the content's BeforeUnloadFired(), which from the
  //     WebContents perspective looks the same as the |content|'s own
  //     beforeunload dialog having had it's 'stay on this page' button clicked.
  // 3b. If |proceed| = true, then it fires beforeunload event on |contents|
  //     and everything proceeds as it normally would without the citizennotes
  //     interception.
  // 4. If the user cancels the dialog put up by either the WebContents or
  //    citizennotes frontend, then |contents|'s |BeforeUnloadFired| callback is
  //    called with the proceed argument set to false, this causes
  //    |CitizenNotesWindow::OnPageCloseCancelled| to be called.

  // citizennotes window in undocked state is not set as a delegate of
  // its frontend. Instead, an instance of browser is set as the delegate, and
  // thus beforeunload event callback from citizennotes frontend is not delivered
  // to the instance of citizennotes window, which is solely responsible for
  // managing custom beforeunload event flow.
  // This is a helper method to route callback from
  // |Browser::BeforeUnloadFired| back to |CitizenNotesWindow::BeforeUnloadFired|.
  // * |proceed| - true if the user clicked 'ok' in the beforeunload dialog,
  //   false otherwise.
  // * |proceed_to_fire_unload| - output parameter, whether we should continue
  //   to fire the unload event or stop things here.
  // Returns true if citizennotes window is in a state of intercepting beforeunload
  // event and if it will manage unload process on its own.
  static bool HandleBeforeUnload(content::WebContents* contents,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);

  // Returns true if this contents beforeunload event was intercepted by
  // citizennotes and false otherwise. If the event was intercepted, caller should
  // not fire beforeunload event on |contents| itself as citizennotes window will
  // take care of it, otherwise caller should continue handling the event as
  // usual.
  static bool InterceptPageBeforeUnload(content::WebContents* contents);

  // Returns true if citizennotes browser has already fired its beforeunload event
  // as a result of beforeunload event interception.
  static bool HasFiredBeforeUnloadEventForCitizenNotesBrowser(Browser* browser);

  // Returns true if citizennotes window would like to hook beforeunload event
  // of this |contents|.
  static bool NeedsToInterceptBeforeUnload(content::WebContents* contents);

  // Notify citizennotes window that closing of |contents| was cancelled
  // by user.
  static void OnPageCloseCanceled(content::WebContents* contents);

  content::WebContents* GetInspectedWebContents();
  BrowserWindow* GetInspectedBrowserWindow();

 private:
  friend class CitizenNotesWindowTesting;
  friend class CitizenNotesWindowCreationObserver;
  friend class HatsNextWebDialogBrowserTest;

  using CreationCallback = base::RepeatingCallback<void(CitizenNotesWindow*)>;
  static void AddCreationCallbackForTest(const CreationCallback& callback);
  static void RemoveCreationCallbackForTest(const CreationCallback& callback);

  static void OpenCitizenNotesWindowForFrame(
      Profile* profile,
      const scoped_refptr<content::CitizenNotesAgentHost>& agent_host,
      CitizenNotesOpenedByAction opened_by);
  static void OpenCitizenNotesWindowForWorker(
      Profile* profile,
      const scoped_refptr<content::CitizenNotesAgentHost>& worker_agent,
      CitizenNotesOpenedByAction opened_by);

                           
  // citizennotes lifecycle typically follows this way:
  // - Toggle/Open: client call;
  // - Create;
  // - ScheduleShow: setup window to be functional, but not yet show;
  // - DocumentOnLoadCompletedInPrimaryMainFrame: frontend loaded;
  // - SetIsDocked: frontend decided on docking state;
  // - OnLoadCompleted: ready to present frontend;
  // - Show: actually placing frontend WebContents to a Browser or docked place;
  // - DoAction: perform action passed in Toggle/Open;
  // - ...;
  // - CloseWindow: initiates before unload handling;
  // - CloseContents: destroys frontend;
  // - CitizenNotesWindow is dead once it's main_web_contents dies.
  enum LifeStage {
    kNotLoaded,
    kOnLoadFired, // Implies SetIsDocked was not yet called.
    kIsDockedSet, // Implies DocumentOnLoadCompleted was not yet called.
    kLoadCompleted,
    kClosing
  };

  enum FrontendType {
    kFrontendDefault,
    kFrontendWorker,
    kFrontendV8,
    kFrontendNode,
    kFrontendRemote,
    kFrontendRemoteWorker,
  };

  CitizenNotesWindow(FrontendType frontend_type,
                 Profile* profile,
                 std::unique_ptr<content::WebContents> main_web_contents,
                 CitizenNotesUIBindings* bindings,
                 content::WebContents* inspected_web_contents,
                 bool can_dock,
                 CitizenNotesOpenedByAction opened_by);

  // External frontend is always undocked.
  static void OpenExternalFrontend(
      Profile* profile,
      const std::string& frontend_uri,
      const scoped_refptr<content::CitizenNotesAgentHost>& agent_host,
      bool use_bundled_frontend,
      CitizenNotesOpenedByAction opened_by);

  static void OpenCitizenNotesWindow(scoped_refptr<content::CitizenNotesAgentHost> host,
                                 Profile* profile,
                                 bool use_bundled_frontend,
                                 CitizenNotesOpenedByAction opened_by);

  static CitizenNotesWindow* Create(Profile* profile,
                                content::WebContents* inspected_web_contents,
                                FrontendType frontend_type,
                                const std::string& frontend_url,
                                bool can_dock,
                                const std::string& settings,
                                const std::string& panel,
                                bool has_other_clients,
                                bool browser_connection,
                                CitizenNotesOpenedByAction opened_by);
  static GURL GetCitizenNotesURL(Profile* profile,
                             FrontendType frontend_type,
                             const std::string& frontend_url,
                             bool can_dock,
                             const std::string& panel,
                             bool has_other_clients,
                             bool browser_connection);

  static void ToggleCitizenNotesWindow(
      content::WebContents* web_contents,
      Profile* profile,
      bool force_open,
      const CitizenNotesToggleAction& action,
      const std::string& settings,
      CitizenNotesOpenedByAction opened_by = CitizenNotesOpenedByAction::kUnknown);
  static Profile* GetProfileForCitizenNotesWindow(
      content::WebContents* web_contents);

  // content::WebContentsDelegate:
  void ActivateContents(content::WebContents* contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void CloseContents(content::WebContents* source) override;
  void ContentsZoomChange(bool zoom_in) override;
  void BeforeUnloadFired(content::WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* render_frame_host,
      content::EyeDropperListener* listener) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

  // content::CitizenNotesUIBindings::Delegate overrides
  void ActivateWindow() override;
  void CloseWindow() override;
  void Inspect(scoped_refptr<content::CitizenNotesAgentHost> host) override;
  void SetInspectedPageBounds(const gfx::Rect& rect) override;
  void InspectElementCompleted() override;
  void SetIsDocked(bool is_docked) override;
  void OpenInNewTab(const std::string& url) override;
  void SetWhitelistedShortcuts(const std::string& message) override;
  void SetEyeDropperActive(bool active) override;
  void OpenNodeFrontend() override;
  void InspectedContentsClosing() override;
  void OnLoadCompleted() override;
  void ReadyForTest() override;
  void ConnectionReady() override;
  void SetOpenNewWindowForPopups(bool value) override;
  infobars::ContentInfoBarManager* GetInfoBarManager() override;
  void RenderProcessGone(bool crashed) override;
  void ShowCertificateViewer(const std::string& cert_viewer) override;

  void ColorPickedInEyeDropper(int r, int g, int b, int a);

  // This method creates a new Browser object (if possible), and passes
  // ownership of owned_main_web_contents_ to the tab strip of the Browser.
  void CreateCitizenNotesBrowser();
  void ScheduleShow(const CitizenNotesToggleAction& action);
  void Show(const CitizenNotesToggleAction& action);
  void DoAction(const CitizenNotesToggleAction& action);
  void LoadCompleted();
  void UpdateBrowserToolbar();
  void UpdateBrowserWindow();

  // Registers a WebContentsModalDialogManager for our WebContents in order to
  // display web modal dialogs triggered by it.
  void RegisterModalDialogManager(Browser* browser);

  void OnReattachMainTargetComplete(base::Value);

  // Called when the accepted language changes. |navigator.language| of the
  // CitizenNotes window should match the application language. When the user
  // changes the accepted language then this listener flips the language back
  // to the application language for the CitizenNotes renderer process.
  // Please note that |navigator.language| will have the wrong language for
  // a very short period of time (until this handler has reset it again).
  void OnLocaleChanged();
  void OverrideAndSyncCitizenNotesRendererPrefs();

  base::WeakPtr<content::WebContents> inspected_web_contents_;

  FrontendType frontend_type_;
  RAW_PTR_EXCLUSION Profile* profile_;
  RAW_PTR_EXCLUSION content::WebContents* main_web_contents_;

  // CitizenNotesWindow is informed of the creation of the |toolbox_web_contents_|
  // in WebContentsCreated right before ownership is passed to to CitizenNotesWindow
  // in AddNewContents(). The former call has information not available in the
  // latter, so it's easiest to record a raw pointer first in
  // |toolbox_web_contents_|, and then update ownership immediately afterwards.
  // TODO(erikchen): If we updated AddNewContents() to also pass back the
  // target url, then we wouldn't need to listen to WebContentsCreated at all.
  RAW_PTR_EXCLUSION content::WebContents* toolbox_web_contents_;
  std::unique_ptr<content::WebContents> owned_toolbox_web_contents_;

  RAW_PTR_EXCLUSION CitizenNotesUIBindings* bindings_;
  RAW_PTR_EXCLUSION Browser* browser_;

  // When CitizenNotesWindow is docked, it owns main_web_contents_. When it isn't
  // docked, the tab strip model owns the main_web_contents_.
  bool is_docked_;
  class OwnedMainWebContents;
  std::unique_ptr<OwnedMainWebContents> owned_main_web_contents_;

  const bool can_dock_;
  bool close_on_detach_;
  LifeStage life_stage_;
  CitizenNotesToggleAction action_on_load_;
  CitizenNotesContentsResizingStrategy contents_resizing_strategy_;
  // True if we're in the process of handling a beforeunload event originating
  // from the inspected webcontents, see InterceptPageBeforeUnload for details.
  bool intercepted_page_beforeunload_;
  base::OnceClosure load_completed_callback_;
  base::OnceClosure close_callback_;
  bool ready_for_test_;
  base::OnceClosure ready_for_test_callback_;

  base::TimeTicks inspect_element_start_time_;
  std::unique_ptr<CitizenNotesEventForwarder> event_forwarder_;
  std::unique_ptr<CitizenNotesEyeDropper> eye_dropper_;

  const CitizenNotesOpenedByAction opened_by_;

  class Throttle;
  RAW_PTR_EXCLUSION Throttle* throttle_ = nullptr;
  bool open_new_window_for_popups_ = false;

  base::OnceCallback<void()> reattach_complete_callback_;

  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedClosureRunner capture_handle_;

  friend class CitizenNotesEventForwarder;
};

#endif  // CHROME_BROWSER_CITIZENOTES_CITIZENOTES_WINDOW_H_
