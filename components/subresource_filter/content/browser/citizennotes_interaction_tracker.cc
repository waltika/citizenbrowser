// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/citizennotes_interaction_tracker.h"

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

namespace subresource_filter {

CitizennotesInteractionTracker::CitizennotesInteractionTracker(
    content::WebContents* web_contents)
    : content::WebContentsUserData<CitizennotesInteractionTracker>(*web_contents) {}

CitizennotesInteractionTracker::~CitizennotesInteractionTracker() = default;

void CitizennotesInteractionTracker::ToggleForceActivation(bool force_activation) {
  if (!activated_via_citizennotes_ && force_activation)
    ContentSubresourceFilterThrottleManager::LogAction(
        SubresourceFilterAction::kForcedActivationEnabled);
  activated_via_citizennotes_ = force_activation;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CitizennotesInteractionTracker);

}  // namespace subresource_filter
