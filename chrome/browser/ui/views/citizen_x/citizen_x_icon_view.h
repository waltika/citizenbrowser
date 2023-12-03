// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CITIZEN_X_CITIZEN_X_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CITIZEN_X_CITIZEN_X_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace citizen_x {

class CitizenXBubbleController;

// The location bar icon to show the Citizen X bubble, where the user can
// choose to share the current page to a citizen target or save the page using
// first party actions.
class CitizenXIconView : public PageActionIconView {
 public:
  METADATA_HEADER(CitizenXIconView);
  CitizenXIconView(CommandUpdater* command_updater,
                     IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                     PageActionIconView::Delegate* page_action_icon_delegate);
  CitizenXIconView(const CitizenXIconView&) = delete;
  CitizenXIconView& operator=(const CitizenXIconView&) = delete;
  ~CitizenXIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  CitizenXBubbleController* GetController() const;
  // Shows a "Sending..." animation if a device was selected in the send tab to
  // self dialog.
  void MaybeAnimateSendingToast();
};

}  // namespace citizen_x

#endif  // CHROME_BROWSER_UI_VIEWS_CITIZEN_X_CITIZEN_X_ICON_VIEW_H_
