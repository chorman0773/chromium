// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_service.h"

#include <utility>

#include "base/bind_helpers.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "components/sync_sessions/session_sync_bridge.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace sync_sessions {

SessionSyncService::SessionSyncService(
    version_info::Channel channel,
    std::unique_ptr<SyncSessionsClient> sessions_client)
    : sessions_client_(std::move(sessions_client)) {
  DCHECK(sessions_client_);
  if (base::FeatureList::IsEnabled(switches::kSyncUSSSessions)) {
    sessions_sync_manager_ = std::make_unique<sync_sessions::SessionSyncBridge>(
        base::BindRepeating(&SessionSyncService::NotifyForeignSessionUpdated,
                            base::Unretained(this)),
        sessions_client_.get(),
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::SESSIONS,
            base::BindRepeating(&syncer::ReportUnrecoverableError, channel)));
  } else {
    sessions_sync_manager_ =
        std::make_unique<sync_sessions::SessionsSyncManager>(
            base::BindRepeating(
                &SessionSyncService::NotifyForeignSessionUpdated,
                base::Unretained(this)),
            sessions_client_.get());
  }
}

SessionSyncService::~SessionSyncService() {}

syncer::GlobalIdMapper* SessionSyncService::GetGlobalIdMapper() const {
  return sessions_sync_manager_->GetGlobalIdMapper();
}

OpenTabsUIDelegate* SessionSyncService::GetOpenTabsUIDelegate() {
  if (!proxy_tabs_running_) {
    return nullptr;
  }
  return sessions_sync_manager_->GetOpenTabsUIDelegate();
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
SessionSyncService::SubscribeToForeignSessionsChanged(
    const base::RepeatingClosure& cb) {
  return foreign_sessions_changed_callback_list_.Add(cb);
}

void SessionSyncService::ScheduleGarbageCollection() {
  sessions_sync_manager_->ScheduleGarbageCollection();
}

syncer::SyncableService* SessionSyncService::GetSyncableService() {
  return sessions_sync_manager_->GetSyncableService();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
SessionSyncService::GetControllerDelegate() {
  return sessions_sync_manager_->GetModelTypeSyncBridge()
      ->change_processor()
      ->GetControllerDelegate();
}

FaviconCache* SessionSyncService::GetFaviconCache() {
  return sessions_sync_manager_->GetFaviconCache();
}

void SessionSyncService::ProxyTabsStateChanged(
    syncer::DataTypeController::State state) {
  const bool was_proxy_tabs_running = proxy_tabs_running_;
  proxy_tabs_running_ = (state == syncer::DataTypeController::RUNNING);
  if (proxy_tabs_running_ != was_proxy_tabs_running) {
    NotifyForeignSessionUpdated();
  }
}

void SessionSyncService::SetSyncSessionsGUID(const std::string& guid) {
  sessions_client_->GetSessionSyncPrefs()->SetSyncSessionsGUID(guid);
}

void SessionSyncService::NotifyForeignSessionUpdated() {
  foreign_sessions_changed_callback_list_.Notify();
}

}  // namespace sync_sessions
