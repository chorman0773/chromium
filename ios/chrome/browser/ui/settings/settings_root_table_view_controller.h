// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/material_components/app_bar_view_controller_presenting.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// SettingsRootTableViewController is a base class for integrating UITableViews
// into the Settings UI.  It handles the configuration and display of the MDC
// AppBar.
@interface SettingsRootTableViewController
    : ChromeTableViewController<AppBarViewControllerPresenting,
                                SettingsRootViewControlling,
                                TableViewLinkHeaderFooterItemDelegate>

// Whether this table view controller should hide the "Done" button (the right
// navigation bar button). Default is NO.
@property(nonatomic, assign) BOOL shouldHideDoneButton;

// Updates the edit or done button to reflect editing state.  If the
// tableView is not in edit mode (and thus showing the 'Done' button) it is
// using shouldHideDoneButton to know if it should display the edit button.
- (void)updateEditButton;

@end

// Subclasses of SettingsRootTableViewController should implement the
// following methods to customize the behavior.
@interface SettingsRootTableViewController (Subclassing)

// Returns NO.  Subclasses should overload this if an edit button is required.
- (BOOL)shouldShowEditButton;

// Returns NO.  Subclasses should overload this if the edit button should be
// enabled.
- (BOOL)editButtonEnabled;

// Notifies the view controller that the edit button has been tapped. If you
// override this method, you must call -[super editButtonPressed] at some point
// in your implementation.
//
// Note that this method calls -[self setEditing:] in order to change the
// editing mode of this controller.
- (void)editButtonPressed;

// Called when this ViewController toolbar's delete item has been tapped.
// |indexPaths| is the index paths of the currently selected item to be deleted.
// Default implementation removes the items.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_
