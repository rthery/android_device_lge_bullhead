/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.imsbridge;

import android.os.Bundle;
import android.os.RemoteException;

import com.android.ims.internal.IImsUt;
import com.android.ims.internal.IImsUtListener;

/**
 * Forwards to the legacy IImsUt except close(): ims.apk shares one ImsUtImpl that close() breaks
 * for good, while the framework closes its ImsUt whenever the MMTel feature is re-added.
 */
class LegacyUt extends IImsUt.Stub {
    private final IImsUt mUt;

    LegacyUt(IImsUt ut) {
        mUt = ut;
    }

    @Override
    public void close() throws RemoteException {
        mUt.setListener(null);
    }

    @Override
    public int queryCallBarring(int cbType) throws RemoteException {
        return mUt.queryCallBarring(cbType);
    }

    @Override
    public int queryCallForward(int condition, String number) throws RemoteException {
        return mUt.queryCallForward(condition, number);
    }

    @Override
    public int queryCallWaiting() throws RemoteException {
        return mUt.queryCallWaiting();
    }

    @Override
    public int queryCLIR() throws RemoteException {
        return mUt.queryCLIR();
    }

    @Override
    public int queryCLIP() throws RemoteException {
        return mUt.queryCLIP();
    }

    @Override
    public int queryCOLR() throws RemoteException {
        return mUt.queryCOLR();
    }

    @Override
    public int queryCOLP() throws RemoteException {
        return mUt.queryCOLP();
    }

    @Override
    public int transact(Bundle ssInfo) throws RemoteException {
        return mUt.transact(ssInfo);
    }

    @Override
    public int updateCallBarring(int cbType, int action, String[] barrList)
            throws RemoteException {
        return mUt.updateCallBarring(cbType, action, barrList);
    }

    @Override
    public int updateCallForward(int action, int condition, String number, int serviceClass,
            int timeSeconds) throws RemoteException {
        return mUt.updateCallForward(action, condition, number, serviceClass, timeSeconds);
    }

    @Override
    public int updateCallWaiting(boolean enable, int serviceClass) throws RemoteException {
        return mUt.updateCallWaiting(enable, serviceClass);
    }

    @Override
    public int updateCLIR(int clirMode) throws RemoteException {
        return mUt.updateCLIR(clirMode);
    }

    @Override
    public int updateCLIP(boolean enable) throws RemoteException {
        return mUt.updateCLIP(enable);
    }

    @Override
    public int updateCOLR(int presentation) throws RemoteException {
        return mUt.updateCOLR(presentation);
    }

    @Override
    public int updateCOLP(boolean enable) throws RemoteException {
        return mUt.updateCOLP(enable);
    }

    @Override
    public void setListener(IImsUtListener listener) throws RemoteException {
        mUt.setListener(listener);
    }

    @Override
    public int queryCallBarringForServiceClass(int cbType, int serviceClass)
            throws RemoteException {
        return mUt.queryCallBarringForServiceClass(cbType, serviceClass);
    }

    @Override
    public int updateCallBarringForServiceClass(int cbType, int action, String[] barrList,
            int serviceClass) throws RemoteException {
        return mUt.updateCallBarringForServiceClass(cbType, action, barrList, serviceClass);
    }

    @Override
    public int updateCallBarringWithPassword(int cbType, int action, String[] barrList,
            int serviceClass, String password) throws RemoteException {
        return mUt.updateCallBarringWithPassword(cbType, action, barrList, serviceClass,
                password);
    }
}
