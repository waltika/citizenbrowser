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
  createTestLiveExpression: 'Create live expression',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  hideTestNetworkMessages: 'Hide network messages',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showTestNetworkMessages: 'Show network messages',
  /**
   *@description Alternative title text of a setting in Test View of the Test panel
   */
  selectedTestContextOnly: 'Selected context only',
  /**
   *@description Tooltip text that appears on the setting when hovering over it in Test View of the Test panel
   */
  onlyShowTestMessagesFromTheCurrent: 'Only show messages from the current context (`top`, `iframe`, `worker`, extension)',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showTestMessagesFromAllContexts: 'Show messages from all contexts',
  /**
   *@description Title of a setting under the Test category in Settings
   */
  logTestXmlhttprequests: 'Log XMLHttpRequests',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showTestTimestamps: 'Show timestamps',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  hideTestTimestamps: 'Hide timestamps',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  autocompleteTestFromHistory: 'Autocomplete from history',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotAutocompleteTestFromHistory: 'Do not autocomplete from history',
  /**
   * @description Title of a setting under the Test category that controls whether to accept autocompletion with Enter.
   */
  autocompleteTestOnEnter: 'Accept autocomplete suggestion on Enter',
  /**
   * @description Title of a setting under the Test category that controls whether to accept autocompletion with Enter.
   */
  doNotAutocompleteTestOnEnter: 'Do not accept autocomplete suggestion on Enter',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  groupSimilarTestMessagesIn: 'Group similar messages in test',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotGroupTestSimilarMessagesIn: 'Do not group similar messages in test',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  showCorsTestErrorsIn: 'Show `CORS` errors in test',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotShowCorsTestErrorsIn: 'Do not show `CORS` errors in test',
  /**
   *@description Title of a setting under the Test category in Settings
   */
  eagerTestEvaluation: 'Eager evaluation',
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
  evaluateTestTriggersUserActivation: 'Treat code evaluation as user action',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  treatTestEvaluationAsUserActivation: 'Treat evaluation as user activation',
  /**
   *@description Title of a setting under the Test category that can be invoked through the Command Menu
   */
  doNotTreatTestEvaluationAsUser: 'Do not treat evaluation as user activation',
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
  title: i18nLazyString(UIStrings.createTestLiveExpression),
  iconClass: UI.ActionRegistration.IconClass.EYE,
  async loadActionDelegate() {
    const Test = await loadTestModule();
    return new Test.TestView.ActionDelegate();
  },
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.hideTestNetworkMessages),
  settingName: 'hideTestNetworkMessages',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.hideTestNetworkMessages),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.showTestNetworkMessages),
    },
  ],
});



Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.selectedTestContextOnly),
  settingName: 'selectedTestContextFilterEnabled',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.onlyShowTestMessagesFromTheCurrent),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.showTestMessagesFromAllContexts),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.logTestXmlhttprequests),
  settingName: 'monitoringXTestHREnabled',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.showTestTimestamps),
  settingName: 'testTimestampsEnabled',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.showTestTimestamps),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.hideTestTimestamps),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  title: i18nLazyString(UIStrings.autocompleteTestFromHistory),
  settingName: 'testHistoryAutocomplete',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.autocompleteTestFromHistory),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotAutocompleteTestFromHistory),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.autocompleteTestOnEnter),
  settingName: 'testAutocompleteOnEnter',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.autocompleteTestOnEnter),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotAutocompleteTestOnEnter),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.groupSimilarTestMessagesIn),
  settingName: 'testGroupSimilar',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.groupSimilarTestMessagesIn),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotGroupTestSimilarMessagesIn),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  title: i18nLazyString(UIStrings.showCorsTestErrorsIn),
  settingName: 'testShowsCorsErrors',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.showCorsTestErrorsIn),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotShowCorsTestErrorsIn),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.TEST,
  storageType: Common.Settings.SettingStorageType.Synced,
  title: i18nLazyString(UIStrings.eagerTestEvaluation),
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
  title: i18nLazyString(UIStrings.evaluateTestTriggersUserActivation),
  settingName: 'testUserActivationEval',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.treatTestEvaluationAsUserActivation),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotTreatTestEvaluationAsUser),
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
