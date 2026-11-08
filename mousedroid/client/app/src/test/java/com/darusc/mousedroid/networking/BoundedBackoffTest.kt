package com.darusc.mousedroid.networking

import org.junit.Assert.assertEquals
import org.junit.Test

class BoundedBackoffTest {
    @Test
    fun doublesDelayAndNeverExceedsTheConfiguredMaximum() {
        val backoff = BoundedBackoff(initialDelayMillis = 500L, maximumDelayMillis = 4_000L)
        val delays = (0 until 8).map { backoff.nextDelayMillis() }
        assertEquals(listOf(500L, 1_000L, 2_000L, 4_000L, 4_000L, 4_000L, 4_000L, 4_000L), delays)
    }

    @Test
    fun resetRestoresImmediateRetryAfterRecovery() {
        val backoff = BoundedBackoff()
        backoff.nextDelayMillis()
        backoff.nextDelayMillis()
        backoff.reset()
        assertEquals(500L, backoff.nextDelayMillis())
    }
}
