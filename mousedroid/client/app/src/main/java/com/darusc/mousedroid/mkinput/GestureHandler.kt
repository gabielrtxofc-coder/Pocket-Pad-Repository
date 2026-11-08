package com.darusc.mousedroid.mkinput

import android.content.Context
import android.os.Looper
import android.view.*
import androidx.core.view.GestureDetectorCompat
import kotlinx.coroutines.Runnable

/**
 * Class that handles simples gestures (tap, long press, scroll) using
 * a GestureDetectorCompat
 */
class GestureHandler(
    context: Context,
    private val sendInputCallback: (InputEvent) -> (Unit)
) : View.OnTouchListener {

    private val preferences = context.getSharedPreferences("touchpad_preferences", Context.MODE_PRIVATE)
    private val pointerMotion = PointerMotion()
    private var lastMoveTime = 0L
    private val EV_DELAY_MILLIS: Long = 150

    private data class State(
        var scrolling: Boolean,
        var lastScrolled: Long,
        var doublePress: Boolean,
        var lastDoublePress: Long,
        var dragging: Boolean,
        var activeMouseWhileDragging: InputEvent.MouseButton
    )

    private val state = State(false, 0, false, 0, false, InputEvent.MouseButton.NONE)

    private val multi = TwoFingerGesture(
        ViewConfiguration.get(context).scaledTouchSlop.toFloat(),
        10f * context.resources.displayMetrics.density
    )
    private var multiActive = false
    private var dragX = 0f
    private var dragY = 0f

    /**
     * Detector used for gestures like: tap, double tap, move, drag and scroll
     */
    private val gestureDetector: GestureDetectorCompat =
        GestureDetectorCompat(context, object : GestureDetector.SimpleOnGestureListener() {
            override fun onSingleTapUp(e: MotionEvent): Boolean {
                // Post a runnable that sends a click event after a set delay
                // onDoubleTap will cancel it when called
                singleTapRunnable = Runnable {
                    if (preferences.getBoolean("tap_click", true)) sendInputCallback(InputEvent.MouseClick(InputEvent.MouseButton.LEFT))
                }
                handler.postDelayed(singleTapRunnable!!, EV_DELAY_MILLIS)
                return super.onSingleTapUp(e)
            }

            override fun onDoubleTap(e: MotionEvent): Boolean {
                // Cancel the click event from the single tap
                singleTapRunnable?.let { handler.removeCallbacks(it) }

                state.doublePress = true
                state.lastDoublePress = System.currentTimeMillis()

                // Post a runnable that will send 2 click events after a set time
                // If before that a move event is detected by the onTouch, it will be canceled
                doubleTapRunnable = Runnable {
                    sendInputCallback(InputEvent.MouseClick(InputEvent.MouseButton.LEFT))
                    doubleTapRunnable = Runnable {
                        sendInputCallback(InputEvent.MouseClick(InputEvent.MouseButton.LEFT))
                        state.doublePress = false
                    }
                    handler.postDelayed(doubleTapRunnable!!, 75)
                }
                handler.postDelayed(doubleTapRunnable!!, EV_DELAY_MILLIS)
                return false
            }

            override fun onScroll(
                e1: MotionEvent?,
                e2: MotionEvent,
                distanceX: Float,
                distanceY: Float
            ): Boolean {
                val elapsed = if (lastMoveTime == 0L) 16L else e2.eventTime - lastMoveTime
                lastMoveTime = e2.eventTime
                val (dx, dy) = pointerMotion.move(distanceX, distanceY, elapsed,
                    preferences.getInt("sensitivity", 100) / 100f,
                    preferences.getInt("acceleration", 0) / 100f)
                if (dx != 0 || dy != 0) {
                    sendInputCallback(InputEvent.MouseMove(dx, dy, state.activeMouseWhileDragging))
                }

                return super.onScroll(e1, e2, distanceX, distanceY)
            }
        })


    private val handler = android.os.Handler(Looper.getMainLooper())
    private var singleTapRunnable: Runnable? = null
    private var doubleTapRunnable: Runnable? = null

    fun cancel() {
        singleTapRunnable?.let { handler.removeCallbacks(it) }
        doubleTapRunnable?.let { handler.removeCallbacks(it) }
        if (state.dragging) sendInputCallback(InputEvent.MouseDragState(InputEvent.MouseButton.LEFT, false))
        state.dragging = false
        state.doublePress = false
        state.scrolling = false
        state.activeMouseWhileDragging = InputEvent.MouseButton.NONE
        pointerMotion.reset()
        lastMoveTime = 0L
        multi.block()
        multiActive = false
    }

    override fun onTouch(p0: View?, p1: MotionEvent?): Boolean {
        if (p1?.actionMasked == MotionEvent.ACTION_DOWN) {
            pointerMotion.reset()
            lastMoveTime = 0L
        }
        if (p1?.actionMasked == MotionEvent.ACTION_CANCEL) {
            gestureDetector.onTouchEvent(p1)
            cancel()
            return true
        }

        if (p1 == null) return false
        if (p1.actionMasked == MotionEvent.ACTION_POINTER_DOWN) {
            if (!multiActive && p1.pointerCount == 2) {
                // Terminate the single-finger recognizer and any pending tap or drag.
                val cancelEvent = MotionEvent.obtain(p1)
                cancelEvent.action = MotionEvent.ACTION_CANCEL
                gestureDetector.onTouchEvent(cancelEvent)
                cancelEvent.recycle()
                cancel()
                multiActive = true
                multi.begin(p1.getX(0), p1.getY(0), p1.getX(1), p1.getY(1), p1.downTime)
            } else if (multiActive) multi.block()
            return true
        }
        if (multiActive) {
            if (p1.actionMasked == MotionEvent.ACTION_MOVE && p1.pointerCount == 2) {
                val out = multi.move(p1.getX(0), p1.getY(0), p1.getX(1), p1.getY(1),
                    preferences.getInt("scroll_speed", 100) / 100f)
                if (out.scrollX != 0 || out.scrollY != 0) sendInputCallback(InputEvent.MouseScroll(out.scrollX, out.scrollY))
                if (out.zoom != 0) sendInputCallback(InputEvent.Zoom(out.zoom))
            } else if (p1.actionMasked == MotionEvent.ACTION_POINTER_UP && p1.pointerCount == 2) {
                // Include the final coordinates before deciding whether this was a tap.
                val out = multi.move(p1.getX(0), p1.getY(0), p1.getX(1), p1.getY(1),
                    preferences.getInt("scroll_speed", 100) / 100f)
                if (out.scrollX != 0 || out.scrollY != 0) sendInputCallback(InputEvent.MouseScroll(out.scrollX, out.scrollY))
                if (out.zoom != 0) sendInputCallback(InputEvent.Zoom(out.zoom))
                if (multi.finish(p1.eventTime) && preferences.getBoolean("two_finger_click", true)) {
                    sendInputCallback(InputEvent.MouseClick(InputEvent.MouseButton.RIGHT))
                }
            } else if (p1.actionMasked == MotionEvent.ACTION_UP) {
                multi.block()
                multiActive = false
            }
            return true
        }

        if (p1.action == MotionEvent.ACTION_UP && state.dragging) {
            // Cancel dragging
            state.scrolling = false
            state.doublePress = false
            state.activeMouseWhileDragging = InputEvent.MouseButton.NONE
            sendInputCallback(InputEvent.MouseDragState(InputEvent.MouseButton.LEFT, false))
            state.dragging = false
        }

        if (state.dragging && p1.actionMasked == MotionEvent.ACTION_MOVE) {
            val (dx, dy) = pointerMotion.move(dragX - p1.x, dragY - p1.y,
                (p1.eventTime - lastMoveTime).coerceAtLeast(1),
                preferences.getInt("sensitivity", 100) / 100f,
                preferences.getInt("acceleration", 0) / 100f)
            dragX = p1.x; dragY = p1.y; lastMoveTime = p1.eventTime
            if (dx != 0 || dy != 0) sendInputCallback(InputEvent.MouseMove(dx, dy, InputEvent.MouseButton.LEFT))
            return true
        }

        if (p1.let { gestureDetector.onTouchEvent(it) } == true) {
            return true
        }

        when (p1.action) {
            MotionEvent.ACTION_MOVE -> {
                if (state.doublePress && System.currentTimeMillis() - state.lastDoublePress < EV_DELAY_MILLIS) {
                    // If a move event is triggered right after a double press initiate dragging
                    // Can't be detected in onTouch because it seem like the detector classified that event
                    // as a double tap and won't detect a new scroll event right after

                    // Cancel the double tap and continue with dragging
                    doubleTapRunnable?.let { handler.removeCallbacks(it) }
                    state.doublePress = false
                    state.dragging = true
                    state.activeMouseWhileDragging = InputEvent.MouseButton.LEFT
                    dragX = p1.x; dragY = p1.y; lastMoveTime = p1.eventTime
                    // Send a mouse down event to initiate dragging
                    sendInputCallback(
                        InputEvent.MouseDragState(
                            InputEvent.MouseButton.LEFT,
                            true
                        )
                    )

                    val cancelEvent = MotionEvent.obtain(p1)
                    cancelEvent.action = MotionEvent.ACTION_CANCEL
                    gestureDetector.onTouchEvent(cancelEvent)
                    cancelEvent.recycle()
                }
            }
        }

        return true
    }
}
