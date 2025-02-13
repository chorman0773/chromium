// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_SUGGESTIONS_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_SUGGESTIONS_PROVIDER_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/autofill/form_suggestion_client.h"

namespace autofill {
struct FormActivityParams;
}

namespace web {
struct FormActivityParams;
class WebState;
}  // namespace web

@class FormSuggestion;
@protocol FormInputAccessoryViewDelegate;
@protocol FormInputSuggestionsProvider;

// Block type to provide form suggestions asynchronously.
typedef void (^FormSuggestionsReadyCompletion)(
    NSArray<FormSuggestion*>* suggestions,
    id<FormInputSuggestionsProvider> provider);

// Represents an object that can provide form input suggestions.
@protocol FormInputSuggestionsProvider<FormSuggestionClient>

// A delegate for form navigation.
@property(nonatomic, assign) id<FormInputAccessoryViewDelegate>
    accessoryViewDelegate;

// Asynchronously retrieves form suggestions from this provider for the
// specified form/field and returns it via |accessoryViewUpdateBlock|. View
// will be nil if no accessories are available from this provider.
- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState
          accessoryViewUpdateBlock:
              (FormSuggestionsReadyCompletion)accessoryViewUpdateBlock;

// Notifies this provider that the accessory view is going away.
- (void)inputAccessoryViewControllerDidReset;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_SUGGESTIONS_PROVIDER_H_
