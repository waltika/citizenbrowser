// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizennotes_infobar_delegate.h"

#include "chrome/browser/citizen_x/cnglobal_confirm_info_bar.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
void CitizenNotesInfoBarDelegate::Create(const std::u16string& message,
                                     Callback callback) {
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new CitizenNotesInfoBarDelegate(message, std::move(callback)));
  CNGlobalConfirmInfoBar::Show(std::move(delegate));
}

CitizenNotesInfoBarDelegate::CitizenNotesInfoBarDelegate(const std::u16string& message,
                                                 Callback callback)
    : message_(message), callback_(std::move(callback)) {}

CitizenNotesInfoBarDelegate::~CitizenNotesInfoBarDelegate() {
  if (!callback_.is_null())
    std::move(callback_).Run(false);
}

infobars::InfoBarDelegate::InfoBarIdentifier
CitizenNotesInfoBarDelegate::GetIdentifier() const {
  return CITIZEN_NOTES_INFOBAR_DELEGATE;
}

std::u16string CitizenNotesInfoBarDelegate::GetMessageText() const {
  return message_;
}

std::u16string CitizenNotesInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_CITIZEN_NOTES_CONFIRM_ALLOW_BUTTON
                                       : IDS_CITIZEN_NOTES_CONFIRM_DENY_BUTTON);
}

bool CitizenNotesInfoBarDelegate::Accept() {
  std::move(callback_).Run(true);
  return true;
}

bool CitizenNotesInfoBarDelegate::Cancel() {
  std::move(callback_).Run(false);
  return true;
}
