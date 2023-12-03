// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZEN_X_CITIZEN_X_MODEL_H_
#define CHROME_BROWSER_CITIZEN_X_CITIZEN_X_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "ui/gfx/image/image_skia.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace citizen_x {

struct CitizenXAction {
  CitizenXAction(int command_id,
                   std::u16string title,
                   const gfx::VectorIcon* icon,
                   std::string feature_name_for_metrics,
                   int announcement_id);
  CitizenXAction(const CitizenXAction&);
  CitizenXAction& operator=(const CitizenXAction&);
  CitizenXAction(CitizenXAction&&);
  CitizenXAction& operator=(CitizenXAction&&);
  ~CitizenXAction() = default;
  int command_id;
  std::u16string title;
  raw_ptr<const gfx::VectorIcon> icon;
  std::string feature_name_for_metrics;
  int announcement_id;
};

// The Citizen X model contains a list of first and third party actions.
// This object should only be accessed from one thread, which is usually the
// main thread.
class CitizenXModel {
 public:
  explicit CitizenXModel(content::BrowserContext* context);
  CitizenXModel(const CitizenXModel&) = delete;
  CitizenXModel& operator=(const CitizenXModel&) = delete;
  ~CitizenXModel();

  // Returns a vector with the first party Citizen X actions, ordered by
  // appearance in the dialog. Some actions (i.e. send tab to self) may not be
  // shown for some URLs.
  std::vector<CitizenXAction> GetFirstPartyActionList(
      content::WebContents* web_contents) const;

  // Executes the third party action indicated by |id|, i.e. opens a popup to
  // the corresponding webpage. The |url| is the URL to share, and the |title|
  // is the title (if there is one) of the shared URL.
  void ExecuteThirdPartyAction(Profile* profile,
                               const GURL& url,
                               const std::u16string& title,
                               int id);

  // Convenience wrapper around the above when citizen a WebContents. This
  // extracts the title and URL to share from the provided WebContents.
  void ExecuteThirdPartyAction(content::WebContents* contents, int id);

 private:
  void PopulateFirstPartyActions();
  void PopulateThirdPartyActions();

  // A list of Citizen X first party actions in order in which they appear.
  std::vector<CitizenXAction> first_party_action_list_;

  raw_ptr<content::BrowserContext> context_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace citizen_x

#endif  // CHROME_BROWSER_CITIZEN_X_CITIZEN_X_MODEL_H_
