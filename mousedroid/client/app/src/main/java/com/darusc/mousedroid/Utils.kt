package com.darusc.mousedroid

import android.os.Build
import com.darusc.mousedroid.networking.Connection

/** Metadata sent in the length-prefixed USB connection handshake. */
fun getDeviceDetails(connectionMode: Connection.Mode): String {
    val manufacturer = Build.MANUFACTURER
    val model = Build.MODEL
    val deviceName = Build.MODEL.ifBlank { Build.DEVICE }
    return "$manufacturer/$deviceName/$model/${connectionMode.ordinal}"
}
