// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_DEVICE_CITIZENNOTES_DEVICE_DISCOVERY_H_
#define CHROME_BROWSER_CITIZENNOTES_DEVICE_CITIZENNOTES_DEVICE_DISCOVERY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/citizen_x/device/cnandroid_device_manager.h"
#include "content/public/browser/citizennotes_agent_host.h"

class CitizenNotesDeviceDiscovery {
 public:
  class RemotePage : public base::RefCountedThreadSafe<RemotePage> {
   public:
    RemotePage(const RemotePage&) = delete;
    RemotePage& operator=(const RemotePage&) = delete;

    scoped_refptr<CNAndroidDeviceManager::Device> device() { return device_; }
    const std::string& socket() { return browser_id_; }
    const std::string& frontend_url() { return frontend_url_; }
    scoped_refptr<content::CitizenNotesAgentHost> CreateTarget();

   private:
    friend class base::RefCountedThreadSafe<RemotePage>;
    friend class CitizenNotesDeviceDiscovery;

    RemotePage(scoped_refptr<CNAndroidDeviceManager::Device> device,
               const std::string& browser_id,
               const std::string& browser_version,
               base::Value::Dict dict);

    virtual ~RemotePage();

    scoped_refptr<CNAndroidDeviceManager::Device> device_;
    std::string browser_id_;
    std::string browser_version_;
    std::string frontend_url_;
    base::Value::Dict dict_;
    scoped_refptr<content::CitizenNotesAgentHost> agent_host_;
  };

  using RemotePages = std::vector<scoped_refptr<RemotePage>>;

  class RemoteBrowser : public base::RefCountedThreadSafe<RemoteBrowser> {
   public:
    RemoteBrowser(const RemoteBrowser&) = delete;
    RemoteBrowser& operator=(const RemoteBrowser&) = delete;

    const std::string& serial() { return serial_; }
    const std::string& socket() { return browser_id_; }
    const std::string& display_name() { return display_name_; }
    const std::string& user() { return user_; }
    const std::string& version() { return version_; }
    const std::string& browser_target_id() { return browser_target_id_; }
    const RemotePages& pages() { return pages_; }

    bool IsChrome();
    std::string GetId();

    using ParsedVersion = std::vector<int>;
    ParsedVersion GetParsedVersion();

   private:
    friend class base::RefCountedThreadSafe<RemoteBrowser>;
    friend class CitizenNotesDeviceDiscovery;

    RemoteBrowser(const std::string& serial,
                  const CNAndroidDeviceManager::BrowserInfo& browser_info);

    virtual ~RemoteBrowser();

    std::string serial_;
    std::string browser_id_;
    std::string display_name_;
    std::string user_;
    CNAndroidDeviceManager::BrowserInfo::Type type_;
    std::string version_;
    std::string browser_target_id_;
    RemotePages pages_;
  };

  using RemoteBrowsers = std::vector<scoped_refptr<RemoteBrowser>>;

  class RemoteDevice : public base::RefCountedThreadSafe<RemoteDevice> {
   public:
    RemoteDevice(const RemoteDevice&) = delete;
    RemoteDevice& operator=(const RemoteDevice&) = delete;

    std::string serial() { return serial_; }
    std::string model() { return model_; }
    bool is_connected() { return connected_; }
    RemoteBrowsers& browsers() { return browsers_; }
    gfx::Size screen_size() { return screen_size_; }

   private:
    friend class base::RefCountedThreadSafe<RemoteDevice>;
    friend class CitizenNotesDeviceDiscovery;

    RemoteDevice(const std::string& serial,
                 const CNAndroidDeviceManager::DeviceInfo& device_info);

    virtual ~RemoteDevice();

    std::string serial_;
    std::string model_;
    bool connected_;
    RemoteBrowsers browsers_;
    gfx::Size screen_size_;
  };

  using RemoteDevices = std::vector<scoped_refptr<RemoteDevice>>;

  using CompleteDevice =
      std::pair<scoped_refptr<CNAndroidDeviceManager::Device>,
                scoped_refptr<RemoteDevice>>;
  using CompleteDevices = std::vector<CompleteDevice>;
  using DeviceListCallback =
      base::RepeatingCallback<void(const CompleteDevices&)>;

  CitizenNotesDeviceDiscovery(CNAndroidDeviceManager* device_manager,
                          DeviceListCallback callback);

  CitizenNotesDeviceDiscovery(const CitizenNotesDeviceDiscovery&) = delete;
  CitizenNotesDeviceDiscovery& operator=(const CitizenNotesDeviceDiscovery&) = delete;

  ~CitizenNotesDeviceDiscovery();

  void SetScheduler(base::RepeatingCallback<void(base::OnceClosure)> scheduler);

  static scoped_refptr<content::CitizenNotesAgentHost> CreateBrowserAgentHost(
      scoped_refptr<CNAndroidDeviceManager::Device> device,
      scoped_refptr<RemoteBrowser> browser);

 private:
  class DiscoveryRequest;

  void RequestDeviceList();
  void ReceivedDeviceList(const CompleteDevices& complete_devices);

  RAW_PTR_EXCLUSION CNAndroidDeviceManager* device_manager_;
  const DeviceListCallback callback_;
  base::RepeatingCallback<void(base::OnceClosure)> task_scheduler_;
  base::WeakPtrFactory<CitizenNotesDeviceDiscovery> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CITIZENNOTES_DEVICE_CITIZENNOTES_DEVICE_DISCOVERY_H_
