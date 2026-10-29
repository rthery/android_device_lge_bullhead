/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.imsbridge;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.WifiInfo;
import android.os.UserHandle;
import android.util.Log;

/**
 * The 8.1 CNEService reads the Wi-Fi address and AP from the linkProperties and wifiInfo extras of
 * NETWORK_STATE_CHANGED_ACTION, which current Wi-Fi no longer sends. Without them it reports Wi-Fi
 * to the modem with no address, and the modem never offers IWLAN. Resend the broadcast to it with
 * those extras.
 */
public class WifiInfoRelay extends ConnectivityManager.NetworkCallback {
    private static final String TAG = "ImsCompatBridge";
    private static final String CNE_PACKAGE = "com.quicinc.cne.CNEService";
    private static final String NETWORK_STATE_CHANGED_ACTION = "android.net.wifi.STATE_CHANGE";

    private final Context mContext;
    private NetworkCapabilities mCaps;
    private LinkProperties mLinkProperties;

    public WifiInfoRelay(Context context) {
        super(FLAG_INCLUDE_LOCATION_INFO);
        mContext = context;
    }

    public void start() {
        NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build();
        mContext.getSystemService(ConnectivityManager.class).registerNetworkCallback(request, this);
    }

    public void stop() {
        mContext.getSystemService(ConnectivityManager.class).unregisterNetworkCallback(this);
    }

    @Override
    public void onCapabilitiesChanged(Network network, NetworkCapabilities caps) {
        mCaps = caps;
        relay();
    }

    @Override
    public void onLinkPropertiesChanged(Network network, LinkProperties lp) {
        mLinkProperties = lp;
        relay();
    }

    @Override
    public void onLost(Network network) {
        mCaps = null;
        mLinkProperties = null;
    }

    private void relay() {
        if (mCaps == null || mLinkProperties == null) return;
        NetworkInfo info = new NetworkInfo(ConnectivityManager.TYPE_WIFI, 0, "WIFI", "");
        info.setDetailedState(NetworkInfo.DetailedState.CONNECTED, null, null);
        Intent intent = new Intent(NETWORK_STATE_CHANGED_ACTION)
                .setPackage(CNE_PACKAGE)
                .putExtra("networkInfo", info)
                .putExtra("linkProperties", new LinkProperties(mLinkProperties));
        if (mCaps.getTransportInfo() instanceof WifiInfo wifiInfo) {
            intent.putExtra("wifiInfo", wifiInfo);
        }
        mContext.sendBroadcastAsUser(intent, UserHandle.SYSTEM);
        Log.d(TAG, "Relayed Wi-Fi state to CNE: " + mLinkProperties.getInterfaceName());
    }
}
