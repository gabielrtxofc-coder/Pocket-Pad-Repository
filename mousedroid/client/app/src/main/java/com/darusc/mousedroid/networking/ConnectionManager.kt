package com.darusc.mousedroid.networking

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.util.Log
import com.darusc.mousedroid.BatteryMonitor
import com.darusc.mousedroid.mkinput.InputEvent
import com.darusc.mousedroid.networking.bluetooth.BluetoothConnection
import com.darusc.mousedroid.networking.sockets.TCPConnection
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import kotlin.coroutines.coroutineContext

/**
 * Process-wide connection owner. USB uses one supervisor coroutine and one
 * socket at a time; cable/EOF/heartbeat events only wake that owner.
 */
class ConnectionManager private constructor() : Connection.Listener, BatteryMonitor.Listener {

    companion object {
        private const val TAG = "Mousedroid"
        private const val USB_STATE_ACTION = "android.hardware.usb.action.USB_STATE"

        @Volatile
        private var instance: ConnectionManager? = null

        fun getInstance(): ConnectionManager = synchronized(this) {
            instance ?: ConnectionManager().also { instance = it }
        }

        fun getInstance(connectionStateCallback: ConnectionStateCallback): ConnectionManager =
            getInstance().also { it.setConnectionStateCallback(connectionStateCallback) }
    }

    sealed class UsbStatus {
        object WaitingForCable : UsbStatus()
        data class Connecting(val attempt: Int) : UsbStatus()
        object Connected : UsbStatus()
        data class Retrying(val message: String, val delayMillis: Long) : UsbStatus()
    }

    interface ConnectionStateCallback {
        fun onConnectionInitiated(mode: Connection.Mode) {}
        fun onConnectionSuccessful(connectionMode: Connection.Mode, hostName: String) {}
        fun onConnectionFailed(connectionMode: Connection.Mode) {}
        fun onDisconnected(connectionMode: Connection.Mode, hostName: String) {}
        fun onUsbStatusChanged(status: UsbStatus) {}
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val lock = Any()
    private val usbConnectMutex = Mutex()
    private val usbWake = kotlinx.coroutines.channels.Channel<Unit>(kotlinx.coroutines.channels.Channel.CONFLATED)

    @Volatile
    private var connectionStateCallback: ConnectionStateCallback? = null
    private var usbSupervisor: Job? = null
    private var usbModeActive = false
    private var usbGeneration = 0L
    private var usbConnection: TCPConnection? = null
    private var usbReceiver: BroadcastReceiver? = null
    private var usbReceiverContext: Context? = null
    private var bluetoothConnection: BluetoothConnection? = null
    @Volatile
    private var connected = false

    init {
        BatteryMonitor.getInstance().addListener(this)
    }

    private fun setConnectionStateCallback(callback: ConnectionStateCallback) {
        connectionStateCallback = callback
    }

    fun clearConnectionStateCallback(callback: ConnectionStateCallback) {
        synchronized(lock) {
            if (connectionStateCallback === callback) connectionStateCallback = null
        }
    }

    /** Start once and keep retrying while this process-level USB mode is active. */
    fun startUsbAutoConnect(context: Context, port: Int, deviceDetails: String) {
        val appContext = context.applicationContext
        synchronized(lock) {
            if (usbSupervisor?.isActive == true) {
                usbWake.trySend(Unit)
                return
            }

            usbModeActive = true
            usbGeneration += 1
            val generation = usbGeneration
            registerUsbReceiverLocked(appContext)
            usbSupervisor = scope.launch {
                runUsbSupervisor(appContext, port, deviceDetails, generation)
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun registerUsbReceiverLocked(context: Context) {
        if (usbReceiver != null) return
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                if (intent.action != USB_STATE_ACTION) return
                usbWake.trySend(Unit)
            }
        }
        try {
            val filter = IntentFilter(USB_STATE_ACTION)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                context.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED)
            } else {
                context.registerReceiver(receiver, filter)
            }
            usbReceiver = receiver
            usbReceiverContext = context
        } catch (error: Exception) {
            // Polling remains as a fallback if the device disallows this
            // dynamically registered broadcast on a particular Android build.
            Log.w(TAG, "USB state receiver unavailable; using bounded polling", error)
        }
    }

