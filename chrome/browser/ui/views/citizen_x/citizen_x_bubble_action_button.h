// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CITIZEN_X_CITIZEN_X_BUBBLE_ACTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CITIZEN_X_CITIZEN_X_BUBBLE_ACTION_BUTTON_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/citizen_x/citizen_x_model.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace citizen_x {

class CitizenXBubbleViewImpl;
struct CitizenXAction;

// A button representing an action in the Citizen X bubble.
class CitizenXBubbleActionButton : public views::Button {
 public:
  METADATA_HEADER(CitizenXBubbleActionButton);
  CitizenXBubbleActionButton(CitizenXBubbleViewImpl* bubble,
                               const CitizenXAction& action_info);
  CitizenXBubbleActionButton(const CitizenXBubbleActionButton&) = delete;
  CitizenXBubbleActionButton& operator=(const CitizenXBubbleActionButton&) =
      delete;
  ~CitizenXBubbleActionButton() override;

  const CitizenXAction& action_info() const { return action_info_; }

  // views::Button:
  // Listeners for various events, which this class uses to keep its visuals
  // consistent.
  void OnThemeChanged() override;
  void StateChanged(views::Button::ButtonState old_state) override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  const CitizenXAction action_info_;

  raw_ptr<views::Label> title_;
  raw_ptr<views::ImageView> image_;

  void UpdateColors();
};

}  // namespace citizen_x

#endif  // CHROME_BROWSER_UI_VIEWS_CITIZEN_X_CITIZEN_X_BUBBLE_ACTION_BUTTON_H_
