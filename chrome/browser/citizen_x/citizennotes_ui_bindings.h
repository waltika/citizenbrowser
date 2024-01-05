
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_UI_BINDINGS_H_
#define CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_UI_BINDINGS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/devtools/aida_client.h"
#include "chrome/browser/citizen_x/device/citizennotes_android_bridge.h"
#include "chrome/browser/citizen_x/citizennotes_embedder_message_dispatcher.h"
#include "chrome/browser/devtools/devtools_file_helper.h"
#include "chrome/browser/devtools/devtools_file_system_indexer.h"
#include "chrome/browser/citizen_x/citizennotes_infobar_delegate.h"
#include "chrome/browser/citizen_x/citizennotes_settings.h"
#include "chrome/browser/citizen_x/citizennotes_targets_ui.h"
#include "chrome/browser/devtools/visual_logging.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/citizennotes_frontend_host.h"
#include "ui/gfx/geometry/size.h"

class CitizenNotesAndroidBridge;
class CNPortForwardingStatusSerializer;
class Profile;

namespace content {
class CitizenNotesFrontendHost;
class NavigationHandle;
class WebContents;
}

namespace infobars {
class ContentInfoBarManager;
}

// Base implementation of DevTools bindings around front-end.
class CitizenNotesUIBindings : public CitizenNotesEmbedderMessageDispatcher::Delegate,
                           public CitizenNotesAndroidBridge::DeviceCountListener,
                           public content::CitizenNotesAgentHostClient,
                           public DevToolsFileHelper::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void ActivateWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual void Inspect(scoped_refptr<content::CitizenNotesAgentHost> host) = 0;
    virtual void SetInspectedPageBounds(const gfx::Rect& rect) = 0;
    virtual void InspectElementCompleted() = 0;
    virtual void SetIsDocked(bool is_docked) = 0;
    virtual void OpenInNewTab(const std::string& url) = 0;
    virtual void SetWhitelistedShortcuts(const std::string& message) = 0;
    virtual void SetEyeDropperActive(bool active) = 0;
    virtual void OpenNodeFrontend() = 0;

    virtual void InspectedContentsClosing() = 0;
    virtual void OnLoadCompleted() = 0;
    virtual void ReadyForTest() = 0;
    virtual void ConnectionReady() = 0;
    virtual void SetOpenNewWindowForPopups(bool value) = 0;
    virtual infobars::ContentInfoBarManager* GetInfoBarManager() = 0;
    virtual void RenderProcessGone(bool crashed) = 0;
    virtual void ShowCertificateViewer(const std::string& cert_chain) = 0;
  };

  static CitizenNotesUIBindings* ForWebContents(content::WebContents* web_contents);

  static GURL SanitizeFrontendURL(const GURL& url);
  static bool IsValidFrontendURL(const GURL& url);
  static bool IsValidRemoteFrontendURL(const GURL& url);

  explicit CitizenNotesUIBindings(content::WebContents* web_contents);

  CitizenNotesUIBindings(const CitizenNotesUIBindings&) = delete;
  CitizenNotesUIBindings& operator=(const CitizenNotesUIBindings&) = delete;

  ~CitizenNotesUIBindings() override;

  std::string GetTypeForMetrics() override;

  content::WebContents* web_contents() { return web_contents_; }
  Profile* profile() { return profile_; }
  content::CitizenNotesAgentHost* agent_host() { return agent_host_.get(); }

  // Takes ownership over the |delegate|.
  void SetDelegate(Delegate* delegate);
  void CallClientMethod(
      const std::string& object_name,
      const std::string& method_name,
      base::Value arg1 = {},
      base::Value arg2 = {},
      base::Value arg3 = {},
      base::OnceCallback<void(base::Value)> completion_callback =
          base::OnceCallback<void(base::Value)>());
  void AttachTo(const scoped_refptr<content::CitizenNotesAgentHost>& agent_host);
  void AttachViaBrowserTarget(
      const scoped_refptr<content::CitizenNotesAgentHost>& agent_host);
  void Detach();
  bool IsAttachedTo(content::CitizenNotesAgentHost* agent_host);

  static base::Value::Dict GetSyncInformationForProfile(Profile* profile);

 private:
  using CitizenNotesUIBindingsList = std::vector<CitizenNotesUIBindings*>;

  void HandleMessageFromCitizenNotesFrontend(base::Value::Dict message);

  // content::CitizenNotesAgentHostClient implementation.
  void DispatchProtocolMessage(content::CitizenNotesAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(content::CitizenNotesAgentHost* agent_host) override;
  bool MayWriteLocalFiles() override;

  // CitizenNotesEmbedderMessageDispatcher::Delegate implementation.
  void ActivateWindow() override;
  void CloseWindow() override;
  void LoadCompleted() override;
  void SetInspectedPageBounds(const gfx::Rect& rect) override;
  void InspectElementCompleted() override;
  void InspectedURLChanged(const std::string& url) override;
  void LoadNetworkResource(DispatchCallback callback,
                           const std::string& url,
                           const std::string& headers,
                           int stream_id) override;
  void SetIsDocked(DispatchCallback callback, bool is_docked) override;
  void OpenInNewTab(const std::string& url) override;
  void ShowItemInFolder(const std::string& file_system_path) override;
  void SaveToFile(const std::string& url,
                  const std::string& content,
                  bool save_as) override;
  void AppendToFile(const std::string& url,
                    const std::string& content) override;
  void RequestFileSystems() override;
  void AddFileSystem(const std::string& type) override;
  void RemoveFileSystem(const std::string& file_system_path) override;
  void UpgradeDraggedFileSystemPermissions(
      const std::string& file_system_url) override;
  void IndexPath(int index_request_id,
                 const std::string& file_system_path,
                 const std::string& excluded_folders) override;
  void StopIndexing(int index_request_id) override;
  void SearchInPath(int search_request_id,
                    const std::string& file_system_path,
                    const std::string& query) override;
  void SetWhitelistedShortcuts(const std::string& message) override;
  void SetEyeDropperActive(bool active) override;
  void ShowCertificateViewer(const std::string& cert_chain) override;
  void ZoomIn() override;
  void ZoomOut() override;
  void ResetZoom() override;
  void SetDevicesDiscoveryConfig(
      bool discover_usb_devices,
      bool port_forwarding_enabled,
      const std::string& port_forwarding_config,
      bool network_discovery_enabled,
      const std::string& network_discovery_config) override;
  void SetDevicesUpdatesEnabled(bool enabled) override;
  void PerformActionOnRemotePage(const std::string& page_id,
                                 const std::string& action) override;
  void OpenRemotePage(const std::string& browser_id,
                      const std::string& url) override;
  void OpenNodeFrontend() override;
  void DispatchProtocolMessageFromCitizenNotesFrontend(
      const std::string& message) override;
  void RecordCountHistogram(const std::string& name,
                            int sample,
                            int min,
                            int exclusive_max,
                            int buckets) override;
  void RecordEnumeratedHistogram(const std::string& name,
                                 int sample,
                                 int boundary_value) override;
  void RecordPerformanceHistogram(const std::string& name,
                                  double duration) override;
  void RecordUserMetricsAction(const std::string& name) override;
  void RecordImpression(const ImpressionEvent& event) override;
  void RecordClick(const ClickEvent& event) override;
  void RecordHover(const HoverEvent& event) override;
  void RecordDrag(const DragEvent& event) override;
  void RecordChange(const ChangeEvent& event) override;
  void RecordKeyDown(const KeyDownEvent& event) override;
  void SendJsonRequest(DispatchCallback callback,
                       const std::string& browser_id,
                       const std::string& url) override;
  void RegisterPreference(const std::string& name,
                          const CNRegisterOptions& options) override;
  void GetPreferences(DispatchCallback callback) override;
  void GetPreference(DispatchCallback callback,
                     const std::string& name) override;
  void SetPreference(const std::string& name,
                     const std::string& value) override;
  void RemovePreference(const std::string& name) override;
  void ClearPreferences() override;
  void GetSyncInformation(DispatchCallback callback) override;
  void Reattach(DispatchCallback callback) override;
  void ReadyForTest() override;
  void ConnectionReady() override;
  void SetOpenNewWindowForPopups(bool value) override;
  void RegisterExtensionsAPI(const std::string& origin,
                             const std::string& script) override;
  void ShowSurvey(DispatchCallback callback,
                  const std::string& trigger) override;
  void CanShowSurvey(DispatchCallback callback,
                     const std::string& trigger) override;
  void DoAidaConversation(DispatchCallback callback,
                          const std::string& request) override;

  void EnableRemoteDeviceCounter(bool enable);

  void SendMessageAck(int request_id,
                      const base::Value* arg1);
  void InnerAttach();

  // CitizenNotesAndroidBridge::DeviceCountListener override:
  void DeviceCountChanged(int count) override;

  // Forwards discovered devices to frontend.
  virtual void DevicesUpdated(const std::string& source,
                              const base::Value& targets);

  void ReadyToCommitNavigation(content::NavigationHandle* navigation_handle);
  void DocumentOnLoadCompletedInPrimaryMainFrame();
  void PrimaryPageChanged();
  void FrontendLoaded();

  void JsonReceived(DispatchCallback callback,
                    int result,
                    const std::string& message);
  void DevicesDiscoveryConfigUpdated();
  void SendPortForwardingStatus(base::Value status);

  // DevToolsFileHelper::Delegate overrides.
  void FileSystemAdded(
      const std::string& error,
      const DevToolsFileHelper::FileSystem* file_system) override;
  void FileSystemRemoved(const std::string& file_system_path) override;
  void FilePathsChanged(const std::vector<std::string>& changed_paths,
                        const std::vector<std::string>& added_paths,
                        const std::vector<std::string>& removed_paths) override;

  // DevToolsFileHelper callbacks.
  void FileSavedAs(const std::string& url, const std::string& file_system_path);
  void CanceledFileSaveAs(const std::string& url);
  void AppendedTo(const std::string& url);
  void IndexingTotalWorkCalculated(int request_id,
                                   const std::string& file_system_path,
                                   int total_work);
  void IndexingWorked(int request_id,
                      const std::string& file_system_path,
                      int worked);
  void IndexingDone(int request_id, const std::string& file_system_path);
  void SearchCompleted(int request_id,
                       const std::string& file_system_path,
                       const std::vector<std::string>& file_paths);
  void ShowCitizenNotesInfoBar(const std::u16string& message,
                           CitizenNotesInfoBarDelegate::Callback callback);
  base::TimeDelta GetTimeSinceLastAction();

  // Extensions support.
  void AddCitizenNotesExtensionsToClient();

  static CitizenNotesUIBindingsList& GetCitizenNotesUIBindings();

  void OnAidaConversationResponse(DispatchCallback callback,
                                  const std::string& response);
  class FrontendWebContentsObserver;
  std::unique_ptr<FrontendWebContentsObserver> frontend_contents_observer_;

  RAW_PTR_EXCLUSION Profile* profile_;
  RAW_PTR_EXCLUSION CitizenNotesAndroidBridge* android_bridge_;
  RAW_PTR_EXCLUSION content::WebContents* web_contents_;
  std::unique_ptr<Delegate> delegate_;
  scoped_refptr<content::CitizenNotesAgentHost> agent_host_;
  std::unique_ptr<content::CitizenNotesFrontendHost> frontend_host_;
  std::unique_ptr<DevToolsFileHelper> file_helper_;
  scoped_refptr<DevToolsFileSystemIndexer> file_system_indexer_;
  typedef std::map<
      int,
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> >
      IndexingJobsMap;
  IndexingJobsMap indexing_jobs_;

  bool devices_updates_enabled_;
  bool frontend_loaded_;
  std::unique_ptr<CitizenNotesTargetsUIHandler> remote_targets_handler_;
  std::unique_ptr<CNPortForwardingStatusSerializer> port_status_serializer_;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<CitizenNotesEmbedderMessageDispatcher>
      embedder_message_dispatcher_;
  GURL url_;

  class NetworkResourceLoader;
  std::set<std::unique_ptr<NetworkResourceLoader>, base::UniquePtrComparator>
      loaders_;

  using ExtensionsAPIs = std::map<std::string, std::string>;
  ExtensionsAPIs extensions_api_;
  std::string initial_target_id_;

  CitizenNotesSettings settings_;
  base::TimeTicks last_action_time_;

  std::unique_ptr<AidaClient> aida_client_;
  base::WeakPtrFactory<CitizenNotesUIBindings> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_UI_BINDINGS_H_
