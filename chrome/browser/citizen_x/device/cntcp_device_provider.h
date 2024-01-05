// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_DEVICE_TCP_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_CITIZENNOTES_DEVICE_TCP_DEVICE_PROVIDER_H_

#include <stdint.h>

#include <set>

#include "chrome/browser/citizen_x/device/cnandroid_device_manager.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

class CNTCPDeviceProvider : public CNAndroidDeviceManager::DeviceProvider {
 public:
  static scoped_refptr<CNTCPDeviceProvider> CreateForLocalhost(uint16_t port);

  using HostPortSet = std::set<net::HostPortPair>;
  explicit CNTCPDeviceProvider(const HostPortSet& targets);

  void QueryDevices(SerialsCallback callback) override;

  void QueryDeviceInfo(const std::string& serial,
                       DeviceInfoCallback callback) override;

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  SocketCallback callback) override;

  void ReleaseDevice(const std::string& serial) override;

  void set_release_callback_for_test(base::OnceClosure callback);

  HostPortSet get_targets_for_test() { return targets_; }

 private:
  ~CNTCPDeviceProvider() override;

  HostPortSet targets_;
  base::OnceClosure release_callback_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_DEVICE_TCP_DEVICE_PROVIDER_H_
