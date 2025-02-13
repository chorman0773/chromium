// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

class Profile;
class UpgradeDetector;

// AppMenuIconController encapsulates the logic for badging the app menu icon
// as a result of various events - such as available updates, errors, etc.
class AppMenuIconController :
    public content::NotificationObserver,
    public UpgradeObserver {
 public:
  enum class IconType : uint32_t {
    NONE,
    UPGRADE_NOTIFICATION,
    GLOBAL_ERROR,
  };
  enum class Severity : uint32_t {
    NONE,
    LOW,
    MEDIUM,
    HIGH,
  };

  // The app menu icon's type and severity.
  struct TypeAndSeverity {
    IconType type;
    Severity severity;
  };

  // Delegate interface for receiving icon update notifications.
  class Delegate {
   public:
    // Notifies the UI to update the icon to have the specified
    // |type_and_severity|.
    virtual void UpdateTypeAndSeverity(TypeAndSeverity type_and_severity) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates an instance of this class for the given |profile| that will notify
  // |delegate| of updates.
  AppMenuIconController(Profile* profile, Delegate* delegate);
  AppMenuIconController(UpgradeDetector* upgrade_detector,
                        Profile* profile,
                        Delegate* delegate);
  ~AppMenuIconController() override;

  // Forces an update of the UI based on the current state of the world. This
  // will check whether there are any current pending updates, global errors,
  // etc. and based on that information trigger an appropriate call to the
  // delegate.
  void UpdateDelegate();

  // Returns the icon type and severity based on the current state.
  TypeAndSeverity GetTypeAndSeverity() const;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // True for desktop Chrome on dev and canary channels.
  const bool is_unstable_channel_;
  UpgradeDetector* const upgrade_detector_;
  Profile* const profile_;
  Delegate* const delegate_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuIconController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_
