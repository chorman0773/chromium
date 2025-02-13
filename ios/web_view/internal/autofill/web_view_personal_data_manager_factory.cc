// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

namespace ios_web_view {

// static
autofill::PersonalDataManager*
WebViewPersonalDataManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<autofill::PersonalDataManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewPersonalDataManagerFactory*
WebViewPersonalDataManagerFactory::GetInstance() {
  return base::Singleton<WebViewPersonalDataManagerFactory>::get();
}

WebViewPersonalDataManagerFactory::WebViewPersonalDataManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PersonalDataManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewWebDataServiceWrapperFactory::GetInstance());
}

WebViewPersonalDataManagerFactory::~WebViewPersonalDataManagerFactory() {}

std::unique_ptr<KeyedService>
WebViewPersonalDataManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  std::unique_ptr<autofill::PersonalDataManager> service(
      new autofill::PersonalDataManager(
          ApplicationContext::GetInstance()->GetApplicationLocale()));
  auto profile_db =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  auto account_db =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  service->Init(
      profile_db, account_db, browser_state->GetPrefs(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      /*client_profile_validator=*/nullptr, /*history_service=*/nullptr,
      browser_state->IsOffTheRecord());
  return service;
}

}  // namespace ios_web_view
