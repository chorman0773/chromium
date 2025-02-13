// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.autofill.AutofillNameFixFlowPrompt.AutofillNameFixFlowPromptDelegate;
import org.chromium.chrome.browser.autofill.AutofillNameFixFlowPrompt.LegalMessageLine;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * JNI call glue for AutofillNameFixFlowPrompt C++ and Java objects.
 */
@JNINamespace("autofill")
final class AutofillNameFixFlowBridge implements AutofillNameFixFlowPromptDelegate {
    private final long mNativeCardNameFixFlowViewAndroid;
    private final Activity mActivity;
    private final String mTitle;
    private final String mInferredName;
    private final String mConfirmButtonLabel;
    private final int mIconId;
    private final List<LegalMessageLine> mLegalMessageLines = new ArrayList<LegalMessageLine>();
    private AutofillNameFixFlowPrompt mNameFixFlowPrompt;

    private AutofillNameFixFlowBridge(long nativeCardNameFixFlowViewAndroid, String title,
            String inferredName, String confirmButtonLabel, int iconId,
            WindowAndroid windowAndroid) {
        mNativeCardNameFixFlowViewAndroid = nativeCardNameFixFlowViewAndroid;
        mTitle = title;
        mInferredName = inferredName;
        mConfirmButtonLabel = confirmButtonLabel;
        mIconId = iconId;

        mActivity = windowAndroid.getActivity().get();
        if (mActivity == null) {
            mNameFixFlowPrompt = null;
            // Clean up the native counterpart. This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            ThreadUtils.postOnUiThread(() -> onPromptDismissed());
        }
    }

    @CalledByNative
    private static AutofillNameFixFlowBridge create(long nativeNameFixFlowPrompt, String title,
            String inferredName, String confirmButtonLabel, int iconId,
            WindowAndroid windowAndroid) {
        return new AutofillNameFixFlowBridge(nativeNameFixFlowPrompt, title, inferredName,
                confirmButtonLabel, iconId, windowAndroid);
    }

    /**
     * Adds a line of legal message plain text to the infobar.
     *
     * @param text The legal message plain text.
     */
    @CalledByNative
    private void addLegalMessageLine(String text) {
        mLegalMessageLines.add(new LegalMessageLine(text));
    }

    /**
     * Marks up the last added line of legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastLegalMessageLine(int start, int end, String url) {
        mLegalMessageLines.get(mLegalMessageLines.size() - 1)
                .links.add(new LegalMessageLine.Link(start, end, url));
    }

    @Override
    public void onPromptDismissed() {
        nativePromptDismissed(mNativeCardNameFixFlowViewAndroid);
    }

    @Override
    public void onUserAccept(String name) {
        nativeOnUserAccept(mNativeCardNameFixFlowViewAndroid, name);
    }

    @Override
    public void onLegalMessageLinkClicked(String url) {
        nativeOnLegalMessageLinkClicked(mNativeCardNameFixFlowViewAndroid, url);
    }

    /**
     * Shows a prompt for name fix flow.
     */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        mNameFixFlowPrompt = new AutofillNameFixFlowPrompt(mActivity, this, mTitle, mInferredName,
                mLegalMessageLines, mConfirmButtonLabel, ResourceId.mapToDrawableId(mIconId));

        if (mNameFixFlowPrompt != null) {
            mNameFixFlowPrompt.show((ChromeActivity) (windowAndroid.getActivity().get()));
        }
    }

    /**
     * Dismisses the prompt without returning any user response.
     */
    @CalledByNative
    private void dismiss() {
        if (mNameFixFlowPrompt != null) mNameFixFlowPrompt.dismiss();
    }

    private native void nativePromptDismissed(long nativeCardNameFixFlowViewAndroid);
    private native void nativeOnUserAccept(long nativeCardNameFixFlowViewAndroid, String name);
    private native void nativeOnLegalMessageLinkClicked(
            long nativeCardNameFixFlowViewAndroid, String url);
}
