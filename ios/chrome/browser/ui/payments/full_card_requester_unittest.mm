// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/full_card_requester.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#import "base/test/ios/wait_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/payments/payment_request_unittest_base.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/crw_test_js_injection_receiver.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FakeResultDelegate
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  FakeResultDelegate() : weak_ptr_factory_(this) {}
  ~FakeResultDelegate() override {}

  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& /* full_card_request */,
      const autofill::CreditCard& card,
      const base::string16& cvc) override {}

  void OnFullCardRequestFailed() override {}

  base::WeakPtr<FakeResultDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeResultDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeResultDelegate);
};

class PaymentRequestFullCardRequesterTest : public PaymentRequestUnitTestBase,
                                            public PlatformTest {
 protected:
  PaymentRequestFullCardRequesterTest() {}

  // PlatformTest:
  void SetUp() override {
    DoSetUp();

    AddCreditCard(autofill::test::GetCreditCard());  // Visa.

    // Set up what is needed to have an instance of autofill::AutofillManager.
    CRWTestJSInjectionReceiver* injectionReceiver =
        [[CRWTestJSInjectionReceiver alloc] init];
    web_state()->SetJSInjectionReceiver(injectionReceiver);

    AutofillAgent* autofill_agent =
        [[AutofillAgent alloc] initWithPrefService:browser_state()->GetPrefs()
                                          webState:web_state()];
    InfoBarManagerImpl::CreateForWebState(web_state());
    web_state()->CreateWebFramesManager();
    auto main_frame = std::make_unique<web::FakeWebFrame>("main", true, GURL());
    web_state()->AddWebFrame(std::move(main_frame));
    autofill_controller_ =
        [[AutofillController alloc] initWithBrowserState:browser_state()
                                                webState:web_state()
                                           autofillAgent:autofill_agent
                               passwordGenerationManager:nullptr
                                         downloadEnabled:NO];
  }

  // PlatformTest:
  void TearDown() override {
    [autofill_controller_ detachFromWebState];

    DoTearDown();
  }

  // Manages autofill for a single page.
  AutofillController* autofill_controller_;
};

// Tests that the FullCardRequester presents and dismisses the card unmask
// prompt view controller, when the full card is requested and when the user
// enters the CVC/expiration information respectively.
TEST_F(PaymentRequestFullCardRequesterTest, PresentAndDismiss) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  FullCardRequester full_card_requester(base_view_controller, browser_state());

  EXPECT_EQ(nil, base_view_controller.presentedViewController);
  web::WebFrame* main_frame = web::GetMainWebFrame(web_state());
  autofill::AutofillManager* autofill_manager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state(),
                                                           main_frame)
          ->autofill_manager();
  FakeResultDelegate* fake_result_delegate = new FakeResultDelegate;
  full_card_requester.GetFullCard(*credit_cards()[0], autofill_manager,
                                  fake_result_delegate->GetWeakPtr());

  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[CardUnmaskPromptViewController class]]);

  full_card_requester.OnUnmaskVerificationResult(
      autofill::AutofillClient::SUCCESS);

  // Wait until the view controller is ordered to be dismissed and the animation
  // completes.
  base::test::ios::WaitUntilCondition(
      ^bool {
        return !base_view_controller.presentedViewController;
      },
      true, base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}
