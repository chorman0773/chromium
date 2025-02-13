// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"

#include <memory>

#include "components/signin/core/browser/account_info.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#include "services/identity/public/cpp/identity_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
class IdentityManagerObserver;
}  // namespace

@interface BookmarkPromoController ()<SigninPromoViewConsumer> {
  bool _isIncognito;
  ios::ChromeBrowserState* _browserState;
  std::unique_ptr<IdentityManagerObserver> _identityManagerObserver;
}

// Mediator to use for the sign-in promo view displayed in the bookmark view.
@property(nonatomic, readwrite, strong)
    SigninPromoViewMediator* signinPromoViewMediator;

// IdentityManagerObserver Callbacks

// Called when a user signs into Google services such as sync.
- (void)onPrimaryAccountSetWithAccountId:(const std::string&)account_id
                                username:(const std::string&)username;

// Called when the currently signed-in user for a user has been signed out.
- (void)onPrimaryAccountClearedWithAccountId:(const std::string&)account_id
                                    username:(const std::string&)username;

@end

namespace {
class IdentityManagerObserver : public identity::IdentityManager::Observer {
 public:
  IdentityManagerObserver(BookmarkPromoController* controller)
      : controller_(controller) {}

  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override {
    [controller_
        onPrimaryAccountSetWithAccountId:primary_account_info.account_id
                                username:primary_account_info.email];
  }

  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override {
    [controller_
        onPrimaryAccountClearedWithAccountId:previous_primary_account_info
                                                 .account_id
                                    username:previous_primary_account_info
                                                 .email];
  }

 private:
  __weak BookmarkPromoController* controller_;
};
}  // namespace

@implementation BookmarkPromoController

@synthesize delegate = _delegate;
@synthesize shouldShowSigninPromo = _shouldShowSigninPromo;
@synthesize signinPromoViewMediator = _signinPromoViewMediator;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:
                                (id<BookmarkPromoControllerDelegate>)delegate
                           presenter:(id<SigninPresenter>)presenter {
  self = [super init];
  if (self) {
    _delegate = delegate;
    // Incognito browserState can go away before this class is released, this
    // code avoids keeping a pointer to it.
    _isIncognito = browserState->IsOffTheRecord();
    if (!_isIncognito) {
      _browserState = browserState;
      _identityManagerObserver.reset(new IdentityManagerObserver(self));
      identity::IdentityManager* identityManager =
          IdentityManagerFactory::GetForBrowserState(_browserState);
      identityManager->AddObserver(_identityManagerObserver.get());
      _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
          initWithBrowserState:_browserState
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_BOOKMARK_MANAGER
                     presenter:presenter];
      _signinPromoViewMediator.consumer = self;
    }
    [self updateShouldShowSigninPromo];
  }
  return self;
}

- (void)dealloc {
  [_signinPromoViewMediator signinPromoViewRemoved];
  if (!_isIncognito) {
    DCHECK(_browserState);
    identity::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    identityManager->RemoveObserver(_identityManagerObserver.get());
  }
}

- (void)hidePromoCell {
  DCHECK(!_isIncognito);
  DCHECK(_browserState);
  self.shouldShowSigninPromo = NO;
}

- (void)setShouldShowSigninPromo:(BOOL)shouldShowSigninPromo {
  if (_shouldShowSigninPromo != shouldShowSigninPromo) {
    _shouldShowSigninPromo = shouldShowSigninPromo;
    [self.delegate promoStateChanged:shouldShowSigninPromo];
  }
}

- (void)updateShouldShowSigninPromo {
  self.shouldShowSigninPromo = NO;
  if (_isIncognito)
    return;

  DCHECK(_browserState);
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                         browserState:_browserState]) {
    identity::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    self.shouldShowSigninPromo = !identityManager->HasPrimaryAccount();
  }
}

#pragma mark - IdentityManagerObserver

// Called when a user signs into Google services such as sync.
- (void)onPrimaryAccountSetWithAccountId:(const std::string&)account_id
                                username:(const std::string&)username {
  if (!self.signinPromoViewMediator.isSigninInProgress)
    self.shouldShowSigninPromo = NO;
}

// Called when the currently signed-in user for a user has been signed out.
- (void)onPrimaryAccountClearedWithAccountId:(const std::string&)account_id
                                    username:(const std::string&)username {
  [self updateShouldShowSigninPromo];
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  [self.delegate configureSigninPromoWithConfigurator:configurator
                                      identityChanged:identityChanged];
}

- (void)signinDidFinish {
  [self updateShouldShowSigninPromo];
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self updateShouldShowSigninPromo];
}

@end
