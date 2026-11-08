package com.darusc.mousedroid.mkinput

import org.junit.Assert.*
import org.junit.Test

class TwoFingerGestureTest {
    private fun gesture() = TwoFingerGesture(8f, 10f).apply { begin(0f, 0f, 100f, 0f, 0) }
    @Test fun stationaryShortGestureIsRightTap() {
        val g = gesture()
        assertEquals(TwoFingerGesture.Output(), g.move(1f, 0f, 101f, 0f, 1f))
        assertTrue(g.finish(200))
        assertFalse(g.finish(201))
    }
    @Test fun longOrMovedGestureIsNotTap() {
        assertFalse(gesture().finish(251))
        val g = gesture()
        g.move(9f, 0f, 91f, 0f, 1f)
        assertFalse(g.finish(100))
    }
    @Test fun scrollCannotZoomOrClickEvenIfFingersLaterSpread() {
        val g = gesture()
        val a = g.move(0f, 20f, 100f, 20f, 1f)
        assertEquals(20, a.scrollY); assertEquals(0, a.zoom)
        val b = g.move(-20f, 20f, 120f, 20f, 1f)
        assertEquals(0, b.zoom); assertFalse(g.finish(100))
    }
    @Test fun pinchCannotScrollOrClickEvenIfFingersLaterTranslate() {
        val g = gesture()
        val a = g.move(-10f, 0f, 110f, 0f, 1f)
        assertEquals(2, a.zoom); assertEquals(0, a.scrollY)
        val b = g.move(-10f, 40f, 110f, 40f, 1f)
        assertEquals(0, b.scrollY); assertFalse(g.finish(100))
    }
    @Test fun horizontalScrollAndSensitivity() {
        val g = gesture()
        assertEquals(20, g.move(10f, 0f, 110f, 0f, 2f).scrollX)
        assertFalse(g.finish(100))
    }
    @Test fun thirdFingerOrCancelBlocksClickAndOutput() {
        val g = gesture(); g.block()
        assertEquals(TwoFingerGesture.Output(), g.move(30f, 0f, 130f, 0f, 1f))
        assertFalse(g.finish(100))
    }
}
