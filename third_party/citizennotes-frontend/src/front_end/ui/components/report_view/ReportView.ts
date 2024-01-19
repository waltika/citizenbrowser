// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ComponentHelpers from '../../components/helpers/helpers.js';
import * as LitHtml from '../../lit-html/lit-html.js';

import reportStyles from './report.css.js';
import reportKeyStyles from './reportKey.css.js';
import reportSectionStyles from './reportSection.css.js';
import reportSectionDividerStyles from './reportSectionDivider.css.js';
import reportSectionHeaderStyles from './reportSectionHeader.css.js';
import reportValueStyles from './reportValue.css.js';

/**
 * The `Report` component can be used to display static information. A report
 * usually consists of multiple sections where each section has rows of name/value
 * pairs. The exact structure of a report is determined by the user, as is the
 * rendering and content of the individual name/value pairs.
 *
 * Example:
 * ```
 *   <citizennotes-report .data=${{reportTitle: 'Optional Title'} as Components.ReportView.ReportData}>
 *     <citizennotes-report-section-header>Some Header</citizennotes-report-section-header>
 *     <citizennotes-report-key>Key (rendered in the left column)</citizennotes-report-key>
 *     <citizennotes-report-value>Value (rendered in the right column)</citizennotes-report-value>
 *     <citizennotes-report-key class="foo">Name (with custom styling)</citizennotes-report-key>
 *     <citizennotes-report-value>Some Value</citizennotes-report-value>
 *     <citizennotes-report-divider></citizennotes-report-divider>
 *   </citizennotes-report>
 * ```
 * The component is intended to replace UI.ReportView in an idiomatic way.
 */
export interface ReportData {
  reportTitle: string;
}
export class Report extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-report`;

  readonly #shadow = this.attachShadow({mode: 'open'});
  #reportTitle: string = '';

  set data({reportTitle}: ReportData) {
    this.#reportTitle = reportTitle;
    this.#render();
  }

  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [reportStyles];
    this.#render();
  }

  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    LitHtml.render(LitHtml.html`
      <div class="content">
        ${this.#reportTitle ? LitHtml.html`<div class="report-title">${this.#reportTitle}</div>` : LitHtml.nothing}
        <slot></slot>
      </div>
    `, this.#shadow, {host: this});
    // clang-format on
  }
}

export interface ReportSectionData {
  sectionTitle: string;
}

export class ReportSection extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-report-section`;
  readonly #shadow = this.attachShadow({mode: 'open'});
  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [reportSectionStyles];
    this.#render();
  }
  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    LitHtml.render(LitHtml.html`
      <div class="section">
        <slot></slot>
      </div>
    `, this.#shadow, {host: this});
    // clang-format on
  }
}

export class ReportSectionHeader extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-report-section-header`;

  readonly #shadow = this.attachShadow({mode: 'open'});
  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [reportSectionHeaderStyles];
    this.#render();
  }

  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    LitHtml.render(LitHtml.html`
      <div class="section-header">
        <slot></slot>
      </div>
    `, this.#shadow, {host: this});
    // clang-format on
  }
}

export class ReportSectionDivider extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-report-divider`;

  readonly #shadow = this.attachShadow({mode: 'open'});
  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [reportSectionDividerStyles];
    this.#render();
  }

  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    LitHtml.render(LitHtml.html`
      <div class="section-divider">
      </div>
    `, this.#shadow, {host: this});
    // clang-format on
  }
}

export class ReportKey extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-report-key`;

  readonly #shadow = this.attachShadow({mode: 'open'});
  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [reportKeyStyles];
    this.#render();
  }

  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    LitHtml.render(LitHtml.html`
      <div class="key"><slot></slot></div>
    `, this.#shadow, {host: this});
    // clang-format on
  }
}

export class ReportValue extends HTMLElement {
  static readonly litTagName = LitHtml.literal`citizennotes-report-value`;

  readonly #shadow = this.attachShadow({mode: 'open'});
  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [reportValueStyles];
    this.#render();
  }

  #render(): void {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    LitHtml.render(LitHtml.html`
      <div class="value"><slot></slot></div>
    `, this.#shadow, {host: this});
    // clang-format on
  }
}

ComponentHelpers.CustomElements.defineComponent('citizennotes-report', Report);
ComponentHelpers.CustomElements.defineComponent('citizennotes-report-section', ReportSection);
ComponentHelpers.CustomElements.defineComponent('citizennotes-report-section-header', ReportSectionHeader);
ComponentHelpers.CustomElements.defineComponent('citizennotes-report-key', ReportKey);
ComponentHelpers.CustomElements.defineComponent('citizennotes-report-value', ReportValue);
ComponentHelpers.CustomElements.defineComponent('citizennotes-report-divider', ReportSectionDivider);

declare global {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  interface HTMLElementTagNameMap {
    'citizennotes-report': Report;
    'citizennotes-report-section': ReportSection;
    'citizennotes-report-section-header': ReportSectionHeader;
    'citizennotes-report-key': ReportKey;
    'citizennotes-report-value': ReportValue;
    'citizennotes-report-divider': ReportSectionDivider;
  }
}
