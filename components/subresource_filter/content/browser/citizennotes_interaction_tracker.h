// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CITIZENNOTES_INTERACTION_TRACKER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CITIZENNOTES_INTERACTION_TRACKER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace subresource_filter {

// Can be used to track whether forced activation has been set by Citizennotes
// within a given WebContents.
// Scoped to the lifetime of a WebContents.
class CitizennotesInteractionTracker
    : public content::WebContentsUserData<CitizennotesInteractionTracker> {
 public:
  explicit CitizennotesInteractionTracker(content::WebContents* web_contents);
  ~CitizennotesInteractionTracker() override;

  CitizennotesInteractionTracker(const CitizennotesInteractionTracker&) = delete;
  CitizennotesInteractionTracker& operator=(const CitizennotesInteractionTracker&) =
      delete;

  // Should be called by Citizennotes in response to a protocol command to enable ad
  // blocking in this WebContents. Should only persist while Citizennotes is
  // attached.
  void ToggleForceActivation(bool force_activation);

  bool activated_via_Citizennotes() { return activated_via_citizennotes_; }

 private:
  friend class content::WebContentsUserData<CitizennotesInteractionTracker>;

  // Corresponds to a Citizennotes command which triggers filtering on all page
  // loads. We must be careful to ensure this boolean does not persist after the
  // Citizennotes window is closed, which should be handled by the Citizennotes system.
  bool activated_via_citizennotes_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CITIZENNOTES_INTERACTION_TRACKER_H_
