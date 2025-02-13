// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_feature_list_creator.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/browser/metrics/cast_metrics_prefs.h"
#include "chromecast/browser/pref_service_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

namespace chromecast {

CastFeatureListCreator::CastFeatureListCreator() {}

CastFeatureListCreator::~CastFeatureListCreator() {}

void CastFeatureListCreator::CreatePrefServiceAndFeatureList() {
  DCHECK(!pref_service_);

  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
  metrics::RegisterPrefs(pref_registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(pref_registry.get());
  pref_service_ =
      shell::PrefServiceHelper::CreatePrefService(pref_registry.get());

  const auto* features_dict =
      pref_service_->GetDictionary(prefs::kLatestDCSFeatures);
  const auto* experiment_ids =
      pref_service_->GetList(prefs::kActiveDCSExperiments);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  InitializeFeatureList(
      *features_dict, *experiment_ids,
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));
}

std::unique_ptr<PrefService> CastFeatureListCreator::TakePrefService() {
  return std::move(pref_service_);
}

}  // namespace chromecast
