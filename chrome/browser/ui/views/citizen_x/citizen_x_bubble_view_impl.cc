// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/citizen_x/citizen_x_bubble_view_impl.h"

#include "chrome/browser/share/share_features.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/ui/citizen_x/citizen_x_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/citizen_x/citizen_x_bubble_action_button.h"
#include "chrome/browser/ui/views/sharing_hub/preview_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace citizen_x {

namespace {

// The valid action button height.
constexpr int kActionButtonHeight = 56;
// Maximum number of buttons that are shown without scrolling. If the number of
// actions is larger than kMaximumButtons, the bubble will be scrollable.
constexpr int kMaximumButtons = 10;

// Used to group the action buttons together, which makes moving between them
// with arrow keys possible.
constexpr int kActionButtonGroup = 0;

constexpr int kInterItemPadding = 4;

}  // namespace

CitizenXBubbleViewImpl::CitizenXBubbleViewImpl(
    views::View* anchor_view,
    share::ShareAttempt attempt,
    CitizenXBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, attempt.web_contents.get()),
      attempt_(attempt) {
  DCHECK(anchor_view);
  DCHECK(controller);

  SetAccessibleTitle(l10n_util::GetStringUTF16(IDS_CITIZEN_X_TOOLTIP));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  RegisterWindowClosingCallback(base::BindOnce(
      &CitizenXBubbleViewImpl::OnWindowClosing, base::Unretained(this)));
  SetEnableArrowKeyTraversal(true);
  SetShowCloseButton(false);
  SetShowTitle(false);

  controller_ = controller->GetWeakPtr();
}

CitizenXBubbleViewImpl::~CitizenXBubbleViewImpl() = default;

void CitizenXBubbleViewImpl::Hide() {
  OnWindowClosing();
  CloseBubble();
}

void CitizenXBubbleViewImpl::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  if (GetWidget()) {
    set_color(GetColorProvider()->GetColor(ui::kColorMenuBackground));
  }
}

void CitizenXBubbleViewImpl::OnActionSelected(
    CitizenXBubbleActionButton* button) {
  if (!controller_)
    return;

  // The announcement has to happen here rather than in the button itself: the
  // button doesn't know whether controller_ will be null, so it doesn't know
  // whether the action will actually happen.
  const CitizenXAction& action = button->action_info();
  if (action.announcement_id) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(action.announcement_id));
  }
  controller_->OnActionSelected(action);

  Hide();
}

void CitizenXBubbleViewImpl::Init() {
  const int kPadding = 8;
  set_margins(gfx::Insets::TLBR(kPadding, 0, kPadding, 0));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kInterItemPadding));

  AddChildView(std::make_unique<sharing_hub::PreviewView>(attempt_));
  AddChildView(std::make_unique<views::Separator>());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kActionButtonHeight * kMaximumButtons);
  scroll_view_->SetBackgroundThemeColorId(ui::kColorMenuBackground);

  PopulateScrollView(controller_->GetFirstPartyActions());
}

void CitizenXBubbleViewImpl::PopulateScrollView(
    const std::vector<CitizenXAction>& first_party_actions) {
  auto* action_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  action_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kInterItemPadding));

  for (const auto& action : first_party_actions) {
    auto* view = action_list_view->AddChildView(
        std::make_unique<CitizenXBubbleActionButton>(this, action));
    view->SetGroup(kActionButtonGroup);
  }

  MaybeSizeToContents();
  Layout();
}

void CitizenXBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

void CitizenXBubbleViewImpl::OnWindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

BEGIN_METADATA(CitizenXBubbleViewImpl, LocationBarBubbleDelegateView)
END_METADATA

}  // namespace citizen_x
