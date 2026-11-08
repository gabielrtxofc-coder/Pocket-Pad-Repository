package com.darusc.mousedroid.networking

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class DeviceHandshakeTest {
    @Test
    fun encodesUtf8PayloadWithTwoByteBigEndianLength() {
        val details = "Samsung/Galaxy A12/SM-A125F/0"
        val payload = details.toByteArray(Charsets.UTF_8)
        val frame = DeviceHandshake.encode(details)

        assertEquals(payload.size shr 8, frame[0].toInt() and 0xff)
        assertEquals(payload.size and 0xff, frame[1].toInt() and 0xff)
        assertArrayEquals(payload, frame.copyOfRange(2, frame.size))
    }

    @Test
    fun rejectsOversizedHandshakeBeforeOpeningASocket() {
        assertThrows(IllegalArgumentException::class.java) {
            DeviceHandshake.encode("x".repeat(128))
        }
    }
}
