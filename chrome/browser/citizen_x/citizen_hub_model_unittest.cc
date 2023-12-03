// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_hub/citizen_hub_model.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class CitizenHubModelTest : public ::testing::Test {
 public:
  CitizenHubModelTest() = default;
  ~CitizenHubModelTest() override = default;

  citizen_hub::CitizenHubModel* model() { return &model_; }
  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return test_web_contents_.get(); }

  void NavigateTo(const GURL& url) {
    content::WebContentsTester::For(test_web_contents_.get())
        ->NavigateAndCommit(url);
  }

  std::vector<citizen_hub::SharingHubAction> GetFirstPartyActions() {
    return model_.GetFirstPartyActionList(test_web_contents_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

  citizen_hub::CitizenHubModel model_{&profile_};
};

// TODO(https://crbug.com/1326249): This unit test won't work while
// GetFirstPartyActions() depends on the WebContents being part of a valid
// browser. We need to break that dependency before this test can be enabled.
TEST_F(CitizenHubModelTest, DISABLED_FirstPartyOptionsOfferedOnAllURLs) {
  NavigateTo(GURL("https://www.chromium.org"));
  EXPECT_GT(GetFirstPartyActions().size(), 0u);
  NavigateTo(GURL("chrome://version"));
  EXPECT_GT(GetFirstPartyActions().size(), 0u);
}
