// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_sync_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using sync_pb::UserEventSpecifics;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;
using WriteBatch = ModelTypeStore::WriteBatch;

MATCHER_P(MatchesUserEvent, expected, "") {
  if (!arg.has_user_event()) {
    *result_listener << "which is not a user event";
    return false;
  }
  const UserEventSpecifics& actual = arg.user_event();
  if (actual.event_time_usec() != expected.event_time_usec()) {
    return false;
  }
  if (actual.navigation_id() != expected.navigation_id()) {
    return false;
  }
  if (actual.session_id() != expected.session_id()) {
    return false;
  }
  return true;
}

UserEventSpecifics CreateSpecifics(int64_t event_time_usec,
                                   int64_t navigation_id,
                                   uint64_t session_id) {
  UserEventSpecifics specifics;
  specifics.set_event_time_usec(event_time_usec);
  specifics.set_navigation_id(navigation_id);
  specifics.set_session_id(session_id);
  return specifics;
}

std::unique_ptr<UserEventSpecifics> SpecificsUniquePtr(int64_t event_time_usec,
                                                       int64_t navigation_id,
                                                       uint64_t session_id) {
  return std::make_unique<UserEventSpecifics>(
      CreateSpecifics(event_time_usec, navigation_id, session_id));
}

class TestGlobalIdMapper : public GlobalIdMapper {
 public:
  void AddGlobalIdChangeObserver(GlobalIdChange callback) override {
    callback_ = std::move(callback);
  }

  int64_t GetLatestGlobalId(int64_t global_id) override {
    auto iter = id_map_.find(global_id);
    return iter == id_map_.end() ? global_id : iter->second;
  }

  void ChangeId(int64_t old_id, int64_t new_id) {
    id_map_[old_id] = new_id;
    callback_.Run(old_id, new_id);
  }

 private:
  GlobalIdChange callback_;
  std::map<int64_t, int64_t> id_map_;
};

class UserEventSyncBridgeTest : public testing::Test {
 protected:
  UserEventSyncBridgeTest() { ResetBridge(); }

  void ResetBridge() {
    OnceModelTypeStoreFactory store_factory;
    if (bridge_) {
      // Carry over the underlying store from previous bridge instances.
      std::unique_ptr<ModelTypeStore> store = bridge_->StealStoreForTest();
      bridge_.reset();
      store_factory =
          ModelTypeStoreTestUtil::MoveStoreToFactory(std::move(store));
    } else {
      store_factory = ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
    }
    bridge_ = std::make_unique<UserEventSyncBridge>(
        std::move(store_factory), mock_processor_.CreateForwardingProcessor(),
        &test_global_id_mapper_);
  }

  void WaitUntilModelReadyToSync(
      const std::string& account_id = "test_account_id") {
    base::RunLoop loop;
    base::RepeatingClosure quit_closure = loop.QuitClosure();
    // Let the bridge initialize fully, which should run ModelReadyToSync().
    ON_CALL(*processor(), ModelReadyToSync(_))
        .WillByDefault(InvokeWithoutArgs([=]() { quit_closure.Run(); }));
    loop.Run();
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return(account_id));
  }

  static std::string GetStorageKey(const UserEventSpecifics& specifics) {
    return UserEventSyncBridge::GetStorageKeyFromSpecificsForTest(specifics);
  }

  UserEventSyncBridge* bridge() { return bridge_.get(); }
  MockModelTypeChangeProcessor* processor() { return &mock_processor_; }
  TestGlobalIdMapper* mapper() { return &test_global_id_mapper_; }

  std::map<std::string, sync_pb::EntitySpecifics> GetAllData() {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetAllDataForDebugging(base::BindOnce(
        [](base::RunLoop* loop, std::unique_ptr<DataBatch>* out_batch,
           std::unique_ptr<DataBatch> batch) {
          *out_batch = std::move(batch);
          loop->Quit();
        },
        &loop, &batch));
    loop.Run();
    EXPECT_NE(nullptr, batch);

    std::map<std::string, sync_pb::EntitySpecifics> storage_key_to_specifics;
    if (batch != nullptr) {
      while (batch->HasNext()) {
        const syncer::KeyAndData& pair = batch->Next();
        storage_key_to_specifics[pair.first] = pair.second->specifics;
      }
    }
    return storage_key_to_specifics;
  }

  std::unique_ptr<sync_pb::EntitySpecifics> GetData(
      const std::string& storage_key) {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetData(
        {storage_key},
        base::BindOnce(
            [](base::RunLoop* loop, std::unique_ptr<DataBatch>* out_batch,
               std::unique_ptr<DataBatch> batch) {
              *out_batch = std::move(batch);
              loop->Quit();
            },
            &loop, &batch));
    loop.Run();
    EXPECT_NE(nullptr, batch);

    std::unique_ptr<sync_pb::EntitySpecifics> specifics;
    if (batch != nullptr && batch->HasNext()) {
      const syncer::KeyAndData& pair = batch->Next();
      specifics =
          std::make_unique<sync_pb::EntitySpecifics>(pair.second->specifics);
      EXPECT_FALSE(batch->HasNext());
    }
    return specifics;
  }

 private:
  base::MessageLoop message_loop_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  TestGlobalIdMapper test_global_id_mapper_;
  std::unique_ptr<UserEventSyncBridge> bridge_;
};

