// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"

#include <set>
#include <string>

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_detailed_view_delegate.h"
#include "base/stl_util.h"

using device::mojom::BluetoothSystem;

namespace ash {

const int kUpdateFrequencyMs = 1000;

namespace {

// Updates bluetooth device |device| in the |list|. If it is new, append to the
// end of the |list|; otherwise, keep it at the same place, but update the data
// with new device info provided by |device|.
void UpdateBluetoothDeviceListHelper(BluetoothDeviceList* list,
                                     const BluetoothDeviceInfo& device) {
  for (BluetoothDeviceList::iterator it = list->begin(); it != list->end();
       ++it) {
    if ((*it).address == device.address) {
      *it = device;
      return;
    }
  }

  list->push_back(device);
}

// Removes the obsolete BluetoothDevices from |list|, if they are not in the
// |new_device_address_list|.
void RemoveObsoleteBluetoothDevicesFromList(
    BluetoothDeviceList* device_list,
    const std::set<std::string>& new_device_address_list) {
  base::EraseIf(*device_list, [&new_device_address_list](
                                  const BluetoothDeviceInfo& info) {
    return !base::ContainsKey(new_device_address_list, info.address);
  });
}

}  // namespace

UnifiedBluetoothDetailedViewController::UnifiedBluetoothDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<UnifiedDetailedViewDelegate>(tray_controller)) {
  Shell::Get()->system_tray_notifier()->AddBluetoothObserver(this);
}

UnifiedBluetoothDetailedViewController::
    ~UnifiedBluetoothDetailedViewController() {
  Shell::Get()->system_tray_notifier()->RemoveBluetoothObserver(this);
  // Stop discovering bluetooth devices when exiting BT detailed view.
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper && helper->HasBluetoothDiscoverySession()) {
    helper->StopBluetoothDiscovering();
  }
}

views::View* UnifiedBluetoothDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::BluetoothDetailedView(
      detailed_view_delegate_.get(),
      Shell::Get()->session_controller()->login_status());
  OnBluetoothSystemStateChanged();
  return view_;
}

void UnifiedBluetoothDetailedViewController::OnBluetoothSystemStateChanged() {
  auto* helper = Shell::Get()->tray_bluetooth_helper();
  const BluetoothSystem::State bluetooth_state = helper->GetBluetoothState();

  if (bluetooth_state == BluetoothSystem::State::kPoweredOn) {
    // If Bluetooth was just turned on, start discovering.
    Shell::Get()->tray_bluetooth_helper()->StartBluetoothDiscovering();
  } else {
    // Otherwise stop updating the list of devices.
    timer_.Stop();
  }

  UpdateDeviceListAndUI();
}

void UnifiedBluetoothDetailedViewController::OnBluetoothScanStateChanged() {
  // To avoid delaying showing devices, update the device list and UI
  // immediately.
  UpdateDeviceListAndUI();

  if (Shell::Get()->tray_bluetooth_helper()->HasBluetoothDiscoverySession()) {
    // Update the device list and UI every |kUpdateFrequencyMs|.
    timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kUpdateFrequencyMs), this,
        &UnifiedBluetoothDetailedViewController::UpdateDeviceListAndUI);
    return;
  }
  timer_.Stop();
}

void UnifiedBluetoothDetailedViewController::UpdateDeviceListAndUI() {
  UpdateBluetoothDeviceList();

  auto* helper = Shell::Get()->tray_bluetooth_helper();
  bool bluetooth_on =
      helper->GetBluetoothState() == BluetoothSystem::State::kPoweredOn;

  // Update toggle.
  view_->SetToggleIsOn(bluetooth_on);

  // Update loading indicator.
  if (helper->HasBluetoothDiscoverySession())
    view_->ShowLoadingIndicator();
  else
    view_->HideLoadingIndicator();

  // Update scroll list or show "BT disabled" panel
  if (bluetooth_on) {
    view_->HideBluetoothDisabledPanel();
    view_->UpdateDeviceScrollList(connected_devices_, connecting_devices_,
                                  paired_not_connected_devices_,
                                  discovered_not_paired_devices_);

    return;
  }

  // If Bluetooth is disabled, show a panel which only indicates that it is
  // disabled, instead of the scroller with Bluetooth devices.
  view_->ShowBluetoothDisabledPanel();
}

void UnifiedBluetoothDetailedViewController::UpdateBluetoothDeviceList() {
  std::set<std::string> new_connecting_devices;
  std::set<std::string> new_connected_devices;
  std::set<std::string> new_paired_not_connected_devices;
  std::set<std::string> new_discovered_not_paired_devices;

  BluetoothDeviceList list =
      Shell::Get()->tray_bluetooth_helper()->GetAvailableBluetoothDevices();
  for (const auto& device : list) {
    if (device.connecting) {
      new_connecting_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&connecting_devices_, device);
    } else if (device.connected && device.paired) {
      new_connected_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&connected_devices_, device);
    } else if (device.paired) {
      new_paired_not_connected_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&paired_not_connected_devices_, device);
    } else {
      new_discovered_not_paired_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&discovered_not_paired_devices_, device);
    }
  }
  RemoveObsoleteBluetoothDevicesFromList(&connecting_devices_,
                                         new_connecting_devices);
  RemoveObsoleteBluetoothDevicesFromList(&connected_devices_,
                                         new_connected_devices);
  RemoveObsoleteBluetoothDevicesFromList(&paired_not_connected_devices_,
                                         new_paired_not_connected_devices);
  RemoveObsoleteBluetoothDevicesFromList(&discovered_not_paired_devices_,
                                         new_discovered_not_paired_devices);
}

}  // namespace ash