    private suspend fun runUsbSupervisor(
        context: Context,
        port: Int,
        deviceDetails: String,
        generation: Long,
    ) {
        val backoff = BoundedBackoff()
        var attempt = 0
        try {
            while (coroutineContext.isActive && isUsbModeActive(generation)) {
                // The sticky state is checked on each wake/timeout as a backup
                // for devices that omit a detach/attach broadcast.
                val attached = hasUsbConnection(context)
                val current = getUsbConnection(generation)

                if (current != null) {
                    // A live socket is authoritative: some Android builds do not
                    // expose USB_STATE broadcasts consistently after attach.
                    if (!current.isConnected) {
                        clearUsbConnection(current, generation)
                        current.close()
                        connected = false
                        if (!attached) {
                            backoff.reset()
                            reportUsbStatus(UsbStatus.WaitingForCable)
                            waitForUsbEvent(5_000L)
                        } else {
                            val waitMillis = backoff.nextDelayMillis()
                            reportUsbStatus(UsbStatus.Retrying(
                                "USB link was interrupted; waiting for the ADB tunnel and server.",
                                waitMillis,
                            ))
                            waitForUsbEvent(waitMillis)
                        }
                    } else {
                        waitForUsbEvent(5_000L)
                    }
                    continue
                }

                attempt += 1
                if (attached) {
                    reportUsbStatus(UsbStatus.Connecting(attempt))
                } else {
                    // A low-frequency loopback probe is also the fallback when
                    // USB_STATE is unavailable; a ready ADB reverse will connect.
                    reportUsbStatus(UsbStatus.WaitingForCable)
                }
                var candidate: TCPConnection? = null
                try {
                    usbConnectMutex.withLock {
                        if (!isUsbModeActive(generation)) return@withLock
                        val connection = TCPConnection(
                            "127.0.0.1",
                            port,
                            true,
                            usbSocketListener(generation),
                        )
                        candidate = connection
                        connection.sendBytes(DeviceHandshake.encode(deviceDetails))
                        if (!isUsbModeActive(generation) || !installUsbConnection(connection, generation)) {
                            connection.close()
                            candidate = null
                            return@withLock
                        }
                        candidate = null // ownership has transferred to the supervisor's active slot.
                        connected = true
                        backoff.reset()
                        reportUsbStatus(UsbStatus.Connected)
                        connectionStateCallback?.onConnectionSuccessful(Connection.Mode.USB, "Windows server")
                    }
                    if (!isUsbModeActive(generation)) break
                } catch (cancelled: CancellationException) {
                    candidate?.close()
                    throw cancelled
                } catch (error: Exception) {
                    candidate?.close()
                    Log.d(TAG, "USB attempt failed; the supervisor will retry: ${error.message}")
                    connected = false
                    if (!hasUsbConnection(context)) {
                        backoff.reset()
                        reportUsbStatus(UsbStatus.WaitingForCable)
                        waitForUsbEvent(5_000L)
                    } else {
                        val waitMillis = backoff.nextDelayMillis()
                        reportUsbStatus(UsbStatus.Retrying(
                            "Waiting for an authorized ADB reverse tunnel and the Windows server.",
                            waitMillis,
                        ))
                        waitForUsbEvent(waitMillis)
                    }
                }
            }
        } finally {
            val stale = removeUsbConnection(generation)
            stale?.close()
            synchronized(lock) {
                if (usbGeneration == generation) usbSupervisor = null
            }
        }
    }

    private fun usbSocketListener(generation: Long) = object : Connection.Listener {
        override fun onConnected(connectionMode: Connection.Mode, hostName: String) = Unit
        override fun onConnectionFailed(connectionMode: Connection.Mode) {
            if (connectionMode == Connection.Mode.USB && isUsbModeActive(generation)) usbWake.trySend(Unit)
        }
        override fun onBytesReceived(buffer: ByteArray, bytes: Int) = Unit
        override fun onDisconnected(connectionMode: Connection.Mode, hostName: String) {
            if (connectionMode == Connection.Mode.USB && isUsbModeActive(generation)) usbWake.trySend(Unit)
        }
    }

    private fun reportUsbStatus(status: UsbStatus) {
        connectionStateCallback?.onUsbStatusChanged(status)
    }

    private suspend fun waitForUsbEvent(timeoutMillis: Long) {
        if (timeoutMillis <= 0L) return
        withTimeoutOrNull(timeoutMillis) { usbWake.receive() }
    }

    private fun isUsbModeActive(generation: Long): Boolean = synchronized(lock) {
        usbModeActive && usbGeneration == generation
    }

    private fun getUsbConnection(generation: Long): TCPConnection? = synchronized(lock) {
        if (usbModeActive && usbGeneration == generation) usbConnection else null
    }

