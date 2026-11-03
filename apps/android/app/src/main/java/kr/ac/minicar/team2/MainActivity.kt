package kr.ac.minicar.team2

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.widget.Button
import android.widget.GridLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import java.util.UUID
import java.util.concurrent.Executors
import kotlin.math.roundToInt

// HC-06(클래식 블루투스, SPP) 조종 앱. 교안 2팀 STEP 4 앱과 같은 동작:
// 누르는 동안 200ms마다 명령 반복, 떼면 's', 모드 버튼은 'a'/'m'.
// 상태 표시는 차가 200ms마다 보내는 줄("ag,23,010,LINE")로만 바꾼다.
// 연결은 아이폰 앱처럼 "기기 찾기" → 주변 기기 목록에서 고르기. HC- 로 시작하는 기기는 ★ 표시하고 맨 위에.
@SuppressLint("MissingPermission", "ClickableViewAccessibility", "SetTextI18n")
class MainActivity : Activity() {
    private val spp = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private val io = Executors.newSingleThreadExecutor()
    private val ui = Handler(Looper.getMainLooper())
    private var adapter: BluetoothAdapter? = null
    @Volatile private var socket: BluetoothSocket? = null
    private var held: Char? = null
    private var auto = false
    private val rxBuf = StringBuilder()
    private val found = LinkedHashMap<String, Pair<BluetoothDevice, Short>>()  // 주소 -> 기기, 신호 세기
    private lateinit var status: TextView
    private lateinit var scanBtn: Button
    private lateinit var disconnectBtn: Button
    private lateinit var listBox: LinearLayout   // 찾은 기기 목록. 비어 있으면 숨김
    private lateinit var modeLabel: TextView
    private lateinit var distLabel: TextView
    private lateinit var modeBtn: Button

    // PID 설정 (버전 C). 슬라이더 값 = 실제 값 x scale
    private class PidRow(val key: Char, val name: String, val max: Int, val scale: Float) {
        lateinit var bar: SeekBar
        lateinit var label: TextView
        fun text() = if (scale == 1f) bar.progress.toString() else "%.1f".format(bar.progress / scale)
    }
    private val pidRows = listOf(
        PidRow('P', "Kp", 200, 1f), PidRow('I', "Ki", 50, 10f),
        PidRow('D', "Kd", 300, 1f), PidRow('B', "기본 속도", 255, 1f),
    )

    private val actNames = mapOf(
        'g' to "직진", 'b' to "후진", 'l' to "좌회전", 'r' to "우회전",
        'q' to "제자리 좌회전", 'w' to "제자리 우회전", 's' to "정지", 'o' to "장애물 정지",
    )

    private var tick = 0
    private val repeat = object : Runnable {
        override fun run() {
            held?.let { if (it != 's') send(it) }
            // 0.6초마다 'h': 차가 "앱이 연결되어 있다"를 알 수 있게 (주행에는 영향 없음)
            if (++tick % 3 == 0) send('h')
            ui.postDelayed(this, 200)
        }
    }

