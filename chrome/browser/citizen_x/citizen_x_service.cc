// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizen_x_service.h"

#include "chrome/browser/citizen_x/citizen_x_model.h"
#include "content/public/browser/browser_context.h"

namespace citizen_x {

CitizenXService::CitizenXService(content::BrowserContext* context) {
  model_ = std::make_unique<CitizenXModel>(context);
}

CitizenXService::~CitizenXService() = default;

CitizenXModel* CitizenXService::GetCitizenXModel() {
  return model_.get();
}

}  // namespace citizen_x
