// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/browser/citizen_x/device/citizennotes_android_bridge.h"
#include "chrome/browser/citizen_x/device/cntcp_device_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

class CitizenNotesAndroidBridgeTest : public InProcessBrowserTest {
};

static void assign_from_callback(scoped_refptr<TCPDeviceProvider>* store,
                                 int* invocation_counter,
                                 scoped_refptr<TCPDeviceProvider> value) {
  (*invocation_counter)++;
  *store = value;
}

static std::string SetToString(const std::set<std::string>& values) {
  std::ostringstream result;
  base::ranges::copy(values, std::ostream_iterator<std::string>(result, ", "));
  std::string result_string = result.str();
  return result_string.substr(0, result_string.length() - 2);
}

static std::string AllTargetsString(
    scoped_refptr<TCPDeviceProvider> provider) {
  std::set<std::string> actual;
  for (const net::HostPortPair& hostport : provider->get_targets_for_test())
    actual.insert(hostport.ToString());
  return SetToString(actual);
}

IN_PROC_BROWSER_TEST_F(CitizenNotesAndroidBridgeTest, DiscoveryListChanges) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  service->ClearPref(prefs::kCitizenNotesTCPDiscoveryConfig);
  service->SetBoolean(prefs::kCitizenNotesDiscoverTCPTargetsEnabled, true);

  CitizenNotesAndroidBridge* bridge =
      CitizenNotesAndroidBridge::Factory::GetForProfile(profile);

  scoped_refptr<TCPDeviceProvider> provider;
  int called = 0;
  bridge->set_tcp_provider_callback_for_test(
      base::BindRepeating(assign_from_callback, &provider, &called));

  EXPECT_LT(0, called);
  EXPECT_NE(nullptr, provider);

  EXPECT_STREQ("localhost:9222, localhost:9229",
               AllTargetsString(provider).c_str());

  int invocations = called;
  base::Value::List list;
  list.Append("somehost:2000");

  service->SetList(prefs::kCitizenNotesTCPDiscoveryConfig, list.Clone());

  EXPECT_LT(invocations, called);
  EXPECT_NE(nullptr, provider);
  EXPECT_STREQ("somehost:2000", AllTargetsString(provider).c_str());

  invocations = called;
  list.clear();
  service->SetList(prefs::kCitizenNotesTCPDiscoveryConfig, list.Clone());

  EXPECT_LT(invocations, called);
  EXPECT_EQ(nullptr, provider);
  invocations = called;

  list.Append("b:1");
  list.Append("c:2");
  list.Append("<not really a good address.");
  list.Append("d:3");
  list.Append("c:2");
  service->SetList(prefs::kCitizenNotesTCPDiscoveryConfig, std::move(list));

  EXPECT_LT(invocations, called);
  EXPECT_NE(nullptr, provider);
  EXPECT_STREQ("b:1, c:2, d:3", AllTargetsString(provider).c_str());
}

IN_PROC_BROWSER_TEST_F(CitizenNotesAndroidBridgeTest, DefaultValues) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  CitizenNotesAndroidBridge::Factory::GetForProfile(profile);
  service->ClearPref(prefs::kCitizenNotesDiscoverTCPTargetsEnabled);
  service->ClearPref(prefs::kCitizenNotesTCPDiscoveryConfig);

  const base::Value::List& targets =
      service->GetList(prefs::kCitizenNotesTCPDiscoveryConfig);
  EXPECT_EQ(2ul, targets.size());

  std::set<std::string> actual;
  for (const base::Value& item : targets) {
    std::string value;
    if (item.is_string())
      value = item.GetString();
    actual.insert(value);
  }
  EXPECT_STREQ("localhost:9222, localhost:9229", SetToString(actual).c_str());
  EXPECT_TRUE(service->GetBoolean(prefs::kCitizenNotesDiscoverTCPTargetsEnabled));
}

IN_PROC_BROWSER_TEST_F(CitizenNotesAndroidBridgeTest, TCPEnableChange) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  service->ClearPref(prefs::kCitizenNotesTCPDiscoveryConfig);
  service->ClearPref(prefs::kCitizenNotesDiscoverTCPTargetsEnabled);

  CitizenNotesAndroidBridge* bridge =
      CitizenNotesAndroidBridge::Factory::GetForProfile(profile);

  scoped_refptr<TCPDeviceProvider> provider;
  int called = 0;
  bridge->set_tcp_provider_callback_for_test(
      base::BindRepeating(assign_from_callback, &provider, &called));

  EXPECT_NE(nullptr, provider);

  service->SetBoolean(prefs::kCitizenNotesDiscoverTCPTargetsEnabled, true);

  EXPECT_NE(nullptr, provider);
  EXPECT_STREQ("localhost:9222, localhost:9229",
               AllTargetsString(provider).c_str());

  service->SetBoolean(prefs::kCitizenNotesDiscoverTCPTargetsEnabled, false);

  EXPECT_EQ(nullptr, provider);
}
