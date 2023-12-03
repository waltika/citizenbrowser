// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZEN_X_CITIZEN_X_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CITIZEN_X_CITIZEN_X_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace citizen_x {

class CitizenXService;

class CitizenXServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CitizenXService* GetForProfile(Profile* profile);
  static CitizenXServiceFactory* GetInstance();

  CitizenXServiceFactory(const CitizenXServiceFactory&) = delete;
  CitizenXServiceFactory& operator=(const CitizenXServiceFactory&) = delete;

 private:
  friend base::NoDestructor<CitizenXServiceFactory>;

  CitizenXServiceFactory();
  ~CitizenXServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace citizen_x

#endif  // CHROME_BROWSER_CITIZEN_X_CITIZEN_X_SERVICE_FACTORY_H_
