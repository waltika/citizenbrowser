// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizen_x_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/citizen_x/citizen_x_service.h"

namespace citizen_x {

// static
CitizenXService* CitizenXServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CitizenXService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CitizenXServiceFactory* CitizenXServiceFactory::GetInstance() {
  static base::NoDestructor<CitizenXServiceFactory> instance;
  return instance.get();
}

CitizenXServiceFactory::CitizenXServiceFactory()
    : ProfileKeyedServiceFactory(
          "CitizenXService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

CitizenXServiceFactory::~CitizenXServiceFactory() = default;

std::unique_ptr<KeyedService>
CitizenXServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<CitizenXService>(context);
}

}  // namespace citizen_x
