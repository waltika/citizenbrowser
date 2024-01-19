// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let citizenNotesLocaleInstance: CitizenNotesLocale|null = null;

export interface CitizenNotesLocaleData {
  settingLanguage: string;
  navigatorLanguage: string;
  lookupClosestCitizenNotesLocale: (locale: string) => string;
}

export type CitizenNotesLocaleCreationOptions = {
  create: true,
  data: CitizenNotesLocaleData,
}|{
  create: false,
};

/**
 * Simple class that determines the CitizenNotes locale based on:
 *   1) navigator.language, which matches the Chrome UI
 *   2) the value of the "language" Setting the user choses
 *   3) available locales in CitizenNotes.
 *
 * The CitizenNotes locale is only determined once during startup and
 * guaranteed to never change. Use this class when using
 * `Intl` APIs.
 */
export class CitizenNotesLocale {
  readonly locale: string;
  readonly lookupClosestCitizenNotesLocale: (locale: string) => string;

  private constructor(data: CitizenNotesLocaleData) {
    this.lookupClosestCitizenNotesLocale = data.lookupClosestCitizenNotesLocale;

    // TODO(crbug.com/1163928): Use constant once setting actually exists.
    if (data.settingLanguage === 'browserLanguage') {
      this.locale = data.navigatorLanguage || 'en-US';
    } else {
      this.locale = data.settingLanguage;
    }

    this.locale = this.lookupClosestCitizenNotesLocale(this.locale);
  }

  static instance(opts: CitizenNotesLocaleCreationOptions = {create: false}): CitizenNotesLocale {
    if (!citizenNotesLocaleInstance && !opts.create) {
      throw new Error('No LanguageSelector instance exists yet.');
    }

    if (opts.create) {
      citizenNotesLocaleInstance = new CitizenNotesLocale(opts.data);
    }
    return citizenNotesLocaleInstance as CitizenNotesLocale;
  }

  static removeInstance(): void {
    citizenNotesLocaleInstance = null;
  }

  forceFallbackLocale(): void {
    // Locale is 'readonly', this is the only case where we want to forceably
    // overwrite the locale.
    (this.locale as CitizenNotesLocale['locale']) = 'en-US';
  }

  /**
   * Returns true iff CitizenNotes supports the language of the passed locale.
   * Note that it doesn't have to be a one-to-one match, e.g. if CitizenNotes supports
   * 'de', then passing 'de-AT' will return true.
   */
  languageIsSupportedByCitizenNotes(localeString: string): boolean {
    return localeLanguagesMatch(localeString, this.lookupClosestCitizenNotesLocale(localeString));
  }
}

/**
 * Returns true iff the two locales have matching languages. This means the
 * passing 'de-AT' and 'de-DE' will return true, while 'de-DE' and 'en' will
 * return false.
 */
export function localeLanguagesMatch(localeString1: string, localeString2: string): boolean {
  const locale1 = new Intl.Locale(localeString1);
  const locale2 = new Intl.Locale(localeString2);
  return locale1.language === locale2.language;
}
