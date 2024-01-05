// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_CHROME_CITIZENNOTES_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_CITIZENNOTES_CHROME_CITIZENNOTES_MANAGER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "chrome/browser/citizen_x/device/citizennotes_device_discovery.h"
#include "chrome/browser/citizen_x/protocol/protocol.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/citizennotes_agent_host_observer.h"
#include "content/public/browser/citizennotes_manager_delegate.h"
#include "net/base/host_port_pair.h"

class ChromeCitizenNotesSession;
class Profile;
class ScopedKeepAlive;
using RemoteLocations = std::set<net::HostPortPair>;

namespace extensions {
class Extension;
}

namespace web_app {
class WebApp;
}

class ChromeCitizenNotesManagerDelegate : public content::CitizenNotesManagerDelegate {
 public:
  static const char kTypeApp[];
  static const char kTypeBackgroundPage[];
  static const char kTypePage[];

  ChromeCitizenNotesManagerDelegate();

  ChromeCitizenNotesManagerDelegate(const ChromeCitizenNotesManagerDelegate&) = delete;
  ChromeCitizenNotesManagerDelegate& operator=(
      const ChromeCitizenNotesManagerDelegate&) = delete;

  ~ChromeCitizenNotesManagerDelegate() override;

  static ChromeCitizenNotesManagerDelegate* GetInstance();
  void UpdateDeviceDiscovery();

  // |web_contents| may be null, in which case this function just checks
  // the settings for |profile|.
  static bool AllowInspection(Profile* profile,
                              content::WebContents* web_contents);

  // |extension| may be null, in which case this function just checks
  // the settings for |profile|.
  static bool AllowInspection(Profile* profile,
                              const extensions::Extension* extension);

  // |web_app| may be null, in which case this function just checks
  // the settings for |profile|.
  static bool AllowInspection(Profile* profile, const web_app::WebApp* web_app);

  // Resets |device_manager_|.
  void ResetAndroidDeviceManagerForTesting();

  std::vector<content::BrowserContext*> GetBrowserContexts() override;
  content::BrowserContext* GetDefaultBrowserContext() override;

  // Closes browser soon, not in the current task.
  static void CloseBrowserSoon();

 private:
  friend class CitizenNotesManagerDelegateTest;

  // content::CitizenNotesManagerDelegate implementation.
  void Inspect(content::CitizenNotesAgentHost* agent_host) override;
  void HandleCommand(content::CitizenNotesAgentHostClientChannel* channel,
                     base::span<const uint8_t> message,
                     NotHandledCallback callback) override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  std::string GetTargetTitle(content::WebContents* web_contents) override;

  content::BrowserContext* CreateBrowserContext() override;
  void DisposeBrowserContext(content::BrowserContext*,
                             DisposeCallback callback) override;

  bool AllowInspectingRenderFrameHost(content::RenderFrameHost* rfh) override;
  void ClientAttached(
      content::CitizenNotesAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::CitizenNotesAgentHostClientChannel* channel) override;
  scoped_refptr<content::CitizenNotesAgentHost> CreateNewTarget(
      const GURL& url,
      TargetType target_type) override;
  bool HasBundledFrontendResources() override;

  void DevicesAvailable(
      const CitizenNotesDeviceDiscovery::CompleteDevices& devices);

  std::map<content::CitizenNotesAgentHostClientChannel*,
           std::unique_ptr<ChromeCitizenNotesSession>>
      sessions_;

  std::unique_ptr<CNAndroidDeviceManager> device_manager_;
  std::unique_ptr<CitizenNotesDeviceDiscovery> device_discovery_;
  content::CitizenNotesAgentHost::List remote_agent_hosts_;
  RemoteLocations remote_locations_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_CHROME_CITIZENNOTES_MANAGER_DELEGATE_H_
