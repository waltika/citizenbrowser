// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Host from '../../../core/host/host.js';
import * as i18n from '../../../core/i18n/i18n.js';
import * as Platform from '../../../core/platform/platform.js';
import * as ComponentHelpers from '../../components/helpers/helpers.js';
import * as LitHtml from '../../lit-html/lit-html.js';
import * as Buttons from '../buttons/buttons.js';

const UIStrings = {
  /**
   * @description The title of the button that leads to the feedback form.
   */
  feedback: 'Feedback',
};

const str_ = i18n.i18n.registerUIStrings('ui/components/panel_feedback/FeedbackButton.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

const feedbackIconUrl = new URL('../../../Images/review.svg', import.meta.url).toString();

export interface FeedbackButtonData {
  feedbackUrl: Platform.CitizenNotesPath.UrlString;
}
export class FeedbackButton extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-feedback-button`;
  readonly #shadow = this.attachShadow({mode: 'open'});
  readonly #boundRender = this.#render.bind(this);

  #props: FeedbackButtonData = {
    feedbackUrl: Platform.CitizenNotesPath.EmptyUrlString,
  };

  set data(data: FeedbackButtonData) {
    this.#props = data;
    void ComponentHelpers.ScheduledRender.scheduleRender(this, this.#boundRender);
  }

  #onFeedbackClick(): void {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.openInNewTab(this.#props.feedbackUrl);
  }

  #render(): void {
    if (!ComponentHelpers.ScheduledRender.isScheduledRender(this)) {
      throw new Error('FeedbackButton render was not scheduled');
    }

    // clang-format off
    LitHtml.render(LitHtml.html`
      <${Buttons.Button.Button.litTagName}
          @click=${this.#onFeedbackClick}
          .iconUrl=${feedbackIconUrl}
          .variant=${Buttons.Button.Variant.SECONDARY}
          .jslogContext=${'feedback'}
      >${i18nString(UIStrings.feedback)}</${Buttons.Button.Button.litTagName}>
      `, this.#shadow, {host: this});
    // clang-format on
  }
}

ComponentHelpers.CustomElements.defineComponent('citizennotes-feedback-button', FeedbackButton);

declare global {
  interface HTMLElementTagNameMap {
    'citizennotes-feedback-button': FeedbackButton;
  }
}
