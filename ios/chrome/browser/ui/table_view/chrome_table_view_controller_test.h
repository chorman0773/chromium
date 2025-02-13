// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_TEST_H_

#import <UIKit/UIKit.h>

#include "base/compiler_specific.h"
#import "base/ios/block_types.h"
#import "ios/chrome/test/block_cleanup_test.h"

@class ChromeTableViewController;

class ChromeTableViewControllerTest : public BlockCleanupTest {
 public:
  ChromeTableViewControllerTest();
  ~ChromeTableViewControllerTest() override;

 protected:
  void TearDown() override;

  // Derived classes allocate their controller here.
  virtual ChromeTableViewController* InstantiateController() = 0;

  // Tests should call this function to create their controller for testing.
  void CreateController();

  // Will call CreateController() if |controller_| is nil.
  ChromeTableViewController* controller();

  // Deletes the controller.
  void ResetController();

  // Tests that the controller's view, model, tableView, and delegate are
  // valid. Also tests that the model is the tableView's data source.
  void CheckController();

  // Returns the number of sections in the tableView.
  int NumberOfSections();

  // Returns the number of items in |section|.
  int NumberOfItemsInSection(int section);

  // Returns the collection view item at |item| in |section|.
  id GetTableViewItem(int section, int item);

  // Verifies that the title matches |expected_title|.
  void CheckTitle(NSString* expected_title);

  // Verifies that the title matches the l10n string for |expected_title_id|.
  void CheckTitleWithId(int expected_title_id);

  // Verifies that the section footer at |section| matches the |expected_text|.
  void CheckSectionFooter(NSString* expected_text, int section);

  // Verifies that the section footer at |section| matches the l10n string for
  // |expected_text_id|.
  void CheckSectionFooterWithId(int expected_text_id, int section);

  // Verifies that the text cell at |item| in |section| has a text property
  // which matches |expected_text|.
  void CheckTextCellText(NSString* expected_text, int section, int item);

  // Verifies that the text cell at |item| in |section| has a text property
  // which matches the l10n string for |expected_text_id|.
  void CheckTextCellTextWithId(int expected_text_id, int section, int item);

  // Verifies that the text cell at |item| in |section| has a text and
  // detailText properties which match strings for |expected_text| and
  // |expected_detail_text|, respectively.
  void CheckTextCellTextAndDetailText(NSString* expected_text,
                                      NSString* expected_detail_text,
                                      int section,
                                      int item);

  // Verifies that the text cell at |item| in |section| has a text and
  // detailText properties which match strings for |expected_text| and
  // |expected_detail_text|, respectively.
  void CheckDetailItemTextWithIds(int expected_text_id,
                                  int expected_detail_text_id,
                                  int section_id,
                                  int item_id);

  // Verifies that the switch cell at |item| in |section| has a text property
  // which matches |expected_title| and a isOn method which matches
  // |expected_state|.
  void CheckSwitchCellStateAndText(BOOL expected_state,
                                   NSString* expected_title,
                                   int section,
                                   int item);

  // Verifies that the switch cell at |item| in |section| has a text property
  // which matches the l10n string for |expected_title_id| and a isOn method
  // which matches  |expected_state|.
  void CheckSwitchCellStateAndTextWithId(BOOL expected_state,
                                         int expected_title_id,
                                         int section,
                                         int item);

  // Verifies that the cell at |item| in |section| has the given
  // |accessory_type|.
  void CheckAccessoryType(UITableViewCellAccessoryType accessory_type,
                          int section,
                          int item);

  // For |section|, deletes the item at |item|. |completion_block| is called at
  // the end of the call to -performBatchUpdates:completion:.
  void DeleteItem(int section, int item, ProceduralBlock completion_block);

 private:
  ChromeTableViewController* controller_;
};

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_TEST_H_
