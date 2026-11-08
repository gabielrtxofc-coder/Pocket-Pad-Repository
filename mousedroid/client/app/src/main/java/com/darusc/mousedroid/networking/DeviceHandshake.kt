package com.darusc.mousedroid.networking

import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Length-prefixed device metadata frame used before input commands on TCP. */
internal object DeviceHandshake {
    fun encode(details: String): ByteArray {
        val payload = details.toByteArray(Charsets.UTF_8)
        require(payload.isNotEmpty() && payload.size <= 127) { "Device details must be 1–127 UTF-8 bytes" }
        return ByteBuffer.allocate(payload.size + 2)
            .order(ByteOrder.BIG_ENDIAN)
            .putShort(payload.size.toShort())
            .put(payload)
            .array()
    }
}