    // 주변 기기 찾기 결과
    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            when (i.action) {
                BluetoothDevice.ACTION_FOUND -> {
                    val d: BluetoothDevice = (if (Build.VERSION.SDK_INT >= 33)
                        i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                    else @Suppress("DEPRECATION") i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)) ?: return
                    found[d.address] = d to i.getShortExtra(BluetoothDevice.EXTRA_RSSI, Short.MIN_VALUE)
                    showList()
                }
                BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                    scanBtn.text = "기기 찾기"
                    scanBtn.isEnabled = true
                    if (socket == null) status.text = if (found.isEmpty()) "찾은 기기가 없습니다" else "연결할 기기를 누르세요"
                }
            }
        }
    }

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        adapter = getSystemService(BluetoothManager::class.java)?.adapter
        registerReceiver(receiver, IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
        })
        val dp = resources.displayMetrics.density
        val pad = (16 * dp).toInt()

        status = TextView(this).apply {
            text = "기기 찾기를 누르세요"; textSize = 16f
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        scanBtn = Button(this).apply { text = "기기 찾기"; setOnClickListener { scan() } }
        disconnectBtn = Button(this).apply { text = "끊기"; visibility = View.GONE; setOnClickListener { close() } }
        listBox = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; visibility = View.GONE }
        modeLabel = TextView(this).apply { text = "수동 · 정지"; textSize = 22f }
        distLabel = TextView(this).apply { textSize = 16f; gravity = Gravity.CENTER_HORIZONTAL }
        modeBtn = Button(this).apply { text = "자동으로"; setOnClickListener { send(if (auto) 'm' else 'a') } }

        // PID 패널 : 슬라이더에서 손을 떼면 "#P60" 처럼 차로 보냄 (버전 C 에서만 반응)
        val pidBox = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; visibility = View.GONE }
        for (r in pidRows) {
            r.label = TextView(this).apply { text = "${r.name}  -"; textSize = 15f }
            r.bar = SeekBar(this).apply {
                max = r.max
                setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                    override fun onProgressChanged(s: SeekBar, p: Int, fromUser: Boolean) { r.label.text = "${r.name}  ${r.text()}" }
                    override fun onStartTrackingTouch(s: SeekBar) {}
                    override fun onStopTrackingTouch(s: SeekBar) { sendText("#${r.key}${r.text()}\n") }
                })
            }
            pidBox.addView(r.label)
            pidBox.addView(r.bar, LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT)
        }
        val pidToggle = Button(this).apply {
            text = "PID 설정 ▼"
            setOnClickListener {
                val show = pidBox.visibility != View.VISIBLE
                pidBox.visibility = if (show) View.VISIBLE else View.GONE
                text = if (show) "PID 설정 ▲" else "PID 설정 ▼"
                if (show) sendText("#?\n")   // 차에 저장된 지금 값 받아 오기
            }
        }

        val grid = GridLayout(this).apply { columnCount = 3 }
        listOf('q' to "↺", 'g' to "▲", 'w' to "↻",
               'l' to "◀", 's' to "■", 'r' to "▶",
               null to "", 'b' to "▼", null to "").forEach { (c, label) ->
            grid.addView(key(c, label, (88 * dp).toInt(), (6 * dp).toInt()))
        }

        setContentView(ScrollView(this).apply {
            // 안드로이드 15부터는 화면이 상태바 밑까지 깔려서, 그만큼 띄우지 않으면 맨 위가 가려진다
            if (Build.VERSION.SDK_INT >= 30) setOnApplyWindowInsetsListener { v, insets ->
                val b = insets.getInsets(WindowInsets.Type.systemBars())
                v.setPadding(b.left, b.top, b.right, b.bottom)
                insets
            }
            addView(LinearLayout(this@MainActivity).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(pad, pad, pad, pad)
            addView(LinearLayout(this@MainActivity).apply {
                gravity = Gravity.CENTER_VERTICAL
                addView(status)
                addView(scanBtn)
                addView(disconnectBtn)
            })
            addView(listBox, LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT)
            addView(LinearLayout(this@MainActivity).apply {
                gravity = Gravity.CENTER_VERTICAL
                setPadding(0, pad, 0, pad)
                addView(modeLabel)
                addView(modeBtn)
            })
            addView(grid)
            addView(distLabel)
            addView(pidToggle)
            addView(pidBox, LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT)
        }) })
        ui.post(repeat)
    }

    private fun key(c: Char?, label: String, size: Int, margin: Int) = Button(this).apply {
        text = label
        textSize = 28f
        layoutParams = GridLayout.LayoutParams().apply {
            width = size; height = size; setMargins(margin, margin, margin, margin)
        }
        if (c == null) { visibility = View.INVISIBLE; return@apply }
        setOnTouchListener { v, e ->
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN -> { held = c; send(c); v.isPressed = true }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> { release(); v.isPressed = false }
            }
            true
        }
    }

    private fun release() {
        if (held == null) return
        held = null
        // 차가 상태 줄을 보내는 순간에는 받은 글자를 놓칠 수 있어서 정지는 두 번 보낸다
        send('s')
        send('s')
    }

    private fun send(c: Char) {
        val s = socket ?: return
        io.execute { try { s.outputStream.write(c.code) } catch (_: Exception) {} }
    }

    private fun sendText(t: String) {
        val s = socket ?: return
        io.execute { try { s.outputStream.write(t.toByteArray()) } catch (_: Exception) {} }
    }

    // ---- 기기 찾기 ----
    private fun scan() {
        val need = if (Build.VERSION.SDK_INT >= 31)
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)   // 안드로이드 11 이하는 주변 기기 찾기에 위치 권한이 필요
        val missing = need.filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) { requestPermissions(missing.toTypedArray(), 1); return }
        val a = adapter ?: run { status.text = "이 기기에는 블루투스가 없습니다"; return }
        if (!a.isEnabled) { status.text = "블루투스를 켜세요"; return }
        found.clear()
        a.bondedDevices.forEach { found[it.address] = it to Short.MIN_VALUE }   // 이미 페어링된 기기도 같이 보여 줌
        showList()
        a.cancelDiscovery()
        a.startDiscovery()
        scanBtn.text = "찾는 중…"
        scanBtn.isEnabled = false
        status.text = "주변 기기를 찾는 중 (약 12초)"
    }

    override fun onRequestPermissionsResult(code: Int, perms: Array<out String>, results: IntArray) {
        if (results.isNotEmpty() && results.all { it == PackageManager.PERMISSION_GRANTED }) scan()
        else status.text = "근처 기기 권한을 허용해야 찾을 수 있습니다"
    }

    private fun isCar(d: BluetoothDevice) = d.name?.let { it.startsWith("HC-") || it.contains("linvor", true) } == true

    // HC- 기기를 맨 위로, 그다음 신호가 센(가까운) 순서
    private fun showList() {
        listBox.removeAllViews()
        val items = found.values.sortedWith(
            compareByDescending<Pair<BluetoothDevice, Short>> { isCar(it.first) }.thenByDescending { it.second })
        for ((d, rssi) in items) {
            val tags = listOfNotNull(
                if (d.bondState == BluetoothDevice.BOND_BONDED) "페어링됨" else null,
                if (rssi != Short.MIN_VALUE) "신호 ${rssi}dBm" else null,
            ).joinToString(" · ")
            listBox.addView(Button(this).apply {
                isAllCaps = false
                gravity = Gravity.START or Gravity.CENTER_VERTICAL
                text = (if (isCar(d)) "★ " else "") + (d.name ?: "(이름 없음)") + "\n" + d.address +
                    (if (tags.isEmpty()) "" else "  $tags")
                setOnClickListener { connect(d) }
            })
        }
        listBox.visibility = if (items.isEmpty() || socket != null) View.GONE else View.VISIBLE
    }

    private fun showConnected(connected: Boolean) {
        listBox.visibility = if (connected || found.isEmpty()) View.GONE else View.VISIBLE
        scanBtn.visibility = if (connected) View.GONE else View.VISIBLE
        disconnectBtn.visibility = if (connected) View.VISIBLE else View.GONE
    }

    // ---- 연결 ----
    private fun connect(d: BluetoothDevice) {
        try { adapter?.cancelDiscovery() } catch (_: Exception) {}
        close()
        status.text = "연결 중… (처음이면 PIN 1234 입력)"
        Thread {
            var msg = "연결 실패: 차 전원과 이름을 확인하세요"
            var s: BluetoothSocket? = null
            try {
                s = try {
                    d.createRfcommSocketToServiceRecord(spp).also { it.connect() }
                } catch (_: Exception) {
                    d.createInsecureRfcommSocketToServiceRecord(spp).also { it.connect() }   // 일부 폰은 이쪽만 됨
                }
                socket = s
                msg = "연결 끊김"
                ui.post { status.text = "연결: ${d.name}"; showConnected(true) }
                send('m')  // 연결 직후 수동으로 맞춘다
                sendText("#?\n")  // 버전 C 면 PID 값을 돌려줌 (A, B 는 무시)
                val input = s!!.inputStream
                while (true) {
                    val b = input.read()
                    if (b < 0) break
                    ui.post { onByte(b.toChar()) }
                }
            } catch (_: Exception) {}
            val mine = s
            ui.post {
                if (socket == null || socket === mine) {   // 다른 기기에 새로 연결한 경우는 건드리지 않음
                    socket = null; held = null
                    status.text = msg
                    showConnected(false)
                }
            }
        }.start()
    }

    private fun onByte(c: Char) {
        when (c) {
            '\n' -> { onLine(rxBuf.toString()); rxBuf.clear() }
            '\r' -> {}
            else -> if (rxBuf.length < 64) rxBuf.append(c) else rxBuf.clear()
        }
    }

    private val stageNames = mapOf(
        "MAN" to "수동 조종", "HOLD" to "전환 대기 (0.5초)", "CAL" to "회전 시간 측정",
        "LINE" to "라인 추종", "CORNER" to "코너 회전", "NOLINE" to "선 놓침",
        "DEAD" to "데드존: 선 찾는 중", "SWAY" to "좌우로 흔들며 선 찾기", "LOST" to "선 못 찾음 (정지)",
        "OBST" to "장애물 대기", "CROSS" to "교차선 통과", "END" to "선 끝 (정지)",
    )

    // 차가 돌려준 PID 값 "#P60.0,I0.00,D40.0,B140" → 슬라이더에 반영
    private fun onPidLine(line: String) {
        for (part in line.drop(1).split(',')) {
            val r = pidRows.find { it.key == part.firstOrNull() } ?: continue
            val v = part.drop(1).toFloatOrNull() ?: continue
            r.bar.progress = (v * r.scale).roundToInt().coerceIn(0, r.max)
        }
    }

    // "ag,23,010,LINE,1,60" → 모드 a, 동작 g, 거리 23, 라인센서 L C R, 단계, 오차, 보정 (뒤쪽 칸은 없을 수도 있음)
    private fun onLine(line: String) {
        if (line.startsWith("#")) { onPidLine(line); return }
        val f = line.split(',')
        if (f[0].length < 2 || (f[0][0] != 'a' && f[0][0] != 'm')) return
        auto = f[0][0] == 'a'
        modeLabel.text = (if (auto) "자동" else "수동") + " · " + (actNames[f[0][1]] ?: "정지")
        modeBtn.text = if (auto) "수동으로" else "자동으로"
        val d = f.getOrNull(1)?.toIntOrNull()
        val dist = when {
            d == null -> ""
            d >= 999 -> "앞: 측정 안 됨"
            else -> "앞: ${d}cm"
        }
        val sensors = f.getOrNull(2)?.let { s -> "센서 " + s.map { if (it == '1') "●" else "○" }.joinToString(" ") } ?: ""
        val stage = f.getOrNull(3)?.let { s -> if (s.startsWith("AV")) "회피 ${s.drop(2)}단계" else stageNames[s] ?: s } ?: ""
        val pid = if (f.size > 5) "오차 ${f[4]}  ·  보정 ${f[5]}" else ""
        distLabel.text = listOf(stage, listOf(dist, sensors).filter { it.isNotEmpty() }.joinToString("  ·  "), pid)
            .filter { it.isNotEmpty() }.joinToString("\n")
    }

    private fun close() {
        try { socket?.close() } catch (_: Exception) {}
        socket = null
    }

    // 앱이 뒤로 가면 누르던 버튼을 놓은 것으로 처리
    override fun onPause() {
        super.onPause()
        release()
    }

    override fun onDestroy() {
        ui.removeCallbacks(repeat)
        try { adapter?.cancelDiscovery() } catch (_: Exception) {}
        unregisterReceiver(receiver)
        close()
        io.shutdown()
        super.onDestroy()
    }
}
