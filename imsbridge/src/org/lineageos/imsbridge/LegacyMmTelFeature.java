/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.imsbridge;

import android.app.PendingIntent;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.telephony.ims.ImsCallProfile;
import android.telephony.ims.compat.feature.MMTelFeature;
import android.telephony.ims.stub.ImsEcbmImplBase;
import android.telephony.ims.stub.ImsMultiEndpointImplBase;
import android.telephony.ims.stub.ImsUtImplBase;
import android.util.Log;

import com.android.ims.internal.IImsCallSession;
import com.android.ims.internal.IImsCallSessionListener;
import com.android.ims.internal.IImsConfig;
import com.android.ims.internal.IImsEcbm;
import com.android.ims.internal.IImsMultiEndpoint;
import com.android.ims.internal.IImsRegistrationListener;
import com.android.ims.internal.IImsService;
import com.android.ims.internal.IImsUt;

/** Forwards the compat MMTel feature to the legacy IImsService that the vendor ims.apk publishes. */
public class LegacyMmTelFeature extends MMTelFeature {
    private static final String TAG = "ImsCompatBridge";
    private static final String LEGACY_SERVICE = "ims";
    // com.android.ims.ImsServiceClass.MMTEL
    private static final int SERVICE_CLASS_MMTEL = 1;

    private final int mSlotId;
    private volatile IImsService mLegacy;
    private int mServiceId = -1;

    private final IBinder.DeathRecipient mDeath = () -> {
        Log.w(TAG, "Legacy ims service died");
        mLegacy = null;
        mServiceId = -1;
        setFeatureState(STATE_NOT_AVAILABLE);
        connect();
    };

    public LegacyMmTelFeature(int slotId) {
        mSlotId = slotId;
        setFeatureState(STATE_INITIALIZING);
        connect();
    }

    // ims.apk publishes the service from its boot receiver, possibly after we are bound.
    private void connect() {
        new Thread(() -> {
            IBinder binder = ServiceManager.waitForService(LEGACY_SERVICE);
            if (binder == null) {
                Log.e(TAG, "Legacy ims service not found");
                return;
            }
            try {
                binder.linkToDeath(mDeath, 0);
            } catch (RemoteException e) {
                mDeath.binderDied();
                return;
            }
            mLegacy = IImsService.Stub.asInterface(binder);
            Log.i(TAG, "Connected to the legacy ims service for slot " + mSlotId);
            setFeatureState(STATE_READY);
        }, TAG).start();
    }

    private IImsService legacy() throws RemoteException {
        IImsService service = mLegacy;
        if (service == null) throw new RemoteException("Legacy ims service not connected");
        return service;
    }

    @Override
    public int startSession(PendingIntent incomingCallIntent, IImsRegistrationListener listener) {
        try {
            mServiceId = legacy().open(mSlotId, SERVICE_CLASS_MMTEL, incomingCallIntent, listener);
            Log.i(TAG, "Opened legacy session " + mServiceId);
        } catch (RemoteException e) {
            Log.e(TAG, "open failed", e);
        }
        return mServiceId;
    }

    @Override
    public void endSession(int sessionId) {
        try {
            legacy().close(sessionId);
        } catch (RemoteException e) {
            Log.e(TAG, "close failed", e);
        }
    }

    @Override
    public boolean isConnected(int callSessionType, int callType) {
        try {
            return legacy().isConnected(mServiceId, callSessionType, callType);
        } catch (RemoteException e) {
            return false;
        }
    }

    @Override
    public boolean isOpened() {
        try {
            return legacy().isOpened(mServiceId);
        } catch (RemoteException e) {
            return false;
        }
    }

    @Override
    public void addRegistrationListener(IImsRegistrationListener listener) {
        try {
            legacy().addRegistrationListener(mSlotId, SERVICE_CLASS_MMTEL, listener);
        } catch (RemoteException e) {
            Log.e(TAG, "addRegistrationListener failed", e);
        }
    }

    // The legacy interface has no way to remove a listener.
    @Override
    public void removeRegistrationListener(IImsRegistrationListener listener) {
    }

    @Override
    public ImsCallProfile createCallProfile(int sessionId, int callSessionType, int callType) {
        try {
            return legacy().createCallProfile(sessionId, callSessionType, callType);
        } catch (RemoteException e) {
            Log.e(TAG, "createCallProfile failed", e);
            return null;
        }
    }

    @Override
    public IImsCallSession createCallSession(int sessionId, ImsCallProfile profile,
            IImsCallSessionListener listener) {
        try {
            return legacy().createCallSession(sessionId, profile, listener);
        } catch (RemoteException e) {
            Log.e(TAG, "createCallSession failed", e);
            return null;
        }
    }

    @Override
    public IImsCallSession getPendingCallSession(int sessionId, String callId) {
        try {
            return legacy().getPendingCallSession(sessionId, callId);
        } catch (RemoteException e) {
            Log.e(TAG, "getPendingCallSession failed", e);
            return null;
        }
    }

    @Override
    public ImsUtImplBase getUtInterface() {
        try {
            IImsUt ut = legacy().getUtInterface(mServiceId);
            return ut == null ? null : new ImsUtImplBase() {
                @Override
                public IImsUt getInterface() {
                    return ut;
                }
            };
        } catch (RemoteException e) {
            Log.e(TAG, "getUtInterface failed", e);
            return null;
        }
    }

    @Override
    public IImsConfig getConfigInterface() {
        try {
            return legacy().getConfigInterface(mSlotId);
        } catch (RemoteException e) {
            Log.e(TAG, "getConfigInterface failed", e);
            return null;
        }
    }

    @Override
    public void turnOnIms() {
        try {
            legacy().turnOnIms(mSlotId);
        } catch (RemoteException e) {
            Log.e(TAG, "turnOnIms failed", e);
        }
    }

    @Override
    public void turnOffIms() {
        try {
            legacy().turnOffIms(mSlotId);
        } catch (RemoteException e) {
            Log.e(TAG, "turnOffIms failed", e);
        }
    }

    @Override
    public ImsEcbmImplBase getEcbmInterface() {
        try {
            IImsEcbm ecbm = legacy().getEcbmInterface(mServiceId);
            return ecbm == null ? null : new ImsEcbmImplBase() {
                @Override
                public IImsEcbm getImsEcbm() {
                    return ecbm;
                }
            };
        } catch (RemoteException e) {
            Log.e(TAG, "getEcbmInterface failed", e);
            return null;
        }
    }

    @Override
    public void setUiTTYMode(int uiTtyMode, Message onComplete) {
        try {
            legacy().setUiTTYMode(mServiceId, uiTtyMode, onComplete);
        } catch (RemoteException e) {
            Log.e(TAG, "setUiTTYMode failed", e);
        }
    }

    @Override
    public ImsMultiEndpointImplBase getMultiEndpointInterface() {
        try {
            IImsMultiEndpoint endpoint = legacy().getMultiEndpointInterface(mServiceId);
            return endpoint == null ? null : new ImsMultiEndpointImplBase() {
                @Override
                public IImsMultiEndpoint getIImsMultiEndpoint() {
                    return endpoint;
                }
            };
        } catch (RemoteException e) {
            Log.e(TAG, "getMultiEndpointInterface failed", e);
            return null;
        }
    }

    @Override
    public void onFeatureReady() {
    }

    @Override
    public void onFeatureRemoved() {
    }
}
