// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CITIZEN_X_CITIZEN_X_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
#define CHROME_BROWSER_UI_CITIZEN_X_CITIZEN_X_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/share/share_attempt.h"
#include "chrome/browser/ui/citizen_x/citizen_x_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace citizen_x {

class CitizenXBubbleView;
class CitizenXModel;
struct CitizenXAction;

// Controller component of the Citizen X dialog bubble.
// Responsible for showing and hiding an owned bubble.
class CitizenXBubbleControllerDesktopImpl
    : public CitizenXBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          CitizenXBubbleControllerDesktopImpl> {
 public:
  CitizenXBubbleControllerDesktopImpl(
      const CitizenXBubbleControllerDesktopImpl&) = delete;
  CitizenXBubbleControllerDesktopImpl& operator=(
      const CitizenXBubbleControllerDesktopImpl&) = delete;

  ~CitizenXBubbleControllerDesktopImpl() override;

  // CitizenXBubbleController:
  void HideBubble() override;
  void ShowBubble(share::ShareAttempt attempt) override;
  CitizenXBubbleView* citizen_x_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;

  // Returns the title of the Citizen X bubble.
  std::u16string GetWindowTitle() const;
  // Returns the current profile.
  Profile* GetProfile() const;

  // CitizenXBubbleController:
  std::vector<CitizenXAction> GetFirstPartyActions() override;
  base::WeakPtr<CitizenXBubbleController> GetWeakPtr() override;
  void OnActionSelected(const CitizenXAction& action) override;
  void OnBubbleClosed() override;

 protected:
  explicit CitizenXBubbleControllerDesktopImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      CitizenXBubbleControllerDesktopImpl>;

  CitizenXModel* GetCitizenXModel();

  // This method can return an empty preview image if the site doesn't have a
  // favicon and we can't generate a preview image for some reason.
  ui::ImageModel GetPreviewImage();

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<CitizenXBubbleView> citizen_x_bubble_view_ = nullptr;
  // Cached reference to the model.
  raw_ptr<CitizenXModel> citizen_x_model_ = nullptr;

  base::WeakPtrFactory<CitizenXBubbleController> weak_factory_{this};

  // This is a bit ugly: CitizenXBubbleController's interface requires it to
  // be able to create WeakPtr<CitizenXBubbleController>, but this class
  // internally also needs to be able to bind weak pointers to itself for use
  // with the image fetching state machine. Those internal weak pointers need to
  // be to an instance of *this class*, not of the parent interface, so that we
  // can bind them to methods on this class rather than the parent interface.
  base::WeakPtrFactory<CitizenXBubbleControllerDesktopImpl>
      internal_weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace citizen_x

#endif  // CHROME_BROWSER_UI_CITIZEN_X_CITIZEN_X_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
