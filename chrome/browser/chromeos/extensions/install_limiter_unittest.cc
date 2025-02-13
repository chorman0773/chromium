// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/install_limiter.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::InstallLimiter;

namespace {

constexpr char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";
constexpr int kLargeExtensionSize = 2000000;
constexpr int kSmallExtensionSize = 200000;

}  // namespace

class InstallLimiterTest
    : public testing::TestWithParam<chromeos::DemoSession::DemoModeConfig> {
 public:
  InstallLimiterTest() = default;
  ~InstallLimiterTest() override = default;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::ScopedStubInstallAttributes test_install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(InstallLimiterTest);
};

TEST_P(InstallLimiterTest, ShouldDeferInstall) {
  const std::vector<std::string> screensaver_ids = {
      extension_misc::kScreensaverAppId, extension_misc::kScreensaverAlt1AppId,
      extension_misc::kScreensaverAlt2AppId};

  chromeos::DemoModeTestHelper demo_mode_test_helper;
  if (GetParam() != chromeos::DemoSession::DemoModeConfig::kNone)
    demo_mode_test_helper.InitializeSession(GetParam());

  // In demo mode (either online or offline), all apps larger than 1MB except
  // for the screensaver should be deferred.
  for (const std::string& id : screensaver_ids) {
    bool expected_defer_install =
        GetParam() == chromeos::DemoSession::DemoModeConfig::kNone ||
        id != chromeos::DemoSession::GetScreensaverAppId();
    EXPECT_EQ(expected_defer_install,
              InstallLimiter::ShouldDeferInstall(kLargeExtensionSize, id));
  }
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));
}

INSTANTIATE_TEST_CASE_P(
    DemoModeConfig,
    InstallLimiterTest,
    ::testing::Values(chromeos::DemoSession::DemoModeConfig::kNone,
                      chromeos::DemoSession::DemoModeConfig::kOnline,
                      chromeos::DemoSession::DemoModeConfig::kOffline));
