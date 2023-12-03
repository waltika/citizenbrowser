// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/citizen_x/citizen_x_icon_view.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/citizen_x/citizen_x_bubble_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/citizen_x/citizen_x_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"

namespace citizen_x {

namespace {

send_tab_to_self::SendTabToSelfBubbleController* GetSendTabToSelfController(
    content::WebContents* web_contents) {
  return send_tab_to_self::SendTabToSelfBubbleController::
      CreateOrGetFromWebContents(web_contents);
}

bool IsQRCodeDialogOpen(content::WebContents* web_contents) {
  qrcode_generator::QRCodeGeneratorBubbleController* controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
  return controller && controller->IsBubbleShown();
}

bool IsSendTabToSelfDialogOpen(content::WebContents* web_contents) {
  send_tab_to_self::SendTabToSelfBubbleController* controller =
      GetSendTabToSelfController(web_contents);
  return controller && controller->IsBubbleShown();
}

}  // namespace

CitizenXIconView::CitizenXIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_CITIZEN_X,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "CitizenX",
                         false) {
  SetID(VIEW_ID_CITIZEN_X_BUTTON);
  SetVisible(false);
  SetLabel(
      l10n_util::GetStringUTF16(IDS_BROWSER_CITIZEN_X_OMNIBOX_SENDING_LABEL));
  SetUpForInOutAnimation();
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(IDS_CITIZEN_X_TOOLTIP));
}

CitizenXIconView::~CitizenXIconView() = default;

views::BubbleDialogDelegate* CitizenXIconView::GetBubble() const {
  CitizenXBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<CitizenXBubbleViewImpl*>(
      controller->citizen_x_bubble_view());
}

void CitizenXIconView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  // |controller| may be nullptr due to lazy initialization.
  CitizenXBubbleController* controller = GetController();
  bool enabled = controller && controller->ShouldOfferOmniboxIcon();

  SetCommandEnabled(enabled);
  SetVisible(enabled);

  if (IsQRCodeDialogOpen(web_contents) ||
      IsSendTabToSelfDialogOpen(web_contents)) {
    SetHighlighted(true);
  }

  if (enabled) {
    MaybeAnimateSendingToast();
  }
}

void CitizenXIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& CitizenXIconView::GetVectorIcon() const {
  return GetCitizenXVectorIcon();
}

CitizenXBubbleController* CitizenXIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return CitizenXBubbleController::CreateOrGetFromWebContents(web_contents);
}

void CitizenXIconView::MaybeAnimateSendingToast() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  send_tab_to_self::SendTabToSelfBubbleController* controller =
      GetSendTabToSelfController(web_contents);

  if (controller && controller->show_message()) {
    controller->set_show_message(false);
    AnimateIn(absl::nullopt);
  }
}

BEGIN_METADATA(CitizenXIconView, PageActionIconView)
END_METADATA

}  // namespace citizen_x
