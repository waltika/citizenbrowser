// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizennotes_settings.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

const char CitizenNotesSettings::kSyncCitizenNotesPreferencesFrontendName[] =
    "sync_preferences";
const bool CitizenNotesSettings::kSyncCitizenNotesPreferencesDefault = false;

CitizenNotesSettings::CitizenNotesSettings(Profile* profile) : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCitizenNotesSyncPreferences,
      base::BindRepeating(&CitizenNotesSettings::CitizenNotesSyncPreferencesChanged,
                          base::Unretained(this)));
}

CitizenNotesSettings::~CitizenNotesSettings() = default;

void CitizenNotesSettings::Register(const std::string& name,
                                const CNRegisterOptions& options) {
  // kSyncCitizenNotesPreferenceFrontendName is not stored in any of the relevant
  // dictionaries. Skip registration.
  if (name == kSyncCitizenNotesPreferencesFrontendName)
    return;

  if (options.sync_mode == CNRegisterOptions::SyncMode::kSync) {
    synced_setting_names_.insert(name);
  }

  // Setting might have had a different sync status in the past. Move the
  // setting to the correct dictionary.
  PrefService* prefs = profile_->GetPrefs();
  const char* dictionary_to_remove_from =
      options.sync_mode == CNRegisterOptions::SyncMode::kSync
          ? prefs::kCitizenNotesPreferences
          : GetDictionaryNameForSyncedPrefs();
  const std::string* settings_value =
      prefs->GetDict(dictionary_to_remove_from).FindString(name);
  if (!settings_value) {
    return;
  }

  const char* dictionary_to_insert_into =
      GetDictionaryNameForSettingsName(name);
  // Settings already moved to the synced dictionary on a different device have
  // precedence.
  const std::string* already_synced_value =
      prefs->GetDict(dictionary_to_insert_into).FindString(name);
  if (dictionary_to_insert_into == prefs::kCitizenNotesPreferences ||
      !already_synced_value) {
    ScopedDictPrefUpdate insert_update(profile_->GetPrefs(),
                                       dictionary_to_insert_into);
    insert_update->Set(name, *settings_value);
  }

  ScopedDictPrefUpdate remove_update(profile_->GetPrefs(),
                                     dictionary_to_remove_from);
  remove_update->Remove(name);
}

base::Value::Dict CitizenNotesSettings::Get() {
  base::Value::Dict settings;

  PrefService* prefs = profile_->GetPrefs();
  // CitizenNotes expects any kind of preference to be a string. Parsing is
  // happening on the frontend.
  settings.Set(
      kSyncCitizenNotesPreferencesFrontendName,
      prefs->GetBoolean(prefs::kCitizenNotesSyncPreferences) ? "true" : "false");
  settings.Merge(prefs->GetDict(prefs::kCitizenNotesPreferences).Clone());
  settings.Merge(prefs->GetDict(GetDictionaryNameForSyncedPrefs()).Clone());

  return settings;
}

absl::optional<base::Value> CitizenNotesSettings::Get(const std::string& name) {
  PrefService* prefs = profile_->GetPrefs();
  if (name == kSyncCitizenNotesPreferencesFrontendName) {
    // CitizenNotes expects any kind of preference to be a string. Parsing is
    // happening on the frontend.
    bool result = prefs->GetBoolean(prefs::kCitizenNotesSyncPreferences);
    return base::Value(result ? "true" : "false");
  }
  const char* dict_name = GetDictionaryNameForSettingsName(name);
  const base::Value::Dict& dict = prefs->GetDict(dict_name);
  const base::Value* value = dict.Find(name);
  return value ? absl::optional<base::Value>(value->Clone()) : absl::nullopt;
}

void CitizenNotesSettings::Set(const std::string& name, const std::string& value) {
  if (name == kSyncCitizenNotesPreferencesFrontendName) {
    profile_->GetPrefs()->SetBoolean(prefs::kCitizenNotesSyncPreferences,
                                     value == "true");
    return;
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              GetDictionaryNameForSettingsName(name));
  update->Set(name, value);
}

void CitizenNotesSettings::Remove(const std::string& name) {
  if (name == kSyncCitizenNotesPreferencesFrontendName) {
    profile_->GetPrefs()->SetBoolean(prefs::kCitizenNotesSyncPreferences,
                                     kSyncCitizenNotesPreferencesDefault);
    return;
  }

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              GetDictionaryNameForSettingsName(name));
  update->Remove(name);
}

void CitizenNotesSettings::Clear() {
  profile_->GetPrefs()->SetBoolean(prefs::kCitizenNotesSyncPreferences,
                                   kSyncCitizenNotesPreferencesDefault);
  profile_->GetPrefs()->SetDict(prefs::kCitizenNotesPreferences,
                                base::Value::Dict());
  profile_->GetPrefs()->SetDict(prefs::kCitizenNotesSyncedPreferencesSyncEnabled,
                                base::Value::Dict());
  profile_->GetPrefs()->SetDict(prefs::kCitizenNotesSyncedPreferencesSyncDisabled,
                                base::Value::Dict());
}

const char* CitizenNotesSettings::GetDictionaryNameForSettingsName(
    const std::string& name) const {
  return synced_setting_names_.contains(name)
             ? GetDictionaryNameForSyncedPrefs()
             : prefs::kCitizenNotesPreferences;
}

const char* CitizenNotesSettings::GetDictionaryNameForSyncedPrefs() const {
  const bool isCitizenNotesSyncEnabled =
      profile_->GetPrefs()->GetBoolean(prefs::kCitizenNotesSyncPreferences);
  return isCitizenNotesSyncEnabled ? prefs::kCitizenNotesSyncedPreferencesSyncEnabled
                               : prefs::kCitizenNotesSyncedPreferencesSyncDisabled;
}

void CitizenNotesSettings::CitizenNotesSyncPreferencesChanged() {
  // There are two cases to handle:
  //
  // Sync was enabled: We assume this was triggered by the user in the local
  // CitizenNotes session as opposed to synced from a different device. As such, the
  // local settings have precedence when merging. The unsynced dictonary is
  // cleared.
  //
  // Sync was disabled: As kCitizenNotesSyncPreferences is synced itself we can
  // clear the synced dictionary after copying to the unsynced one. The unsynced
  // dictionary is empty.
  //
  // Considering the points above, the implementation between both cases can be
  // shared modulo the source/target dictionary.
  const bool sync_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kCitizenNotesSyncPreferences);
  const char* target_dictionary =
      sync_enabled ? prefs::kCitizenNotesSyncedPreferencesSyncEnabled
                   : prefs::kCitizenNotesSyncedPreferencesSyncDisabled;
  const char* source_dictionary =
      sync_enabled ? prefs::kCitizenNotesSyncedPreferencesSyncDisabled
                   : prefs::kCitizenNotesSyncedPreferencesSyncEnabled;

  ScopedDictPrefUpdate source_update(profile_->GetPrefs(), source_dictionary);
  ScopedDictPrefUpdate target_update(profile_->GetPrefs(), target_dictionary);
  base::Value::Dict source_dict;
  std::swap(source_dict, *source_update);
  target_update->Merge(std::move(source_dict));
}
