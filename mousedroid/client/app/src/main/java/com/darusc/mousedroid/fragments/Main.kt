package com.darusc.mousedroid.fragments

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresPermission
import androidx.core.os.bundleOf
import androidx.databinding.DataBindingUtil
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.fragment.findNavController
import com.darusc.mousedroid.R
import com.darusc.mousedroid.databinding.FragmentMainBinding
import com.darusc.mousedroid.networking.Connection
import com.darusc.mousedroid.viewmodels.ConnectionViewModel
import kotlinx.coroutines.launch

class Main : Fragment() {

    private lateinit var binding: FragmentMainBinding
    private val viewModel: ConnectionViewModel by activityViewModels()

    @SuppressLint("MissingPermission")
    private val enableBluetoothLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            if (it.resultCode == Activity.RESULT_OK) {
                viewModel.startBluetoothMode(requireContext(), true)
            } else {
                Toast.makeText(context, "Bluetooth is required for this mode.", Toast.LENGTH_SHORT).show()
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        binding = DataBindingUtil.inflate(inflater, R.layout.fragment_main, container, false)
        binding.lifecycleOwner = viewLifecycleOwner
        return binding.root
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.btnConnectSV.setOnClickListener {
            viewModel.startServerMode(requireContext(), userInitiated = true)
        }
        binding.btnConnectBT.setOnClickListener { viewModel.startBluetoothMode(requireContext()) }
        binding.btnPairBT.setOnClickListener {
            viewModel.startBluetoothMode(requireContext())
            val intent = Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE).apply {
                putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 120)
            }
            startActivity(intent)
        }

        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) {
                launch {
                    viewModel.state.collect { state ->
                        binding.usbStatus.text = when (state) {
                            is ConnectionViewModel.State.WaitingForUsb -> state.message
                            is ConnectionViewModel.State.Connecting -> state.message
                            is ConnectionViewModel.State.Retrying ->
                                "${state.message} Next check in ${((state.delayMillis + 999) / 1_000)}s."
                            is ConnectionViewModel.State.Connected -> when (state.connectionMode) {
                                Connection.Mode.USB -> "Connected over USB to ${state.hostName}."
                                Connection.Mode.BLUETOOTH -> "Bluetooth HID connected to ${state.hostName}."
                                Connection.Mode.WIFI -> "Wi-Fi mode is disabled until secure pairing is available."
                            }
                            is ConnectionViewModel.State.Idle ->
                                "USB connects automatically when available. Wi-Fi mode is disabled for safety."
                            is ConnectionViewModel.State.UsbPaused ->
                                "USB auto-connect is paused. Tap Connect USB to resume; Wi-Fi remains disabled."
                        }
                    }
                }

                launch {
                    viewModel.events.collect { event ->
                        when (event) {
                            is ConnectionViewModel.Event.NavigateToInput ->
                                findNavController().navigate(R.id.action_main_to_touchpad)

                            is ConnectionViewModel.Event.NavigateToDeviceList ->
                                findNavController().navigate(
                                    R.id.action_main_to_devicelist,
                                    bundleOf("CONNECTION_MODE" to event.mode),
                                )

                            is ConnectionViewModel.Event.NavigateToMain -> Unit

                            is ConnectionViewModel.Event.ConnectionDisconnected -> {
                                if (event.connectionMode == Connection.Mode.BLUETOOTH) {
                                    val popup = showPopupDialog(R.layout.connection_disconnected_fragment)
                                    popup?.apply {
                                        findViewById<TextView>(R.id.subtitle).text =
                                            "Bluetooth connection to ${event.hostName} was terminated"
                                        findViewById<TextView>(R.id.description).text =
                                            "The host device turned Bluetooth off or disconnected this phone."
                                    }
                                }
                            }

                            is ConnectionViewModel.Event.ConnectionFailed -> {
                                if (event.connectionMode == Connection.Mode.BLUETOOTH) {
                                    val popup = showPopupDialog(R.layout.connection_failed_fragment)
                                    popup?.apply {
                                        findViewById<TextView>(R.id.subtitle).text = "Bluetooth connection failed"
                                        findViewById<TextView>(R.id.description).text = "Make sure the device is on and paired."
                                    }
                                }
                            }

                            is ConnectionViewModel.Event.WifiModeDisabled ->
                                Toast.makeText(
                                    requireContext(),
                                    "Wi-Fi server mode is disabled until authenticated pairing is implemented.",
                                    Toast.LENGTH_LONG,
                                ).show()

                            is ConnectionViewModel.Event.EnableBluetooth -> {
                                val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                                enableBluetoothLauncher.launch(intent)
                            }

                            else -> Unit
                        }
                    }
                }
            }
        }

        // USB/ADB is the default mode: start monitoring immediately rather than
        // requiring a button press or a one-time USB-state snapshot.
        viewModel.startServerMode(requireContext())
    }
}
