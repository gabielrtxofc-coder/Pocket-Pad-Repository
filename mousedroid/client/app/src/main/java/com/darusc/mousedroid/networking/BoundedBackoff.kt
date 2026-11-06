package com.darusc.mousedroid.networking

/** Deterministic capped exponential delay used by the single USB retry owner. */
internal class BoundedBackoff(
    private val initialDelayMillis: Long = 500L,
    private val maximumDelayMillis: Long = 30_000L,
) {
    private var nextDelayMillis = initialDelayMillis

    init {
        require(initialDelayMillis > 0L)
        require(maximumDelayMillis >= initialDelayMillis)
    }

    fun nextDelayMillis(): Long {
        val result = nextDelayMillis
        nextDelayMillis = (nextDelayMillis * 2L).coerceAtMost(maximumDelayMillis)
        return result
    }

    fun reset() {
        nextDelayMillis = initialDelayMillis
    }
}
