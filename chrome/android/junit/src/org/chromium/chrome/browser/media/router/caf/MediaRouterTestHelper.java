// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.support.v7.media.MediaRouter;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

/** Test helper for MediaRouter-related functionalities. */
public class MediaRouterTestHelper {
    @Mock
    private ShadowMediaRouter.ShadowImplementation mShadowMediaRouter;
    @Mock
    private MediaRouter.RouteInfo mDefaultRoute;
    @Mock
    private MediaRouter.RouteInfo mCastRoute;
    @Mock
    private MediaRouter.RouteInfo mOtherCastRoute;

    public MediaRouterTestHelper() {
        MockitoAnnotations.initMocks(this);
        setUpRoutes();
        ShadowMediaRouter.setImplementation(mShadowMediaRouter);
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                selectRoute(getDefaultRoute());
                return null;
            }
        })
                .when(mShadowMediaRouter)
                .unselect(anyInt());
        doReturn(mDefaultRoute).when(mShadowMediaRouter).getDefaultRoute();
    }

    public ShadowMediaRouter.ShadowImplementation getShadowImpl() {
        return mShadowMediaRouter;
    }

    public MediaRouter.RouteInfo getDefaultRoute() {
        return mDefaultRoute;
    }

    public MediaRouter.RouteInfo getCastRoute() {
        return mCastRoute;
    }

    public MediaRouter.RouteInfo getOtherCastRoute() {
        return mOtherCastRoute;
    }

    private void setUpRoutes() {
        selectRoute(mDefaultRoute);

        setUpRouteStubs(mDefaultRoute);
        setUpRouteStubs(mCastRoute);
        setUpRouteStubs(mOtherCastRoute);

        doReturn("default-route").when(mDefaultRoute).getId();
        doReturn("cast-route").when(mCastRoute).getId();
        doReturn("other-cast-route").when(mOtherCastRoute).getId();
    }

    public void selectRoute(MediaRouter.RouteInfo routeInfo) {
        doReturn(false).when(mDefaultRoute).isSelected();
        doReturn(false).when(mCastRoute).isSelected();
        doReturn(false).when(mOtherCastRoute).isSelected();
        doReturn(true).when(routeInfo).isSelected();
    }

    private void setUpRouteStubs(MediaRouter.RouteInfo routeInfo) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                selectRoute(routeInfo);
                return null;
            }
        })
                .when(routeInfo)
                .select();
    }
}
