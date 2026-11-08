package com.darusc.mousedroid.networking.sockets

import android.os.SystemClock
import android.util.Log
import com.darusc.mousedroid.mkinput.InputEvent
import com.darusc.mousedroid.networking.Connection
import com.darusc.mousedroid.networking.toSocketReport
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketException
import java.net.SocketTimeoutException
import java.util.concurrent.atomic.AtomicBoolean

/** A bounded-connect-time TCP session with one reader and one heartbeat owner. */
class TCPConnection(
    ipAddress: String,
    port: Int,
    private val isOverAdb: Boolean,
    private val listener: Listener,
) : Connection() {

    companion object {
        private const val TAG = "Mousedroid"
        private const val CONNECT_TIMEOUT_MS = 2_500
        private const val READ_TIMEOUT_MS = 1_000
        private const val HEARTBEAT_INTERVAL_MS = 4_000L
        private const val HEARTBEAT_TIMEOUT_MS = 12_000L
        private const val HEARTBEAT_BYTE: Byte = 0x00
    }

    override val maxPacketSize = 65_472

    private val socket = Socket()
    private val running = AtomicBoolean(false)
    private val locallyClosed = AtomicBoolean(false)
    private val disconnected = AtomicBoolean(false)
    private val outputLock = Any()
    private lateinit var outputStream: OutputStream
    private lateinit var inputStream: InputStream
    private var readerThread: Thread? = null
    private var heartbeatThread: Thread? = null
    @Volatile
    private var lastHeartbeatResponse = SystemClock.elapsedRealtime()

    val isConnected: Boolean
        get() = running.get() && socket.isConnected && !socket.isClosed && !disconnected.get()

    init {
        try {
            socket.tcpNoDelay = true
            socket.keepAlive = true
            socket.connect(InetSocketAddress(ipAddress, port), CONNECT_TIMEOUT_MS)
            socket.soTimeout = READ_TIMEOUT_MS
            outputStream = socket.getOutputStream()
            inputStream = socket.getInputStream()
            running.set(true)

            readerThread = Thread({ receiveLoop() }, "PocketPad-TCP-reader").apply { isDaemon = true }
            heartbeatThread = Thread({ heartbeatLoop() }, "PocketPad-TCP-heartbeat").apply { isDaemon = true }
            readerThread?.start()
            heartbeatThread?.start()
            Log.d(TAG, "TCP connected to $ipAddress:$port")
        } catch (error: Exception) {
            running.set(false)
            try {
                socket.close()
            } catch (_: Exception) {
            }
            throw ConnectionFailedException("$ipAddress:$port")
        }
    }

    override fun send(event: InputEvent) {
        event.toSocketReport().forEach { packet ->
            if (packet.isNotEmpty()) write(packet)
        }
    }

    fun sendBytes(bytes: ByteArray) {
        write(bytes)
    }

    private fun write(bytes: ByteArray) {
        if (!isConnected) throw SocketException("TCP session is closed")
        try {
            synchronized(outputLock) {
                if (!isConnected) throw SocketException("TCP session is closed")
                outputStream.write(bytes)
                outputStream.flush()
            }
        } catch (error: Exception) {
            handleDisconnect()
            throw error
        }
    }

    private fun receiveLoop() {
        val buffer = ByteArray(64)
        while (running.get()) {
            try {
                val count = inputStream.read(buffer)
                if (count < 0) {
                    handleDisconnect()
                    return
                }
                if (count == 0) continue
                for (index in 0 until count) {
                    if (buffer[index] == HEARTBEAT_BYTE) {
                        lastHeartbeatResponse = SystemClock.elapsedRealtime()
                    }
                }
                listener.onBytesReceived(buffer.copyOf(count), count)
            } catch (_: SocketTimeoutException) {
                if (SystemClock.elapsedRealtime() - lastHeartbeatResponse >= HEARTBEAT_TIMEOUT_MS) {
                    handleDisconnect()
                    return
                }
            } catch (error: Exception) {
                if (running.get() && !locallyClosed.get()) {
                    Log.w(TAG, "TCP receive ended: ${error.message}")
                    handleDisconnect()
                }
                return
            }
        }
    }

    private fun heartbeatLoop() {
        while (running.get()) {
            try {
                Thread.sleep(HEARTBEAT_INTERVAL_MS)
            } catch (_: InterruptedException) {
                return
            }
            if (!running.get()) return
            if (SystemClock.elapsedRealtime() - lastHeartbeatResponse >= HEARTBEAT_TIMEOUT_MS) {
                handleDisconnect()
                return
            }
            try {
                write(byteArrayOf(HEARTBEAT_BYTE))
            } catch (_: Exception) {
                return
            }
        }
    }

    private fun handleDisconnect() {
        if (locallyClosed.get() || !disconnected.compareAndSet(false, true)) return
        running.set(false)
        try {
            socket.close()
        } catch (_: Exception) {
        }
        val host = if (isOverAdb) "127.0.0.1" else (socket.inetAddress?.hostAddress ?: "x.x.x.x")
        val mode = if (isOverAdb) Mode.USB else Mode.WIFI
        listener.onDisconnected(mode, host)
    }

    override fun close() {
        locallyClosed.set(true)
        running.set(false)
        try {
            socket.close()
        } catch (_: Exception) {
        }
        heartbeatThread?.interrupt()
        joinUnlessCurrent(readerThread)
        joinUnlessCurrent(heartbeatThread)
    }

    private fun joinUnlessCurrent(thread: Thread?) {
        if (thread == null || thread === Thread.currentThread()) return
        try {
            thread.join(1_500L)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }
}
