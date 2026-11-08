package com.darusc.mousedroid.mkinput

import org.junit.Assert.assertEquals
import org.junit.Test

class PointerMotionTest {
    @Test fun fractionalMovementIsPreserved() {
        val motion = PointerMotion()
        assertEquals(0 to 0, motion.move(.4f, -.4f, 16, 1f, 0f))
        assertEquals(0 to 0, motion.move(.4f, -.4f, 16, 1f, 0f))
        assertEquals(1 to -1, motion.move(.4f, -.4f, 16, 1f, 0f))
    }
    @Test fun resetDoesNotLeakMovementIntoNextGesture() {
        val motion = PointerMotion()
        motion.move(.7f, .7f, 16, 1f, 0f)
        motion.reset()
        assertEquals(0 to 0, motion.move(.5f, .5f, 16, 1f, 0f))
    }
    @Test fun sensitivityAndSpeedAffectGain() {
        assertEquals(10 to 0, PointerMotion().move(10f, 0f, 10, 1f, 0f))
        assertEquals(20 to 0, PointerMotion().move(10f, 0f, 10, 2f, 0f))
        assertEquals(15 to 0, PointerMotion().move(10f, 0f, 5, 1f, .5f))
        assertEquals(10 to 0, PointerMotion().move(10f, 0f, 1000, 1f, .5f))
    }
    @Test fun largeDeltasAreNotLostWhenClamped() {
        val motion = PointerMotion()
        assertEquals(200 to -200, motion.move(200f, -200f, 16, 1f, 0f))
        assertEquals(-10 to 10, motion.move(-10f, 10f, 16, 1f, 0f))
    }
}
