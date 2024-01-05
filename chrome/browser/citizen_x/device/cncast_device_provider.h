// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_DEVICE_CAST_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_CITIZENNOTES_DEVICE_CAST_DEVICE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/citizen_x/device/cnandroid_device_manager.h"
#include "chrome/browser/citizen_x/device/cntcp_device_provider.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "content/public/browser/browser_thread.h"

// Supplies Cast device information for the purposes of remote debugging Cast
// applications over ADB.
class CNCastDeviceProvider
    : public CNAndroidDeviceManager::DeviceProvider,
      public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  CNCastDeviceProvider();

  CNCastDeviceProvider(const CNCastDeviceProvider&) = delete;
  CNCastDeviceProvider& operator=(const CNCastDeviceProvider&) = delete;

  // DeviceProvider implementation:
  void QueryDevices(SerialsCallback callback) override;
  void QueryDeviceInfo(const std::string& serial,
                       DeviceInfoCallback callback) override;
  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  SocketCallback callback) override;

  // ServiceDiscoveryDeviceLister::Delegate implementation:
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;

 private:
  class DeviceListerDelegate;

  ~CNCastDeviceProvider() override;

  scoped_refptr<CNTCPDeviceProvider> tcp_provider_;
  std::unique_ptr<DeviceListerDelegate,
                  content::BrowserThread::DeleteOnUIThread>
      lister_delegate_;

  // Keyed on the hostname (IP address).
  std::map<std::string, CNAndroidDeviceManager::DeviceInfo> device_info_map_;
  // Maps a service name to the hostname (IP address).
  std::map<std::string, std::string> service_hostname_map_;

  base::WeakPtrFactory<CNCastDeviceProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CITIZENNOTES_DEVICE_CAST_DEVICE_PROVIDER_H_
