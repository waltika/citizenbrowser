// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_DEVICE_PORT_FORWARDING_CONTROLLER_H_
#define CHROME_BROWSER_CITIZENNOTES_DEVICE_PORT_FORWARDING_CONTROLLER_H_

#include <map>
#include <string>

#include "chrome/browser/citizen_x/device/citizennotes_android_bridge.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;
class Profile;

class CNPortForwardingController {
 public:
  typedef CitizenNotesAndroidBridge::PortStatus PortStatus;
  typedef CitizenNotesAndroidBridge::PortStatusMap PortStatusMap;
  typedef CitizenNotesAndroidBridge::BrowserStatus BrowserStatus;
  typedef CitizenNotesAndroidBridge::ForwardingStatus ForwardingStatus;

  explicit CNPortForwardingController(Profile* profile);

  CNPortForwardingController(const CNPortForwardingController&) = delete;
  CNPortForwardingController& operator=(const CNPortForwardingController&) = delete;

  virtual ~CNPortForwardingController();

  ForwardingStatus DeviceListChanged(
      const CitizenNotesAndroidBridge::CompleteDevices& complete_devices);
  void CloseAllConnections();

 private:
  class Connection;
  typedef std::map<std::string, Connection*> Registry;

  void OnPrefsChange();

  void UpdateConnections();

  RAW_PTR_EXCLUSION Profile* profile_;
  RAW_PTR_EXCLUSION PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  Registry registry_;

  typedef std::map<int, std::string> ForwardingMap;
  ForwardingMap forwarding_map_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_DEVICE_PORT_FORWARDING_CONTROLLER_H_
