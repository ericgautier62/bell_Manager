package cm.dgcast.kairoslab

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.ParcelUuid
import android.webkit.JavascriptInterface
import android.webkit.WebChromeClient
import android.webkit.WebView
import android.webkit.WebViewClient
import java.nio.charset.StandardCharsets
import java.util.ArrayDeque
import java.util.UUID

private data class BleRpc(val id: String, val method: String, val path: String, val body: ByteArray)

class MainActivity : Activity() {
    private lateinit var web: WebView
    private val ble by lazy { DeviceBle(this) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        web = WebView(this)
        web.settings.javaScriptEnabled = true
        web.settings.domStorageEnabled = true
        web.webViewClient = WebViewClient()
        web.webChromeClient = WebChromeClient()
        web.addJavascriptInterface(Bridge(), "KairosBle")
        setContentView(web)
        web.loadUrl("file:///android_asset/index.html")
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISSION_REQUEST && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) ble.scanAndConnect()
    }

    private fun hasBlePermissions(): Boolean {
        val required = if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        return required.all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
    }

    private fun requestBlePermissions() {
        val required = if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        requestPermissions(required, PERMISSION_REQUEST)
    }

    private fun jsState(connected: Boolean) {
        runOnUiThread { web.evaluateJavascript("window.onKairosBleState && window.onKairosBleState($connected)", null) }
    }

    private fun jsResponse(id: String, json: String) {
        val quoted = org.json.JSONObject.quote(json)
        runOnUiThread { web.evaluateJavascript("window.onKairosBleResponse && window.onKairosBleResponse(${org.json.JSONObject.quote(id)}, $quoted)", null) }
    }

    inner class Bridge {
        @JavascriptInterface fun isConnected(): Boolean = ble.connected
        @JavascriptInterface fun connect() {
            runOnUiThread { if (hasBlePermissions()) ble.scanAndConnect() else requestBlePermissions() }
        }
        @JavascriptInterface fun disconnect() = ble.disconnect()
        @JavascriptInterface fun request(id: String, method: String, path: String, body: String) {
            ble.request(id, method, path, body)
        }
    }

    private inner class DeviceBle(private val context: Context) {
        private val serviceId = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        private val controlId = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
        private val inputId = UUID.fromString("6e400004-b5a3-f393-e0a9-e50e24dcca9e")
        private val outputId = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
        private val cccdId = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private val manager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        private val adapter: BluetoothAdapter? get() = manager.adapter
        @Volatile var connected = false
            private set
        private var gatt: BluetoothGatt? = null
        private var output: BluetoothGattCharacteristic? = null
        private var control: BluetoothGattCharacteristic? = null
        private var input: BluetoothGattCharacteristic? = null
        private var mtu = 23
        private val rpcQueue = ArrayDeque<BleRpc>()
        private var active: BleRpc? = null
        private val writes = ArrayDeque<Pair<UUID, ByteArray>>()
        private var writeBusy = false
        private var response = ByteArray(0)
        private var expectedResponse = 0

        fun scanAndConnect() {
            if (!hasBlePermissions()) { requestBlePermissions(); return }
            val a = adapter ?: return
            if (!a.isEnabled) { jsState(false); return }
            gatt?.close(); gatt = null
            val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(serviceId)).build()
            val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
            a.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
            android.os.Handler(mainLooper).postDelayed({ a.bluetoothLeScanner?.stopScan(scanCallback) }, 12000)
        }

        private val scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                adapter?.bluetoothLeScanner?.stopScan(this)
                gatt = if (Build.VERSION.SDK_INT >= 23) result.device.connectGatt(this@MainActivity, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
                else result.device.connectGatt(this@MainActivity, false, gattCallback)
            }
            override fun onScanFailed(errorCode: Int) { jsState(false) }
        }

        private val gattCallback = object : BluetoothGattCallback() {
            override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                    if (Build.VERSION.SDK_INT >= 21) g.requestMtu(247)
                    g.discoverServices()
                } else {
                    connected = false; output = null; control = null; input = null
                    g.close(); if (gatt === g) gatt = null
                    jsState(false)
                }
            }
            override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) return
                val service = g.getService(serviceId) ?: return
                control = service.getCharacteristic(controlId); input = service.getCharacteristic(inputId); output = service.getCharacteristic(outputId)
                val out = output ?: return
                g.setCharacteristicNotification(out, true)
                val descriptor = out.getDescriptor(cccdId) ?: return
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                g.writeDescriptor(descriptor)
            }
            override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    connected = true; jsState(true)
                    pumpRpc()
                }
            }
            override fun onMtuChanged(g: BluetoothGatt, value: Int, status: Int) { if (status == BluetoothGatt.GATT_SUCCESS) mtu = value }
            override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
                writeBusy = false
                if (status != BluetoothGatt.GATT_SUCCESS) { failActive("write_failed"); return }
                pumpWrite()
            }
            override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                handleNotification(characteristic.value ?: return)
            }
        }

        @Synchronized fun request(id: String, method: String, path: String, body: String) {
            if (!connected) { jsResponse(id, "{\"ok\":false,\"error\":\"ble_disconnected\"}"); return }
            rpcQueue.add(BleRpc(id, method, path, body.toByteArray(StandardCharsets.UTF_8)))
            pumpRpc()
        }

        @Synchronized private fun pumpRpc() {
            if (!connected || active != null || rpcQueue.isEmpty()) return
            active = rpcQueue.removeFirst()
            val r = active!!
            val header = "REQ|${r.id}|${r.method}|${r.path}|${r.body.size}".toByteArray(StandardCharsets.UTF_8)
            queueWrite(controlId, header)
            pumpWrite()
        }

        @Synchronized private fun queueWrite(characteristicId: UUID, value: ByteArray) {
            writes.add(characteristicId to value)
        }

        @Synchronized private fun pumpWrite() {
            if (!connected || writeBusy || writes.isEmpty()) return
            val g = gatt ?: return
            val (id, value) = writes.removeFirst()
            val characteristic = g.getService(serviceId)?.getCharacteristic(id) ?: return
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            characteristic.value = value
            writeBusy = g.writeCharacteristic(characteristic)
            if (!writeBusy) failActive("write_rejected")
        }

        @Synchronized private fun handleNotification(value: ByteArray) {
            if (value.size >= 3 && value[0] == 0xD2.toByte()) {
                val offset = (value[1].toInt() and 255) or ((value[2].toInt() and 255) shl 8)
                val data = value.copyOfRange(3, value.size)
                if (offset + data.size <= expectedResponse) System.arraycopy(data, 0, response, offset, data.size)
                if (offset + data.size >= expectedResponse && expectedResponse > 0) finishActive(String(response, StandardCharsets.UTF_8))
                return
            }
            val message = String(value, StandardCharsets.UTF_8)
            when {
                message.startsWith("READY|") -> sendBodyChunks()
                message.startsWith("RESULT|") -> {
                    val parts = message.split('|'); expectedResponse = parts.getOrNull(2)?.toIntOrNull() ?: 0
                    response = ByteArray(expectedResponse)
                    if (expectedResponse == 0) finishActive("{\"ok\":true}")
                }
                message.startsWith("ERROR|") -> failActive(message.substringAfter('|'))
            }
        }

        @Synchronized private fun sendBodyChunks() {
            val r = active ?: return
            val chunkSize = (mtu - 6).coerceIn(16, 240)
            var offset = 0
            while (offset < r.body.size) {
                val count = minOf(chunkSize, r.body.size - offset)
                val frame = ByteArray(count + 3)
                frame[0] = 0xD1.toByte(); frame[1] = (offset and 255).toByte(); frame[2] = ((offset shr 8) and 255).toByte()
                System.arraycopy(r.body, offset, frame, 3, count)
                queueWrite(inputId, frame); offset += count
            }
            queueWrite(controlId, "COMMIT|${r.id}".toByteArray(StandardCharsets.UTF_8))
            pumpWrite()
        }

        @Synchronized private fun finishActive(json: String) {
            val r = active ?: return
            jsResponse(r.id, json)
            active = null; expectedResponse = 0; response = ByteArray(0)
            pumpRpc()
        }

        @Synchronized private fun failActive(error: String) {
            val r = active
            if (r != null) jsResponse(r.id, "{\"ok\":false,\"error\":${org.json.JSONObject.quote(error)}}")
            active = null; expectedResponse = 0; response = ByteArray(0); writes.clear(); writeBusy = false
            pumpRpc()
        }

        fun disconnect() {
            if (hasBlePermissions()) gatt?.disconnect()
            connected = false; jsState(false)
        }
    }

    companion object { private const val PERMISSION_REQUEST = 42 }
}
