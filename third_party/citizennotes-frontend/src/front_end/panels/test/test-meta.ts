// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as UI from '../../ui/legacy/legacy.js';

import type * as Test from './test.js';

import * as i18n from '../../core/i18n/i18n.js';
const UIStrings = {
  /**
   *@description Title of the Test tool
   */
  test: 'Test',
  /**
   *@description Title of an action that shows the test.
   */
  showTest: 'Show Test',
  /**
   *@description Text to clear the test
   */
  clearTest: 'Clear test',
  /**
   *@description Title of an action in the test tool to clear
   */
  clearTestHistory: 'Clear test history',
  /**
   *@description Title of an action in the test tool to create pin. A live expression is code that the user can enter into the test and it will be pinned in the UI. Live expressions are constantly evaluated as the user interacts with the test (hence 'live').
   */
  createLiveExpression: 'Create live expression',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  hideNetworkMessages: 'Hide network messages',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showNetworkMessages: 'Show network messages',
  /**
   *@description Alternative title text of a setting in Test View of the Test panel
   */
  selectedContextOnly: 'Selected context only',
  /**
   *@description Tooltip text that appears on the setting when hovering over it in Test View of the Test panel
   */
  onlyShowMessagesFromTheCurrent: 'Only show messages from the current context (`top`, `iframe`, `worker`, extension)',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showMessagesFromAllContexts: 'Show messages from all contexts',
  /**
   *@description Title of a setting under the Test category in Settings
   */
  logXmlhttprequests: 'Log XMLHttpRequests',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showTimestamps: 'Show timestamps',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  hideTimestamps: 'Hide timestamps',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  autocompleteFromHistory: 'Autocomplete from history',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotAutocompleteFromHistory: 'Do not autocomplete from history',
  /**
   * @description Title of a setting under the Test category that controls whether to accept autocompletion with Enter.
   */
  autocompleteOnEnter: 'Accept autocomplete suggestion on Enter',
  /**
   * @description Title of a setting under the Test category that controls whether to accept autocompletion with Enter.
   */
  doNotAutocompleteOnEnter: 'Do not accept autocomplete suggestion on Enter',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  groupSimilarMessagesInTest: 'Group similar messages in test',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotGroupSimilarMessagesIn: 'Do not group similar messages in test',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showCorsErrorsInTest: 'Show `CORS` errors in test',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotShowCorsErrorsIn: 'Do not show `CORS` errors in test',
  /**
   *@description Title of a setting under the Test category in Settings
   */
  eagerEvaluation: 'Eager evaluation',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  eagerlyEvaluateTestPromptText: 'Eagerly evaluate test prompt text',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotEagerlyEvaluateTest: 'Do not eagerly evaluate test prompt text',
  /**
   *@description Allows code that is executed in the test to do things that usually are only allowed if triggered by a user action
   */
  evaluateTriggersUserActivation: 'Treat code evaluation as user action',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  treatEvaluationAsUserActivation: 'Treat evaluation as user activation',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotTreatEvaluationAsUser: 'Do not treat evaluation as user activation',
  /**
   * @description Title of a setting under the Test category in Settings that controls whether `test.trace()` messages appear expanded by default.
   */
  expandTestTraceMessagesByDefault: 'Automatically expand `test.trace()` messages',
  /**
   * @description Title of a setting under the Test category in Settings that controls whether `test.trace()` messages appear collapsed by default.
   */
  collapseTestTraceMessagesByDefault: 'Do not automatically expand `test.trace()` messages',
};
const str_ = i18n.i18n.registerUIStrings('panels/test/test-meta.ts', UIStrings);
const i18nLazyString = i18n.i18n.getLazilyComputedLocalizedString.bind(undefined, str_);
let loadedTestModule: (typeof Test|undefined);

async function loadTestModule(): Promise<typeof Test> {
  if (!loadedTestModule) {
    loadedTestModule = await import('./test.js');
  }
  return loadedTestModule;
}

function maybeRetrieveContextTypes<T = unknown>(getClassCallBack: (testModule: typeof Test) => T[]): T[] {
  if (loadedTestModule === undefined) {
    return [];
  }
  return getClassCallBack(loadedTestModule);
}

UI.ViewManager.registerViewExtension({
  location: UI.ViewManager.ViewLocationValues.PANEL,
  id: 'test',
  title: i18nLazyString(UIStrings.test),
  commandPrompt: i18nLazyString(UIStrings.showTest),
  order: 20,
  async loadView() {
    const Test = await loadTestModule();
    return Test.TestPanel.TestPanel.instance();
  },
});

