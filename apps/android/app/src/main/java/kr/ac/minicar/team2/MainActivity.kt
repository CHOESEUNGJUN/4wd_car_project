package kr.ac.minicar.team2

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.app.AlertDialog
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.GridLayout
import android.widget.LinearLayout
import android.widget.TextView
import java.util.UUID
import java.util.concurrent.Executors

// HC-06(클래식 블루투스, SPP) 조종 앱. 교안 2팀 STEP 4 앱과 같은 동작:
// 누르는 동안 200ms마다 명령 반복, 떼면 's', 모드 버튼은 'a'/'m'.
// 상태 표시는 차가 200ms마다 보내는 줄("ag,23" = 자동, 직진, 23cm)로만 바꾼다.
@SuppressLint("MissingPermission", "ClickableViewAccessibility", "SetTextI18n")
class MainActivity : Activity() {
    private val spp = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private val io = Executors.newSingleThreadExecutor()
    private val ui = Handler(Looper.getMainLooper())
    @Volatile private var socket: BluetoothSocket? = null
    private var held: Char? = null
    private var auto = false
    private val rxBuf = StringBuilder()
    private lateinit var status: TextView
    private lateinit var modeLabel: TextView
    private lateinit var distLabel: TextView
    private lateinit var modeBtn: Button

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

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        val dp = resources.displayMetrics.density
        val pad = (16 * dp).toInt()

        status = TextView(this).apply { text = "연결 안 됨"; textSize = 16f }
        val connect = Button(this).apply { text = "HC-06 연결"; setOnClickListener { pickDevice() } }
        modeLabel = TextView(this).apply { text = "수동 · 정지"; textSize = 22f }
        distLabel = TextView(this).apply { textSize = 16f }
        modeBtn = Button(this).apply { text = "자동으로"; setOnClickListener { send(if (auto) 'm' else 'a') } }

        val grid = GridLayout(this).apply { columnCount = 3 }
        listOf('q' to "↺", 'g' to "▲", 'w' to "↻",
               'l' to "◀", 's' to "■", 'r' to "▶",
               null to "", 'b' to "▼", null to "").forEach { (c, label) ->
            grid.addView(key(c, label, (88 * dp).toInt(), (6 * dp).toInt()))
        }

        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(pad, pad, pad, pad)
            addView(connect)
            addView(status)
            addView(LinearLayout(this@MainActivity).apply {
                gravity = Gravity.CENTER_VERTICAL
                setPadding(0, pad, 0, pad)
                addView(modeLabel)
                addView(modeBtn)
            })
            addView(grid)
            addView(distLabel)
        })
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

    private fun pickDevice() {
        if (Build.VERSION.SDK_INT >= 31 &&
            checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.BLUETOOTH_CONNECT), 1)
            return
        }
        val adapter = getSystemService(BluetoothManager::class.java)?.adapter
        if (adapter == null || !adapter.isEnabled) { status.text = "블루투스를 켜세요"; return }
        val devices = adapter.bondedDevices.toList()
        if (devices.isEmpty()) { status.text = "폰 설정에서 HC-06을 먼저 페어링하세요 (1234 또는 0000)"; return }
        AlertDialog.Builder(this)
            .setTitle("페어링된 기기")
            .setItems(devices.map { "${it.name}\n${it.address}" }.toTypedArray()) { _, i -> connect(devices[i]) }
            .show()
    }

    override fun onRequestPermissionsResult(code: Int, perms: Array<out String>, results: IntArray) {
        if (results.firstOrNull() == PackageManager.PERMISSION_GRANTED) pickDevice()
        else status.text = "근처 기기 권한을 허용해야 연결할 수 있습니다"
    }

    private fun connect(d: BluetoothDevice) {
        close()
        status.text = "연결 중…"
        Thread {
            var msg = "연결 실패"
            try {
                val s = d.createRfcommSocketToServiceRecord(spp)
                s.connect()
                socket = s
                msg = "연결 끊김"
                ui.post { status.text = "연결: ${d.name}" }
                send('m')  // 연결 직후 수동으로 맞추고, 차가 돌려주는 'm'으로 표시 동기화
                val input = s.inputStream
                while (true) {
                    val b = input.read()
                    if (b < 0) break
                    ui.post { onByte(b.toChar()) }
                }
            } catch (_: Exception) {}
            ui.post { status.text = msg; socket = null; held = null }
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
        "OBST" to "장애물 대기",
    )

    // "ag,23,010,LINE" → 모드 a, 동작 g, 거리 23, 라인센서 L C R, 단계 (뒤 두 칸은 없을 수도 있음)
    private fun onLine(line: String) {
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
        distLabel.text = listOf(stage, listOf(dist, sensors).filter { it.isNotEmpty() }.joinToString("  ·  "))
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
        close()
        io.shutdown()
        super.onDestroy()
    }
}
