// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_info_fetcher_win.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAccessTokenValue[] = "test_access_token_value";
constexpr char kRefreshTokenValue[] = "test_refresh_token_value";
constexpr char kInvalidTokenInfoResponse[] =
    "{"
    "  \"error\": \"invalid_token\""
    "}";
constexpr char kInvalidUserInfoResponse[] =
    "{"
    "  \"error\": \"invalid_token\""
    "}";
constexpr char kInvalidAccessTokenFetchResponse[] =
    "{"
    "  \"error\": \"invalid_token\""
    "}";
}  // namespace

// Provides base functionality for the AccessTokenFetcher Tests below.  The
// FakeURLFetcherFactory allows us to override the response data and payload for
// specified URLs.  We use this to stub out network calls made by the
// AccessTokenFetcher.  This fixture also creates an IO MessageLoop, if
// necessary, for use by the AccessTokenFetcher.
class CredentialProviderFetcherTest : public ::testing::Test {
 protected:
  CredentialProviderFetcherTest();
  ~CredentialProviderFetcherTest() override;

  void OnFetchComplete(base::OnceClosure done_closure,
                       base::Value fetch_result);

  void SetFakeResponses(const std::string& access_token_fetch_data,
                        net::HttpStatusCode access_token_fetch_code,
                        int access_token_net_error,
                        const std::string& user_info_data,
                        net::HttpStatusCode user_info_code,
                        int user_info_net_error,
                        const std::string& token_info_data,
                        net::HttpStatusCode token_info_code,
                        int token_info_net_error);

  scoped_refptr<network::SharedURLLoaderFactory> shared_factory() {
    return shared_factory_;
  }

  void RunFetcher();

  // Used for result verification
  base::Value fetch_result_;
  CredentialProviderSigninDialogTestDataStorage test_data_storage_;

  std::string valid_token_info_response_;
  std::string valid_user_info_response_;
  std::string valid_access_token_fetch_response_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderFetcherTest);
};

CredentialProviderFetcherTest::CredentialProviderFetcherTest()
    : shared_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {
  valid_token_info_response_ =
      "{"
      "  \"audience\": \"blah.apps.googleusercontent.blah.com\","
      "  \"used_id\": \"1234567890\","
      "  \"scope\": \"all the things\","
      "  \"expires_in\": 1800,"
      "  \"token_type\": \"Bearer\","
      "  \"token_handle\": \"" +
      test_data_storage_.GetSuccessTokenHandle() +
      "\""
      "}";
  valid_user_info_response_ =
      "{"
      "  \"name\": \"" +
      test_data_storage_.GetSuccessFullName() +
      "\""
      "}";
  valid_access_token_fetch_response_ =
      "{"
      "  \"access_token\": \"123456789\","
      "  \"id_token\": \"" +
      test_data_storage_.GetSuccessMdmIdToken() +
      "\","
      "  \"expires_in\": 1800"
      "}";
}

CredentialProviderFetcherTest::~CredentialProviderFetcherTest() = default;

void CredentialProviderFetcherTest::OnFetchComplete(
    base::OnceClosure done_closure,
    base::Value fetch_result) {
  EXPECT_TRUE(fetch_result.is_dict());
  fetch_result_ = std::move(fetch_result);

  std::move(done_closure).Run();
}

void CredentialProviderFetcherTest::SetFakeResponses(
    const std::string& access_token_fetch_data,
    net::HttpStatusCode access_token_fetch_code,
    int access_token_net_error,
    const std::string& user_info_data,
    net::HttpStatusCode user_info_code,
    int user_info_net_error,
    const std::string& token_info_data,
    net::HttpStatusCode token_info_code,
    int token_info_net_error) {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_info_url(),
      network::CreateResourceResponseHead(token_info_code), token_info_data,
      network::URLLoaderCompletionStatus(token_info_net_error));

  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url(),
      network::CreateResourceResponseHead(user_info_code), user_info_data,
      network::URLLoaderCompletionStatus(user_info_net_error));

  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(),
      network::CreateResourceResponseHead(access_token_fetch_code),
      access_token_fetch_data,
      network::URLLoaderCompletionStatus(access_token_net_error));
}

void CredentialProviderFetcherTest::RunFetcher() {
  base::RunLoop run_loop;
  auto fetcher_callback =
      base::BindOnce(&CredentialProviderFetcherTest::OnFetchComplete,
                     base::Unretained(this), run_loop.QuitClosure());

  CredentialProviderSigninInfoFetcher fetcher(kRefreshTokenValue,
                                              shared_factory());
  fetcher.SetCompletionCallbackAndStart(kAccessTokenValue,
                                        std::move(fetcher_callback));
  run_loop.Run();
}

TEST_F(CredentialProviderFetcherTest, ValidFetchResult) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher();
  EXPECT_FALSE(fetch_result_.DictEmpty());
  EXPECT_TRUE(test_data_storage_.EqualsSccessfulFetchResult(fetch_result_));
}

TEST_F(CredentialProviderFetcherTest,
       ValidFetchResultWithNetworkErrorOnTokenFetch) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_BAD_REQUEST,
                   net::ERR_FAILED, valid_user_info_response_, net::HTTP_OK,
                   net::OK, valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher();
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest,
       ValidFetchResultWithNetworkErrorOnUserInfoFetch) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_BAD_REQUEST,
                   net::ERR_FAILED, valid_token_info_response_, net::HTTP_OK,
                   net::OK);

  RunFetcher();
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidAccessTokenFetch) {
  SetFakeResponses(kInvalidAccessTokenFetchResponse, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher();
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidUserInfoFetch) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   kInvalidUserInfoResponse, net::HTTP_OK, net::OK,
                   valid_token_info_response_, net::HTTP_OK, net::OK);

  RunFetcher();
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidTokenInfoFetch) {
  SetFakeResponses(valid_access_token_fetch_response_, net::HTTP_OK, net::OK,
                   valid_user_info_response_, net::HTTP_OK, net::OK,
                   kInvalidTokenInfoResponse, net::HTTP_OK, net::OK);

  RunFetcher();
  EXPECT_TRUE(fetch_result_.DictEmpty());
}

TEST_F(CredentialProviderFetcherTest, InvalidFetchResult) {
  SetFakeResponses(kInvalidAccessTokenFetchResponse, net::HTTP_OK, net::OK,
                   kInvalidUserInfoResponse, net::HTTP_OK, net::OK,
                   kInvalidTokenInfoResponse, net::HTTP_OK, net::OK);

  RunFetcher();
  EXPECT_TRUE(fetch_result_.DictEmpty());
}
