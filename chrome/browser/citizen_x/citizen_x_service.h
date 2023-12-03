// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZEN_X_CITIZEN_X_SERVICE_H_
#define CHROME_BROWSER_CITIZEN_X_CITIZEN_X_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace content {
    class BrowserContext;
}

namespace citizen_x {

class CitizenXModel;

// KeyedService responsible for the cache of Citizen Hub actions.
class CitizenXService : public KeyedService {
 public:
  explicit CitizenXService(content::BrowserContext* context);

  CitizenXService(const CitizenXService&) = delete;
  CitizenXService& operator=(const CitizenXService&) = delete;

  ~CitizenXService() override;

  virtual CitizenXModel* GetCitizenXModel();

 private:
  std::unique_ptr<CitizenXModel> model_;
};

}  // namespace citizen_x

#endif  // CHROME_BROWSER_CITIZEN_X_CITIZEN_X_SERVICE_H_