UI.ViewManager.registerViewExtension({
  location: UI.ViewManager.ViewLocationValues.DRAWER_VIEW,
  id: 'test-view',
  title: i18nLazyString(UIStrings.test),
  commandPrompt: i18nLazyString(UIStrings.showTest),
  persistence: UI.ViewManager.ViewPersistence.PERMANENT,
  order: 0,
  async loadView() {
    const Test = await loadTestModule();
    return Test.TestPanel.WrapperView.instance();
  },
});

UI.ActionRegistration.registerActionExtension({
  actionId: 'test.toggle',
  category: UI.ActionRegistration.ActionCategory.TEST,
  title: i18nLazyString(UIStrings.showTest),
  async loadActionDelegate() {
    const Test = await loadTestModule();
    return new Test.TestView.ActionDelegate();
  },
  bindings: [
    {
      shortcut: 'Ctrl+`',
      keybindSets: [
        UI.ActionRegistration.KeybindSet.CITIZENNOTES_DEFAULT,
        UI.ActionRegistration.KeybindSet.VS_CODE,
      ],
    },
  ],
});

UI.ActionRegistration.registerActionExtension({
  actionId: 'test.clear',
  category: UI.ActionRegistration.ActionCategory.TEST,
  title: i18nLazyString(UIStrings.clearTest),
  iconClass: UI.ActionRegistration.IconClass.CLEAR,
  async loadActionDelegate() {
    const Test = await loadTestModule();
    return new Test.TestView.ActionDelegate();
  },
  contextTypes() {
    return maybeRetrieveContextTypes(Test => [Test.TestView.TestView]);
  },
  bindings: [
    {
      shortcut: 'Ctrl+L',
    },
    {
      shortcut: 'Meta+K',
      platform: UI.ActionRegistration.Platforms.Mac,
    },
  ],
});

UI.ActionRegistration.registerActionExtension({
  actionId: 'test.clear.history',
  category: UI.ActionRegistration.ActionCategory.TEST,
  title: i18nLazyString(UIStrings.clearTestHistory),
  async loadActionDelegate() {
    const Test  = await loadTestModule();
    return new Test.TestView.ActionDelegate();
  },
});

UI.ActionRegistration.registerActionExtension({
  actionId: 'test.create-pin',
  category: UI.ActionRegistration.ActionCategory.TEST,
  title: i18nLazyString(UIStrings.createLiveExpression),
  iconClass: UI.ActionRegistration.IconClass.EYE,
  async loadActionDelegate() {
    const Test = await loadTestModule();
    return new Test.TestView.ActionDelegate();
  },
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.hideNetworkMessages),
  settingName: 'hideNetworkMessages',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.hideNetworkMessages),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.showNetworkMessages),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.selectedContextOnly),
  settingName: 'selectedContextFilterEnabled',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.onlyShowMessagesFromTheCurrent),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.showMessagesFromAllContexts),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.logXmlhttprequests),
  settingName: 'monitoringXHREnabled',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.showTimestamps),
  settingName: 'testTimestampsEnabled',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.showTimestamps),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.hideTimestamps),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  title: i18nLazyString(UIStrings.autocompleteFromHistory),
  settingName: 'testHistoryAutocomplete',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.autocompleteFromHistory),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotAutocompleteFromHistory),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.autocompleteOnEnter),
  settingName: 'testAutocompleteOnEnter',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.autocompleteOnEnter),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotAutocompleteOnEnter),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.groupSimilarMessagesInTest),
  settingName: 'testGroupSimilar',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.groupSimilarMessagesInTest),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotGroupSimilarMessagesIn),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  title: i18nLazyString(UIStrings.showCorsErrorsInTest),
  settingName: 'testShowsCorsErrors',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.showCorsErrorsInTest),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotShowCorsErrorsIn),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.eagerEvaluation),
  settingName: 'testEagerEval',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.eagerlyEvaluateTestPromptText),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotEagerlyEvaluateTest),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.evaluateTriggersUserActivation),
  settingName: 'testUserActivationEval',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.treatEvaluationAsUserActivation),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotTreatEvaluationAsUser),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.expandTestTraceMessagesByDefault),
  settingName: 'testTraceExpand',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.expandTestTraceMessagesByDefault),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.collapseTestTraceMessagesByDefault),
    },
  ],
});

Common.Revealer.registerRevealer({
  contextTypes() {
    return [
      Common.Test.Test,
    ];
  },
  destination: undefined,
  async loadRevealer() {
    const Test = await loadTestModule();
    return new Test.TestPanel.TestRevealer();
  },
});
