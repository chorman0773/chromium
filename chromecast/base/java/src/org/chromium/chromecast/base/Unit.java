// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Represents a type with only one possible instance.
 *
 * Retrieved by calling the static unit() function.
 * In algebraic type theory, this is the 1 (or "Singleton") type.
 *
 * Useful in generic method signatures to represent returning nothing (because Void is not
 * instantiable).
 */
public final class Unit {
    private static Unit sInstance;
    private Unit() {}
    public static Unit unit() {
        if (sInstance == null) sInstance = new Unit();
        return sInstance;
    }
}
