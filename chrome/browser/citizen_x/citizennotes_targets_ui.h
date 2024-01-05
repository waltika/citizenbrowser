// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_TARGETS_UI_H_
#define CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_TARGETS_UI_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/citizen_x/device/citizennotes_android_bridge.h"

class Profile;

class CitizenNotesTargetsUIHandler {
 public:
  using Callback =
      base::RepeatingCallback<void(const std::string&, const base::Value&)>;

  CitizenNotesTargetsUIHandler(const std::string& source_id, Callback callback);

  CitizenNotesTargetsUIHandler(const CitizenNotesTargetsUIHandler&) = delete;
  CitizenNotesTargetsUIHandler& operator=(const CitizenNotesTargetsUIHandler&) = delete;

  virtual ~CitizenNotesTargetsUIHandler();

  std::string source_id() const { return source_id_; }

  static std::unique_ptr<CitizenNotesTargetsUIHandler> CreateForLocal(
      Callback callback,
      Profile* profile);

  static std::unique_ptr<CitizenNotesTargetsUIHandler> CreateForAdb(
      Callback callback,
      Profile* profile);

  scoped_refptr<content::CitizenNotesAgentHost> GetTarget(
      const std::string& target_id);

  virtual void Open(const std::string& browser_id, const std::string& url);

  virtual scoped_refptr<content::CitizenNotesAgentHost> GetBrowserAgentHost(
      const std::string& browser_id);

  virtual void ForceUpdate();

 protected:
  base::Value::Dict Serialize(content::CitizenNotesAgentHost* host);
  void SendSerializedTargets(const base::Value& list);

  using TargetMap =
      std::map<std::string, scoped_refptr<content::CitizenNotesAgentHost>>;
  TargetMap targets_;

 private:
  const std::string source_id_;
  Callback callback_;
};

class CNPortForwardingStatusSerializer
    : private CitizenNotesAndroidBridge::PortForwardingListener {
 public:
  using Callback = base::RepeatingCallback<void(base::Value)>;

  CNPortForwardingStatusSerializer(const Callback& callback, Profile* profile);
  ~CNPortForwardingStatusSerializer() override;

  void PortStatusChanged(const ForwardingStatus& status) override;

 private:
  Callback callback_;
  RAW_PTR_EXCLUSION Profile* profile_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_TARGETS_UI_H_
