package com.darusc.mousedroid.mkinput

import kotlin.math.abs
import kotlin.math.hypot

/** A two-finger gesture has exactly one outcome: tap, scroll, zoom or cancellation. */
internal class TwoFingerGesture(private val slop: Float, private val tick: Float) {
    enum class Mode { UNDECIDED, SCROLL, ZOOM, BLOCKED }
    data class Output(val scrollX: Int = 0, val scrollY: Int = 0, val zoom: Int = 0)
    private var mode = Mode.BLOCKED
    private var startTime = 0L
    private var startX = 0f; private var startY = 0f; private var startSpan = 0f
    private var lastX = 0f; private var lastY = 0f; private var lastSpan = 0f
    private var pendingX = 0f; private var pendingY = 0f; private var pendingZoom = 0f
    private var maxFingerMovement = 0f
    private var ax = 0f; private var ay = 0f; private var bx = 0f; private var by = 0f

    fun begin(x1: Float, y1: Float, x2: Float, y2: Float, time: Long) {
        mode = Mode.UNDECIDED; startTime = time
        ax = x1; ay = y1; bx = x2; by = y2
        startX = (x1 + x2) / 2; startY = (y1 + y2) / 2; startSpan = hypot(x1 - x2, y1 - y2)
        lastX = startX; lastY = startY; lastSpan = startSpan
        pendingX = 0f; pendingY = 0f; pendingZoom = 0f; maxFingerMovement = 0f
    }

    fun block() { mode = Mode.BLOCKED }

    fun move(x1: Float, y1: Float, x2: Float, y2: Float, scrollSpeed: Float): Output {
        if (mode == Mode.BLOCKED) return Output()
        val x = (x1 + x2) / 2; val y = (y1 + y2) / 2; val span = hypot(x1 - x2, y1 - y2)
        maxFingerMovement = maxOf(maxFingerMovement, hypot(x1 - ax, y1 - ay), hypot(x2 - bx, y2 - by))
        if (mode == Mode.UNDECIDED) {
            val translation = hypot(x - startX, y - startY)
            val scale = abs(span - startSpan) / 2f
            if (maxOf(translation, scale) >= slop) {
                mode = if (scale > translation) Mode.ZOOM else Mode.SCROLL
                // Include the movement used for classification.
                lastX = startX; lastY = startY; lastSpan = startSpan
            } else {
                return Output()
            }
        }
        var result = Output()
        if (mode == Mode.SCROLL) {
            pendingX += (x - lastX) * scrollSpeed
            pendingY += (y - lastY) * scrollSpeed
            // Both protocols consume one axis per event; lock to dominant accumulated axis.
            if (abs(pendingX) >= tick || abs(pendingY) >= tick) {
                if (abs(pendingY) >= abs(pendingX)) {
                    val amount = (pendingY / tick).toInt().coerceIn(-12, 12) * 10
                    pendingY -= amount / 10f * tick
                    pendingX = 0f
                    result = Output(scrollY = amount)
                } else {
                    val amount = (pendingX / tick).toInt().coerceIn(-12, 12) * 10
                    pendingX -= amount / 10f * tick
                    pendingY = 0f
                    result = Output(scrollX = amount)
                }
            }
        } else {
            pendingZoom += span - lastSpan
            val ticks = (pendingZoom / tick).toInt().coerceIn(-127, 127)
            pendingZoom -= ticks * tick
            result = Output(zoom = ticks)
        }
        lastX = x; lastY = y; lastSpan = span
        return result
    }

    fun finish(time: Long): Boolean {
        val tap = mode == Mode.UNDECIDED && maxFingerMovement < slop && time - startTime in 0..250
        block()
        return tap
    }
}
