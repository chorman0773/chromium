// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_UNIFIED_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_UNIFIED_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace ash {

namespace tray {
class BluetoothDetailedView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Bluetooth detailed view in UnifiedSystemTray.
class UnifiedBluetoothDetailedViewController : public DetailedViewController,
                                               public BluetoothObserver {
 public:
  explicit UnifiedBluetoothDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedBluetoothDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;

  // BluetoothObserver:
  void OnBluetoothSystemStateChanged() override;
  void OnBluetoothScanStateChanged() override;

 private:
  void UpdateDeviceListAndUI();
  void UpdateBluetoothDeviceList();

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::BluetoothDetailedView* view_ = nullptr;

  BluetoothDeviceList connected_devices_;
  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;
  BluetoothDeviceList discovered_not_paired_devices_;

  // Timer used to limit the update frequency.
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedBluetoothDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_UNIFIED_BLUETOOTH_DETAILED_VIEW_CONTROLLER_H_
