/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.bullhead.blodstatus;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemProperties;

public class BlodStatusProvider extends ContentProvider {
    private static final String METHOD_GET_SUMMARY = "getSummary";
    private static final String EXTRA_SUMMARY = "com.android.settings.summary";
    // Set by init.bullhead.power.sh when /cache/blod exists
    private static final String PROP_BLOD = "vendor.bullhead.blod";

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        if (!METHOD_GET_SUMMARY.equals(method)) {
            return null;
        }
        final boolean enabled = SystemProperties.getBoolean(PROP_BLOD, false);
        final Bundle result = new Bundle();
        result.putString(EXTRA_SUMMARY,
                getContext().getString(enabled ? R.string.summary_on : R.string.summary_off));
        return result;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
