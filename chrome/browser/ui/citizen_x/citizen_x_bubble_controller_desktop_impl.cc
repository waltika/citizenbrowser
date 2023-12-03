// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/citizen_x/citizen_x_bubble_controller_desktop_impl.h"

#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/citizen_x/citizen_x_features.h"
#include "chrome/browser/citizen_x/citizen_x_model.h"
#include "chrome/browser/citizen_x/citizen_x_service.h"
#include "chrome/browser/citizen_x/citizen_x_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/citizen_x/citizen_x_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"

namespace citizen_x {

// static
// CitizenXBubbleController:
CitizenXBubbleController*
CitizenXBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  CitizenXBubbleControllerDesktopImpl::CreateForWebContents(web_contents);
  CitizenXBubbleControllerDesktopImpl* controller =
      CitizenXBubbleControllerDesktopImpl::FromWebContents(web_contents);
  return controller;
}

CitizenXBubbleControllerDesktopImpl::
    ~CitizenXBubbleControllerDesktopImpl() {
  if (citizen_x_bubble_view_) {
    citizen_x_bubble_view_->Hide();
  }
}

void CitizenXBubbleControllerDesktopImpl::HideBubble() {
  if (citizen_x_bubble_view_) {
    citizen_x_bubble_view_->Hide();
    citizen_x_bubble_view_ = nullptr;
  }
}

void CitizenXBubbleControllerDesktopImpl::ShowBubble(
    share::ShareAttempt attempt) {
  ui::ImageModel preview_image = GetPreviewImage();
  if (!preview_image.IsEmpty()) {
    attempt.preview_image = preview_image;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  citizen_x_bubble_view_ = browser->window()->ShowCitizenXBubble(attempt);

  share::LogShareSourceDesktop(share::ShareSourceDesktop::kOmniboxSharingHub);
}

CitizenXBubbleView*
CitizenXBubbleControllerDesktopImpl::citizen_x_bubble_view() const {
  return citizen_x_bubble_view_;
}

bool CitizenXBubbleControllerDesktopImpl::ShouldOfferOmniboxIcon() {
  return true;
}

std::u16string CitizenXBubbleControllerDesktopImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CITIZEN_X_TITLE);
}

Profile* CitizenXBubbleControllerDesktopImpl::GetProfile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

std::vector<CitizenXAction>
CitizenXBubbleControllerDesktopImpl::GetFirstPartyActions() {
  std::vector<CitizenXAction> actions;

  CitizenXModel* model = GetCitizenXModel();
  if (model) {
    actions = model->GetFirstPartyActionList(&GetWebContents());
  }

  return actions;
}

base::WeakPtr<CitizenXBubbleController>
CitizenXBubbleControllerDesktopImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CitizenXBubbleControllerDesktopImpl::OnActionSelected(
    const CitizenXAction& action) {
  Browser* browser = chrome::FindBrowserWithTab(&GetWebContents());
  // Can be null in tests.
  if (!browser) {
    return;
  }

  base::RecordComputedAction(action.feature_name_for_metrics);

  // Show a back button for 1P dialogs anchored to the citizen X icon.
  if (action.command_id == IDC_QRCODE_GENERATOR) {
    qrcode_generator::QRCodeGeneratorBubbleController* qrcode_controller =
        qrcode_generator::QRCodeGeneratorBubbleController::Get(
            &GetWebContents());
    qrcode_controller->ShowBubble(GetWebContents().GetLastCommittedURL(), true);
  } else if (action.command_id == IDC_SEND_TAB_TO_SELF) {
    send_tab_to_self::ShowBubble(&GetWebContents(),
                                 /*show_back_button=*/true);
  } else if (action.command_id == IDC_ROUTE_MEDIA) {
    media_router::MediaRouterDialogController* dialog_controller =
        media_router::MediaRouterDialogController::GetOrCreateForWebContents(
            browser->tab_strip_model()->GetActiveWebContents());
    if (!dialog_controller) {
      return;
    }

    dialog_controller->ShowMediaRouterDialog(
        media_router::MediaRouterDialogActivationLocation::CITIZEN_X);
  } else {
    chrome::ExecuteCommand(browser, action.command_id);
  }
}

void CitizenXBubbleControllerDesktopImpl::OnBubbleClosed() {
  citizen_x_bubble_view_ = nullptr;
}

CitizenXModel* CitizenXBubbleControllerDesktopImpl::GetCitizenXModel() {
  if (!citizen_x_model_) {
    CitizenXService* const service =
        CitizenXServiceFactory::GetForProfile(GetProfile());
    if (!service) {
      return nullptr;
    }
    citizen_x_model_ = service->GetCitizenXModel();
  }
  return citizen_x_model_;
}

ui::ImageModel CitizenXBubbleControllerDesktopImpl::GetPreviewImage() {
  content::WebContents* web_contents = &GetWebContents();
  gfx::Image favicon = favicon::TabFaviconFromWebContents(web_contents);
  if (favicon.IsEmpty()) {
    return {};
  }

  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  // Select chrome URLs show themified icons in dark mode.
  if (favicon::ShouldThemifyFaviconForEntry(entry) &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    const ui::ColorProvider& color_provider = web_contents->GetColorProvider();
    favicon = gfx::Image(favicon::ThemeFavicon(
        *favicon.ToImageSkia(),
        color_provider.GetColor(kColorToolbarButtonIcon),
        color_provider.GetColor(kColorTabBackgroundActiveFrameActive),
        color_provider.GetColor(kColorTabBackgroundInactiveFrameActive)));
  }

  return ui::ImageModel::FromImage(favicon);
}

CitizenXBubbleControllerDesktopImpl::CitizenXBubbleControllerDesktopImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CitizenXBubbleControllerDesktopImpl>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CitizenXBubbleControllerDesktopImpl);

}  // namespace citizen_x