    private fun installUsbConnection(connection: TCPConnection, generation: Long): Boolean = synchronized(lock) {
        if (!usbModeActive || usbGeneration != generation) return@synchronized false
        usbConnection = connection
        true
    }

    private fun clearUsbConnection(connection: TCPConnection, generation: Long) {
        synchronized(lock) {
            if (usbGeneration == generation && usbConnection === connection) usbConnection = null
        }
    }

    private fun removeUsbConnection(generation: Long): TCPConnection? = synchronized(lock) {
        if (usbGeneration != generation) return@synchronized null
        val old = usbConnection
        usbConnection = null
        connected = false
        old
    }

    /** Stop retries, unregister USB events, and close the owned socket. */
    private fun stopUsbSupervisor() {
        val resources = synchronized(lock) {
            usbModeActive = false
            usbGeneration += 1
            val job = usbSupervisor
            usbSupervisor = null
            val connection = usbConnection
            usbConnection = null
            connected = false
            val receiver = usbReceiver
            val context = usbReceiverContext
            usbReceiver = null
            usbReceiverContext = null
            Triple(job, connection, Pair(receiver, context))
        }
        resources.third.first?.let { receiver ->
            try {
                resources.third.second?.unregisterReceiver(receiver)
            } catch (_: IllegalArgumentException) {
                // Already unregistered by Android during process shutdown.
            }
        }
        usbWake.trySend(Unit)
        scope.launch {
            resources.first?.cancelAndJoin()
            resources.second?.close()
        }
    }

    /** Wi-Fi server mode is intentionally disabled until pairing/authentication exists. */
    @Suppress("UNUSED_PARAMETER")
    fun connectWIFI(ipAddress: String, port: Int, deviceDetails: String) {
        Log.w(TAG, "Wi-Fi connections are disabled until authenticated pairing is implemented")
        connectionStateCallback?.onConnectionFailed(Connection.Mode.WIFI)
    }

    fun registerBluetoothHID(context: Context) {
        stopUsbSupervisor()
        scope.launch {
            val next = BluetoothConnection(context.applicationContext, this@ConnectionManager)
            val previous = synchronized(lock) {
                bluetoothConnection.also { bluetoothConnection = next }
            }
            previous?.close()
        }
    }

    fun connectBluetooth(macAddress: String) {
        connectionStateCallback?.onConnectionInitiated(Connection.Mode.BLUETOOTH)
        val connection = synchronized(lock) { bluetoothConnection }
        if (connection == null) {
            connectionStateCallback?.onConnectionFailed(Connection.Mode.BLUETOOTH)
            return
        }
        connection.connect(macAddress)
    }

    fun disconnect() {
        stopUsbSupervisor()
        val bluetooth = synchronized(lock) {
            connected = false
            bluetoothConnection.also { bluetoothConnection = null }
        }
        scope.launch { bluetooth?.close() }
    }

    override fun onConnected(connectionMode: Connection.Mode, hostName: String) {
        if (connectionMode == Connection.Mode.USB) return
        connected = true
        connectionStateCallback?.onConnectionSuccessful(connectionMode, hostName)
    }

    override fun onConnectionFailed(connectionMode: Connection.Mode) {
        if (connectionMode != Connection.Mode.USB) {
            connectionStateCallback?.onConnectionFailed(connectionMode)
        }
    }

    override fun onBytesReceived(buffer: ByteArray, bytes: Int) = Unit

    override fun onDisconnected(connectionMode: Connection.Mode, hostName: String) {
        if (connectionMode == Connection.Mode.USB) {
            usbWake.trySend(Unit)
            return
        }
        val staleBluetooth = synchronized(lock) {
            connected = false
            bluetoothConnection.also { bluetoothConnection = null }
        }
        scope.launch { staleBluetooth?.close() }
        connectionStateCallback?.onDisconnected(connectionMode, hostName)
    }

    override fun onBatteryPercentChanged(percentage: Int) {
        if (!connected) return
        send(InputEvent.BatteryEvent(percentage))
    }

    fun send(event: InputEvent, withCoroutine: Boolean = true) {
        val active = synchronized(lock) { usbConnection ?: bluetoothConnection } ?: return
        val sendSafely = {
            try {
                active.send(event)
            } catch (error: Exception) {
                Log.d(TAG, "Input send ended with a disconnected transport: ${error.message}")
                if (active is TCPConnection) usbWake.trySend(Unit)
            }
        }
        if (withCoroutine) {
            scope.launch { sendSafely() }
        } else {
            sendSafely()
        }
    }
}
