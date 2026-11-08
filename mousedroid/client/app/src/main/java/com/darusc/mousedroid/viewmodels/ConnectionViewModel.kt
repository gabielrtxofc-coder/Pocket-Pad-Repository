package com.darusc.mousedroid.viewmodels

import android.Manifest
import android.content.Context
import androidx.annotation.IdRes
import androidx.annotation.RequiresPermission
import androidx.lifecycle.viewModelScope
import com.darusc.mousedroid.getDeviceDetails
import com.darusc.mousedroid.networking.Connection
import com.darusc.mousedroid.networking.ConnectionManager
import com.darusc.mousedroid.networking.bluetooth.BluetoothAdapterWrapper
import kotlinx.coroutines.launch

class ConnectionViewModel :
    BaseViewModel<ConnectionViewModel.State, ConnectionViewModel.Event>(State.Idle),
    ConnectionManager.ConnectionStateCallback {

    sealed class State : BaseViewModel.State() {
        object Idle : State()
        object UsbPaused : State()
        data class WaitingForUsb(val message: String) : State()
        data class Connecting(val message: String) : State()
        data class Retrying(val message: String, val delayMillis: Long) : State()
        data class Connected(val connectionMode: Connection.Mode, val hostName: String) : State()
    }

    sealed class Event : BaseViewModel.Event() {
        data class Navigate(@IdRes val id: Int) : Event()
        object NavigateToInput : Event()
        object NavigateToMain : Event()
        data class NavigateToDeviceList(val mode: Connection.Mode) : Event()

        object EnableBluetooth : Event()
        object WifiModeDisabled : Event()

        data class ConnectionFailed(val connectionMode: Connection.Mode) : Event()
        data class ConnectionDisconnected(val connectionMode: Connection.Mode, val hostName: String) : Event()
    }

    private val connectionManager = ConnectionManager.getInstance(this)
    private val navigationLock = Any()
    private var inputNavigationRequested = false
    @Volatile
    private var usbAutoConnectPaused = false

    override fun onConnectionInitiated(mode: Connection.Mode) {
        if (mode == Connection.Mode.BLUETOOTH &&
            (state.value is State.Idle || state.value is State.UsbPaused)
        ) {
            setState(State.Connecting("Waiting for Bluetooth connection…"))
        }
    }

    override fun onConnectionSuccessful(connectionMode: Connection.Mode, hostName: String) {
        if (connectionMode == Connection.Mode.USB && usbAutoConnectPaused) return
        if (connectionMode == Connection.Mode.BLUETOOTH) usbAutoConnectPaused = false
        setState(State.Connected(connectionMode, hostName))
        val shouldNavigate = synchronized(navigationLock) {
            if (inputNavigationRequested) {
                false
            } else {
                inputNavigationRequested = true
                true
            }
        }
        if (shouldNavigate) sendEvent(Event.NavigateToInput)
    }

    override fun onConnectionFailed(connectionMode: Connection.Mode) {
        if (connectionMode == Connection.Mode.WIFI) {
            setState(if (usbAutoConnectPaused) State.UsbPaused else State.Idle)
            sendEvent(Event.WifiModeDisabled)
            return
        }
        if (connectionMode == Connection.Mode.USB) {
            setState(State.Retrying("Waiting for USB/ADB to become available…", 1_000L))
            return
        }
        setState(if (usbAutoConnectPaused) State.UsbPaused else State.Idle)
        sendEvent(Event.ConnectionFailed(connectionMode))
    }

    override fun onDisconnected(connectionMode: Connection.Mode, hostName: String) {
        if (connectionMode == Connection.Mode.USB) {
            // USB disconnections are recovered by ConnectionManager's single
            // supervisor. Keep the current input screen and avoid popups.
            setState(State.Retrying("USB disconnected; reconnecting…", 500L))
            return
        }
        synchronized(navigationLock) { inputNavigationRequested = false }
        setState(State.Idle)
        sendEvent(Event.ConnectionDisconnected(connectionMode, hostName))
        sendEvent(Event.NavigateToMain)
    }

    override fun onUsbStatusChanged(status: ConnectionManager.UsbStatus) {
        if (usbAutoConnectPaused) return
        when (status) {
            is ConnectionManager.UsbStatus.WaitingForCable ->
                setState(State.WaitingForUsb("Connect the phone to this PC with USB. ADB authorization is still required."))
            is ConnectionManager.UsbStatus.Connecting ->
                setState(State.Connecting("Connecting over USB/ADB (attempt ${status.attempt})…"))
            is ConnectionManager.UsbStatus.Retrying ->
                setState(State.Retrying(status.message, status.delayMillis))
            is ConnectionManager.UsbStatus.Connected -> Unit
        }
    }

    /** Starts automatically when the app opens, and is safe to call repeatedly. */
    fun startServerMode(context: Context, userInitiated: Boolean = false) {
        if (userInitiated) usbAutoConnectPaused = false
        if (usbAutoConnectPaused) return
        connectionManager.startUsbAutoConnect(
            context.applicationContext,
            6969,
            getDeviceDetails(Connection.Mode.USB),
        )
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun startBluetoothMode(context: Context, afterEnableIntent: Boolean = false) {
        if (afterEnableIntent || BluetoothAdapterWrapper.getInstance()?.isEnabled == true) {
            connectionManager.registerBluetoothHID(context.applicationContext)
            sendEvent(Event.NavigateToDeviceList(Connection.Mode.BLUETOOTH))
        } else {
            sendEvent(Event.EnableBluetooth)
        }
    }

    /** Called only for an explicit user disconnect. */
    fun disconnect() {
        usbAutoConnectPaused = true
        viewModelScope.launch {
            connectionManager.disconnect()
            synchronized(navigationLock) { inputNavigationRequested = false }
            setState(State.UsbPaused)
            sendEvent(Event.NavigateToMain)
        }
    }

    override fun onCleared() {
        connectionManager.disconnect()
        connectionManager.clearConnectionStateCallback(this)
        super.onCleared()
    }
}
