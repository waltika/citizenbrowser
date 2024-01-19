// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {assert} = chai;

import * as i18n from '../../../../../front_end/core/i18n/i18n.js';

describe('CitizenNotesLocale', () => {
  // For tests, we assume CitizenNotes supports all locales we throw at it.
  // Finding the closes supported locale is implemented in the i18n lib and tested as part of that lib.
  const identity = (locale: string) => locale;

  after(() => {
    // Reset the singleton after the test suite for other tests.
    const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
      settingLanguage: 'en-US',
      navigatorLanguage: '',
      lookupClosestCitizenNotesLocale: identity,
    };
    i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});
  });

  it('chooses navigator.language if setting is "browserLanguage"', () => {
    const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
      settingLanguage: 'browserLanguage',
      navigatorLanguage: 'en-GB',
      lookupClosestCitizenNotesLocale: identity,
    };
    const citizenNotesLocale = i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});

    assert.strictEqual(citizenNotesLocale.locale, 'en-GB');
  });

  it('chooses setting language if setting has any other value than "browserLanguage"', () => {
    const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
      settingLanguage: 'zh',
      navigatorLanguage: 'en-GB',
      lookupClosestCitizenNotesLocale: identity,
    };
    const citizenNotesLocale = i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});

    assert.strictEqual(citizenNotesLocale.locale, 'zh');
  });

  it('falls back to en-US should navigator.language be empty', () => {
    const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
      settingLanguage: 'browserLanguage',
      navigatorLanguage: '',
      lookupClosestCitizenNotesLocale: identity,
    };
    const citizenNotesLocale = i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});

    assert.strictEqual(citizenNotesLocale.locale, 'en-US');
  });

  it('chooses the closest supported language', () => {
    const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
      settingLanguage: 'zh-HK',
      navigatorLanguage: '',
      lookupClosestCitizenNotesLocale: () => 'zh',
    };
    const citizenNotesLocale = i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});

    assert.strictEqual(citizenNotesLocale.locale, 'zh');
  });

  describe('forceFallbackLocale', () => {
    it('sets locale to English', () => {
      const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
        settingLanguage: 'browserLanguage',
        navigatorLanguage: 'en-GB',
        lookupClosestCitizenNotesLocale: identity,
      };
      const citizenNotesLocale = i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});
      assert.strictEqual(citizenNotesLocale.locale, 'en-GB');

      citizenNotesLocale.forceFallbackLocale();
      assert.strictEqual(citizenNotesLocale.locale, 'en-US');
    });
  });

  describe('languageIsSupportedByCitizenNotes', () => {
    it('returns true if the locale is supported, false otherwise', () => {
      const data: i18n.CitizenNotesLocale.CitizenNotesLocaleData = {
        settingLanguage: 'zh-HK',
        navigatorLanguage: '',
        lookupClosestCitizenNotesLocale: () => 'zh',
      };
      const citizenNotesLocale = i18n.CitizenNotesLocale.CitizenNotesLocale.instance({create: true, data});

      assert.isTrue(citizenNotesLocale.languageIsSupportedByCitizenNotes('zh-HK'));
      assert.isFalse(citizenNotesLocale.languageIsSupportedByCitizenNotes('de-DE'));
    });
  });
});

describe('localeLanguagesMatch', () => {
  it('returns true if the language part of a locale matches, false otherwise', () => {
    assert.isTrue(i18n.CitizenNotesLocale.localeLanguagesMatch('de-DE', 'de-AT'));
    assert.isTrue(i18n.CitizenNotesLocale.localeLanguagesMatch('de-DE', 'de'));

    assert.isFalse(i18n.CitizenNotesLocale.localeLanguagesMatch('de', 'en'));
    assert.isFalse(i18n.CitizenNotesLocale.localeLanguagesMatch('de-AT', 'en-US'));
  });
});