TEST_F(UserEventSyncBridgeTest, MetadataIsInitialized) {
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  WaitUntilModelReadyToSync();
}

TEST_F(UserEventSyncBridgeTest, SingleRecord) {
  WaitUntilModelReadyToSync();
  const UserEventSpecifics specifics(CreateSpecifics(1u, 2u, 3u));
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics));

  EXPECT_THAT(GetData(storage_key), Pointee(MatchesUserEvent(specifics)));
  EXPECT_THAT(GetData("bogus"), IsNull());
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(storage_key, MatchesUserEvent(specifics))));
}

TEST_F(UserEventSyncBridgeTest, ApplyStopSyncChanges) {
  WaitUntilModelReadyToSync();
  const UserEventSpecifics specifics(CreateSpecifics(1u, 2u, 3u));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics));
  ASSERT_THAT(GetAllData(), SizeIs(1));

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(UserEventSyncBridgeTest, ApplyStopSyncChangesShouldKeepConsents) {
  WaitUntilModelReadyToSync("account_id");
  UserEventSpecifics user_event_specifics(CreateSpecifics(2u, 2u, 2u));
  auto* consent = user_event_specifics.mutable_user_consent();
  consent->set_feature(UserEventSpecifics::UserConsent::CHROME_SYNC);
  consent->set_account_id("account_id");
  bridge()->RecordUserEvent(
      std::make_unique<UserEventSpecifics>(user_event_specifics));
  ASSERT_THAT(GetAllData(), SizeIs(1));

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  // User consent specific must be persisted when sync is disabled.
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(_, MatchesUserEvent(user_event_specifics))));
}

TEST_F(UserEventSyncBridgeTest, MultipleRecords) {
  WaitUntilModelReadyToSync();
  std::set<std::string> unique_storage_keys;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .Times(4)
      .WillRepeatedly(
          [&unique_storage_keys](const std::string& storage_key,
                                 std::unique_ptr<EntityData> entity_data,
                                 MetadataChangeList* metadata_change_list) {
            unique_storage_keys.insert(storage_key);
          });

  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 1u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 2u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 2u, 2u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(2u, 2u, 2u));

  EXPECT_EQ(2u, unique_storage_keys.size());
  EXPECT_THAT(GetAllData(), SizeIs(2));
}

TEST_F(UserEventSyncBridgeTest, ApplySyncChanges) {
  WaitUntilModelReadyToSync();
  std::string storage_key1;
  std::string storage_key2;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key1)))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key2)));

  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 1u, 1u));
  bridge()->RecordUserEvent(SpecificsUniquePtr(2u, 2u, 2u));
  EXPECT_THAT(GetAllData(), SizeIs(2));

  auto error_on_delete =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 {EntityChange::CreateDelete(storage_key1)});
  EXPECT_FALSE(error_on_delete);
  EXPECT_THAT(GetAllData(), SizeIs(1));
  EXPECT_THAT(GetData(storage_key1), IsNull());
  EXPECT_THAT(GetData(storage_key2), NotNull());
}

