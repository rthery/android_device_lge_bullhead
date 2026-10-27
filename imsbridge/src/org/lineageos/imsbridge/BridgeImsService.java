/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.imsbridge;

import android.telephony.ims.compat.ImsService;
import android.telephony.ims.compat.feature.MMTelFeature;

/**
 * Compat ImsService that Android 13 and later can bind. The 8.1 vendor ims.apk only publishes the
 * pre-Oreo "ims" binder, which the framework no longer looks up, so each feature forwards there.
 */
public class BridgeImsService extends ImsService {

    @Override
    public MMTelFeature onCreateMMTelImsFeature(int slotId) {
        return new LegacyMmTelFeature(slotId);
    }
}
