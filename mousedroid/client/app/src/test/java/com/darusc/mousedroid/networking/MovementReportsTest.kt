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
    }
}
