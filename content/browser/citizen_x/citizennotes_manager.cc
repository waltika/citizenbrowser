// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/citizennotes_manager.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

// static
CitizenNotesManager* CitizenNotesManager::GetInstance() {
  return base::Singleton<CitizenNotesManager>::get();
}

CitizenNotesManager::CitizenNotesManager()
    : delegate_(
          GetContentClient()->browser()->CreateCitizenNotesManagerDelegate()) {}

void CitizenNotesManager::ShutdownForTests() {
  base::Singleton<CitizenNotesManager>::OnExit(nullptr);
}

CitizenNotesManager::~CitizenNotesManager() = default;

}  // namespace content
