package com.darusc.mousedroid.mkinput

import kotlin.math.hypot

/** Fractional movement is carried forward instead of being discarded per event. */
internal class PointerMotion {
    private var remainderX = 0f
    private var remainderY = 0f

    fun reset() { remainderX = 0f; remainderY = 0f }

    fun move(dx: Float, dy: Float, elapsedMs: Long, sensitivity: Float, acceleration: Float): Pair<Int, Int> {
        val speed = hypot(dx, dy) / elapsedMs.coerceAtLeast(1).toFloat()
        val gain = sensitivity * (1f + acceleration * (speed / 2f).coerceIn(0f, 1f))
        remainderX += dx * gain
        remainderY += dy * gain
        val x = remainderX.toInt()
        val y = remainderY.toInt()
        remainderX -= x
        remainderY -= y
        return x to y
    }
}
