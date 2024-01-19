// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../../core/i18n/i18n.js';

import targetCrashedScreenStyles from './targetCrashedScreen.css.legacy.js';
import {VBox} from './Widget.js';

const UIStrings = {
  /**
   *@description Text in dialog box when the target page crashed
   */
  citizennotesWasDisconnectedFromThe: 'CitizenNotes was disconnected from the page.',
  /**
   *@description Text content of content element
   */
  oncePageIsReloadedCitizennotesWill: 'Once page is reloaded, CitizenNotes will automatically reconnect.',
};
const str_ = i18n.i18n.registerUIStrings('ui/legacy/TargetCrashedScreen.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
export class TargetCrashedScreen extends VBox {
  private readonly hideCallback: () => void;
  constructor(hideCallback: () => void) {
    super(true);
    this.registerRequiredCSS(targetCrashedScreenStyles);
    this.contentElement.createChild('div', 'message').textContent =
        i18nString(UIStrings.citizennotesWasDisconnectedFromThe);
    this.contentElement.createChild('div', 'message').textContent =
        i18nString(UIStrings.oncePageIsReloadedCitizennotesWill);
    this.hideCallback = hideCallback;
  }

  override willHide(): void {
    this.hideCallback.call(null);
  }
}
