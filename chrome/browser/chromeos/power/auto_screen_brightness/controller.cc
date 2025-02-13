// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/controller.h"

#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/adapter.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader_impl.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor_impl.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/gaussian_trainer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

Controller::Controller() {
  als_reader_ = std::make_unique<AlsReaderImpl>();
  als_reader_->Init();

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  DCHECK(power_manager_client);
  brightness_monitor_ =
      std::make_unique<BrightnessMonitorImpl>(power_manager_client);

  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  DCHECK(user_activity_detector);

  const Profile* const profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  modeller_ = std::make_unique<ModellerImpl>(
      profile, als_reader_.get(), brightness_monitor_.get(),
      user_activity_detector, std::make_unique<GaussianTrainer>());

  adapter_ = std::make_unique<Adapter>(profile, als_reader_.get(),
                                       brightness_monitor_.get(),
                                       modeller_.get(), power_manager_client);
}

Controller::~Controller() = default;

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
