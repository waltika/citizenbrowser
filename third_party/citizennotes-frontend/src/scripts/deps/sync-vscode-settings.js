// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const path = require('path');
const fs = require('fs');

// JSON files under .vscode/ synced by this script
const VSCODE_SETTINGS_TO_MERGE = [
  {settingsFile: 'settings.json'}, {settingsFile: 'tasks.json', mergeField: 'tasks', byField: 'label'},
  {settingsFile: 'launch.json', mergeField: 'configurations', byField: 'name'}
];

// If the user has opted out of updates, return and do nothing.
if (Boolean(process.env['SKIP_VSCODE_SETTINGS_SYNC'])) {
  return;
}

for (const {settingsFile, mergeField, byField} of VSCODE_SETTINGS_TO_MERGE) {
  const vscodeSettingsLocation = path.join(process.cwd(), '.vscode', settingsFile);
  const citizennotesSettingsLocation = path.join(process.cwd(), '.vscode', 'citizennotes-workspace-' + settingsFile);

  // If there are no settings to copy and paste, skip.
  if (!fs.existsSync(citizennotesSettingsLocation)) {
    continue;
  }

  try {
    const citizennotesSettings = require(citizennotesSettingsLocation);
    let preExistingSettings = {};
    if (fs.existsSync(vscodeSettingsLocation)) {
      preExistingSettings = require(vscodeSettingsLocation);
    }

    const updatedSettings = {
      ...citizennotesSettings,
      // Any setting specified by the engineer will always take precedence over the defaults
      ...preExistingSettings,
    };

    if (mergeField && byField && preExistingSettings[mergeField] && citizennotesSettings[mergeField]) {
      // We need to merge two lists in a field. The list items have a unique
      // id specified by byField. If an entry is found in citizennotesSettings we
      // assume it should be updated when the citizennotes settings file is updated,
      // otherwise we keep the preexisting setting.
      const mergedList = [...citizennotesSettings[mergeField]];
      const doNotDuplicate = new Set(mergedList.map(item => item[byField]));
      for (const item of preExistingSettings[mergeField]) {
        if (!doNotDuplicate.has(item[byField])) {
          mergedList.push(item);
        }
      }
      updatedSettings[mergeField] = mergedList;
    }
    fs.writeFileSync(vscodeSettingsLocation, JSON.stringify(updatedSettings, null, 2));
  } catch (err) {
    console.warn('Unable to update VSCode settings - skipping');
    console.warn(err.stack);
  }
}
