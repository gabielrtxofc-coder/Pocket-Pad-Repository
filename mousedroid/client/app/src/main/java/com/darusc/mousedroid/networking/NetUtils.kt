package com.darusc.mousedroid.networking

import android.content.Context
import android.content.IntentFilter
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Build

private const val USB_STATE_ACTION = "android.hardware.usb.action.USB_STATE"

/** Returns the latest sticky Android USB cable state; it does not imply ADB authorization. */
@Suppress("DEPRECATION")
fun hasUsbConnection(context: Context): Boolean {
    return try {
        val appContext = context.applicationContext
        val filter = IntentFilter(USB_STATE_ACTION)
        val stateIntent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            appContext.registerReceiver(null, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            appContext.registerReceiver(null, filter)
        }
        stateIntent?.getBooleanExtra("connected", false) == true
    } catch (_: SecurityException) {
        false
    }
}

fun hasWifiConnection(context: Context): Boolean {
    val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    val network = connectivityManager.activeNetwork ?: return false
    val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
    return capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
}