TEST_F(UserEventSyncBridgeTest, HandleGlobalIdChange) {
  WaitUntilModelReadyToSync();

  int64_t first_id = 11;
  int64_t second_id = 12;
  int64_t third_id = 13;
  int64_t fourth_id = 14;

  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));

  // This id update should be applied to the event as it is initially
  // recorded.
  mapper()->ChangeId(first_id, second_id);
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, first_id, 2u));
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(storage_key, MatchesUserEvent(CreateSpecifics(
                                                1u, second_id, 2u)))));

  // This id update is done while the event is "in flight", and should result in
  // it being updated and re-sent to sync.
  EXPECT_CALL(*processor(), Put(storage_key, _, _));
  mapper()->ChangeId(second_id, third_id);
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(storage_key, MatchesUserEvent(CreateSpecifics(
                                                1u, third_id, 2u)))));
  auto error_on_delete =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 {EntityChange::CreateDelete(storage_key)});
  EXPECT_FALSE(error_on_delete);
  EXPECT_THAT(GetAllData(), IsEmpty());

  // This id update should be ignored, since we received commit confirmation
  // above.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  mapper()->ChangeId(third_id, fourth_id);
  EXPECT_THAT(GetAllData(), IsEmpty());
}

TEST_F(UserEventSyncBridgeTest, MulipleEventsChanging) {
  WaitUntilModelReadyToSync();

  int64_t first_id = 11;
  int64_t second_id = 12;
  int64_t third_id = 13;
  int64_t fourth_id = 14;
  const UserEventSpecifics specifics1 = CreateSpecifics(101u, first_id, 2u);
  const UserEventSpecifics specifics2 = CreateSpecifics(102u, second_id, 4u);
  const UserEventSpecifics specifics3 = CreateSpecifics(103u, third_id, 6u);
  const std::string key1 = GetStorageKey(specifics1);
  const std::string key2 = GetStorageKey(specifics2);
  const std::string key3 = GetStorageKey(specifics3);
  ASSERT_NE(key1, key2);
  ASSERT_NE(key1, key3);
  ASSERT_NE(key2, key3);

  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics1));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics2));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics3));
  ASSERT_THAT(GetAllData(),
              UnorderedElementsAre(Pair(key1, MatchesUserEvent(specifics1)),
                                   Pair(key2, MatchesUserEvent(specifics2)),
                                   Pair(key3, MatchesUserEvent(specifics3))));

  mapper()->ChangeId(second_id, fourth_id);
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(key1, MatchesUserEvent(specifics1)),
          Pair(key2, MatchesUserEvent(CreateSpecifics(102u, fourth_id, 4u))),
          Pair(key3, MatchesUserEvent(specifics3))));

  mapper()->ChangeId(first_id, fourth_id);
  mapper()->ChangeId(third_id, fourth_id);
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(key1, MatchesUserEvent(CreateSpecifics(101u, fourth_id, 2u))),
          Pair(key2, MatchesUserEvent(CreateSpecifics(102u, fourth_id, 4u))),
          Pair(key3, MatchesUserEvent(CreateSpecifics(103u, fourth_id, 6u)))));
}

TEST_F(UserEventSyncBridgeTest, RecordBeforeMetadataLoads) {
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(false));
  ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return(""));
  bridge()->RecordUserEvent(SpecificsUniquePtr(1u, 2u, 3u));
  EXPECT_CALL(*processor(), ModelReadyToSync(_));
  WaitUntilModelReadyToSync("account_id");
  EXPECT_THAT(GetAllData(), IsEmpty());
}

// User consents should be buffered if the bridge is not fully initialized.
// Other events should get dropped.
TEST_F(UserEventSyncBridgeTest, RecordWithLateInitializedStore) {
  UserEventSpecifics consent1 = CreateSpecifics(1u, 1u, 1u);
  consent1.mutable_user_consent()->set_account_id("account_id");
  UserEventSpecifics consent2 = CreateSpecifics(2u, 2u, 2u);
  consent2.mutable_user_consent()->set_account_id("account_id");
  UserEventSpecifics specifics1 = CreateSpecifics(3u, 3u, 3u);
  UserEventSpecifics specifics2 = CreateSpecifics(4u, 4u, 4u);

  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(consent1));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics1));

  // Initialize the store.
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  WaitUntilModelReadyToSync("account_id");

  // Record events after metadata is ready.
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(consent2));
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics2));

  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(GetStorageKey(consent1), MatchesUserEvent(consent1)),
          Pair(GetStorageKey(consent2), MatchesUserEvent(consent2)),
          Pair(GetStorageKey(specifics2), MatchesUserEvent(specifics2))));
}

