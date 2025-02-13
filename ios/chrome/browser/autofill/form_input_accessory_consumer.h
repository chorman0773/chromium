// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_

#import <Foundation/Foundation.h>

@class FormSuggestion;
@protocol FormInputAccessoryViewDelegate;
@protocol FormSuggestionClient;

@protocol FormInputAccessoryConsumer<NSObject>

// Removes the animations on the custom keyboard view.
- (void)removeAnimationsOnKeyboardView;

// Removes the presented keyboard view and the input accessory view. Also clears
// the references to them, so nothing shows until a new custom view is passed.
- (void)restoreOriginalKeyboardView;

// Removes the presented keyboard view and the input accessory view until
// |continueCustomKeyboardView| is called.
- (void)pauseCustomKeyboardView;

// Adds the previously presented views to the keyboard. If they have not been
// reset.
- (void)continueCustomKeyboardView;

// Replace the keyboard accessory view with one showing the passed suggestions.
// And form navigation buttons if not an iPad (which already includes those).
- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions
                suggestionClient:(id<FormSuggestionClient>)suggestionClient
              navigationDelegate:
                  (id<FormInputAccessoryViewDelegate>)navigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_
