// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

namespace {

constexpr char kQueryBase[] = "http://beasts.org/search";
constexpr char kSomeParam[] = "&some_param=some_value";
constexpr char kDogQuery[] = "dog";
constexpr char kSharkQuery[] = "shark";
constexpr char kDogCardId[] =
    "https://www.google.com/search?q=dog&sourceid=chrome&ie=UTF-8";
constexpr char kSharkCardId[] =
    "https://www.google.com/search?q=shark&sourceid=chrome&ie=UTF-8";

GURL GetAnswerCardUrl(const std::string& query) {
  return GURL(
      base::StringPrintf("%s?q=%s%s", kQueryBase, query.c_str(), kSomeParam));
}

std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  return std::make_unique<TemplateURLService>(nullptr, 0);
}

}  // namespace

class AnswerCardSearchProviderTest : public AppListTestBase {
 public:
  AnswerCardSearchProviderTest() : field_trial_list_(nullptr) {}

  FakeAppListModelUpdater* GetModelUpdater() const {
    return model_updater_.get();
  }

  const SearchProvider::Results& results() { return provider()->results(); }

  AnswerCardSearchProvider* provider() const { return provider_.get(); }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    model_updater_->SetSearchEngineIsGoogle(true);

    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();

    // Set up card server URL.
    std::map<std::string, std::string> params;
    params["ServerUrl"] = kQueryBase;
    params["QuerySuffix"] = kSomeParam;
    base::AssociateFieldTrialParams("TestTrial", "TestGroup", params);
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial("TestTrial", "TestGroup");
    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        app_list_features::kEnableAnswerCard.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&CreateTemplateURLService));
    provider_ = std::make_unique<AnswerCardSearchProvider>(
        profile_.get(), model_updater_.get(), nullptr);
  }

 private:
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<AnswerCardSearchProvider> provider_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProviderTest);
};

// Basic usage. |Start()| immediately populates an appropriate search result to
// be used by the client. A subsequent |Start()| call replaces the previous
// result.
TEST_F(AnswerCardSearchProviderTest, Start) {
  provider()->Start(base::UTF8ToUTF16(kDogQuery));
  ASSERT_EQ(1u, results().size());
  EXPECT_EQ(kDogCardId, results()[0]->id());
  EXPECT_EQ(GetAnswerCardUrl(kDogQuery), results()[0]->query_url()->spec());

  provider()->Start(base::UTF8ToUTF16(kSharkQuery));
  ASSERT_EQ(1u, results().size());
  EXPECT_EQ(kSharkCardId, results()[0]->id());
  EXPECT_EQ(GetAnswerCardUrl(kSharkQuery), results()[0]->query_url()->spec());
}

// Queries to non-Google search engines are ignored.
TEST_F(AnswerCardSearchProviderTest, NotGoogle) {
  GetModelUpdater()->SetSearchEngineIsGoogle(false);
  provider()->Start(base::UTF8ToUTF16(kDogQuery));
  EXPECT_EQ(0u, results().size());
}

// Escaping a query with a special character produces a well-formed query URL.
TEST_F(AnswerCardSearchProviderTest, QueryEscaping) {
  provider()->Start(base::UTF8ToUTF16("cat&dog"));
  ASSERT_EQ(1u, results().size());
  EXPECT_EQ(GetAnswerCardUrl("cat%26dog"), results()[0]->query_url()->spec());
}

}  // namespace test
}  // namespace app_list