TEST_F(UserEventSyncBridgeTest,
       ShouldReportPreviouslyPersistedConsentsWhenSyncIsReenabled) {
  WaitUntilModelReadyToSync("account_id");

  UserEventSpecifics consent = CreateSpecifics(1u, 1u, 1u);
  consent.mutable_user_consent()->set_account_id("account_id");

  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(consent));

  // User disables sync, however, the consent hasn't been submitted yet. It is
  // preserved to be submitted when sync is re-enabled.
  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetAllData(), SizeIs(1));

  // Reenable sync.
  EXPECT_CALL(*processor(), Put(GetStorageKey(consent), _, _));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return("account_id"));
  bridge()->MergeSyncData(WriteBatch::CreateMetadataChangeList(),
                          EntityChangeList());

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserEventSyncBridgeTest,
       ShouldReportPersistedConsentsOnStartupWithSyncAlreadyEnabled) {
  // Persist a consent while sync is enabled.
  WaitUntilModelReadyToSync("account_id");
  UserEventSpecifics consent = CreateSpecifics(1u, 1u, 1u);
  consent.mutable_user_consent()->set_account_id("account_id");
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(consent));
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(GetAllData(), SizeIs(1));

  // Restart the bridge, mimic-ing a browser restart.
  EXPECT_CALL(*processor(), Put(GetStorageKey(consent), _, _));
  ResetBridge();

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserEventSyncBridgeTest, ShouldReportPersistedConsentsOnSyncEnabled) {
  // Persist a consent before sync is enabled.
  WaitUntilModelReadyToSync(/*account=id=*/"");
  UserEventSpecifics consent = CreateSpecifics(1u, 1u, 1u);
  consent.mutable_user_consent()->set_account_id("account_id");
  bridge()->RecordUserEvent(std::make_unique<UserEventSpecifics>(consent));
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(GetAllData(), SizeIs(1));

  // Restart the bridge, mimic-ing a browser restart. We expect no Put()
  // until sync is enabled.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  ResetBridge();
  WaitUntilModelReadyToSync(/*account_id=*/"");

  // Enable sync.
  EXPECT_CALL(*processor(), Put(GetStorageKey(consent), _, _));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return("account_id"));
  bridge()->MergeSyncData(WriteBatch::CreateMetadataChangeList(),
                          EntityChangeList());

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserEventSyncBridgeTest, ShouldSubmitPersistedConsentOnlyIfSameAccount) {
  WaitUntilModelReadyToSync("first_account");
  UserEventSpecifics user_event_specifics(CreateSpecifics(2u, 2u, 2u));
  auto* consent = user_event_specifics.mutable_user_consent();
  consent->set_account_id("first_account");
  bridge()->RecordUserEvent(
      std::make_unique<UserEventSpecifics>(user_event_specifics));
  ASSERT_THAT(GetAllData(), SizeIs(1));

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(_, MatchesUserEvent(user_event_specifics))));

  // A new user signs in and enables sync.
  // The previous account consent should not be resubmited, because the new sync
  // account is different.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  ON_CALL(*processor(), TrackedAccountId())
      .WillByDefault(Return("second_account"));
  bridge()->MergeSyncData(WriteBatch::CreateMetadataChangeList(),
                          EntityChangeList());
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  base::RunLoop().RunUntilIdle();

  // The previous user signs in again and enables sync.
  EXPECT_CALL(*processor(), Put(GetStorageKey(user_event_specifics), _, _));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  ON_CALL(*processor(), TrackedAccountId())
      .WillByDefault(Return("first_account"));
  bridge()->MergeSyncData(WriteBatch::CreateMetadataChangeList(),
                          EntityChangeList());

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace syncer
