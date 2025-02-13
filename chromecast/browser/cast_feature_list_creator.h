// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_FEATURE_LIST_CREATOR_H_
#define CHROMECAST_BROWSER_CAST_FEATURE_LIST_CREATOR_H_

#include <memory>

class PrefService;

namespace chromecast {

// Creator for the singleton |FeatureList|. Stateful due to having to create and
// hold a |PrefService| instance until |CastBrowserProcess| takes ownership when
// the full browser process starts,
class CastFeatureListCreator {
 public:
  CastFeatureListCreator();
  virtual ~CastFeatureListCreator();

  // Creates the |PrefService| and uses it to initialize |FeatureList|. Retains
  // ownership of the |PrefService|.
  void CreatePrefServiceAndFeatureList();

  // Takes ownership of the |PrefService| previously created.
  std::unique_ptr<PrefService> TakePrefService();

 private:
  // Holds the |PrefService| until TakePrefService() is called and ownership
  // is taken away.
  std::unique_ptr<PrefService> pref_service_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_FEATURE_LIST_CREATOR_H_
