// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CITIZEN_X_CITIZEN_X_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_CITIZEN_X_CITIZEN_X_BUBBLE_VIEW_H_

namespace citizen_x {

// Interface to display the Citizen hub bubble.
// This object is responsible for its own lifetime.
class CitizenXBubbleView {
 public:
  virtual ~CitizenXBubbleView() = default;

  // Closes the bubble and prevents future calls into the controller.
  virtual void Hide() = 0;
};

}  // namespace citizen_hub

#endif  // CHROME_BROWSER_UI_CITIZEN_X_CITIZEN_X_BUBBLE_VIEW_H_
