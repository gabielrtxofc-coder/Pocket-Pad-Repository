package com.darusc.mousedroid.networking

import com.darusc.mousedroid.mkinput.InputEvent
import org.junit.Assert.*
import org.junit.Test

class MovementReportsTest {
    @Test fun largeDeltasAreSplitWithoutLosingMotionOrButton() {
        val event = InputEvent.MouseMove(200, -300, InputEvent.MouseButton.LEFT)
        val parts = splitMouseMovement(event.dx, event.dy)
        assertEquals(200, parts.sumOf { it.first }); assertEquals(-300, parts.sumOf { it.second })
        assertTrue(parts.all { it.first in -127..127 && it.second in -127..127 })
        val socket = event.toSocketReport()
        assertEquals(parts.size, socket.size)
        assertEquals(parts, socket.map { it[1].toInt() to it[2].toInt() })
        val hid = event.toHIDReport()
        assertEquals(parts.size, hid.size)
        assertTrue(hid.all { it.bytes[0].toInt() == 1 })
        assertEquals(parts, hid.map { -it.bytes[1].toInt() to -it.bytes[2].toInt() })
    }
    @Test fun twoFingerScrollReachesUsbAndBluetooth() {
        val vertical = InputEvent.MouseScroll(0, 10)
        assertArrayEquals(byteArrayOf(6, 10), vertical.toSocketReport().single())
        assertEquals(1, vertical.toHIDReport().single().bytes[3].toInt())
        val horizontal = InputEvent.MouseScroll(-10, 0)
        assertArrayEquals(byteArrayOf(8, -10), horizontal.toSocketReport().single())
        val reports = horizontal.toHIDReport()
        assertEquals(3, reports.size)
        assertEquals(-1, reports[1].bytes[3].toInt())
    }
}
