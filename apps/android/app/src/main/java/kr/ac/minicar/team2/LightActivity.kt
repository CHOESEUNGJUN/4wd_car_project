// SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
package kr.ac.minicar.team2

import android.app.Activity
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

// 폰을 신호등으로 (ESP32-CAM 신호등 인식 시험용, 아이폰 앱의 "신호등 화면" 과 같음)
//  - 신호등 : 화면 전체가 초록 -> 노랑 -> 빨강 -> 초록 ... (순수 #00ff00 / #ffff00 / #ff0000, ESP 색 기준에 맞춤).
//    화면을 누르면 바로 다음 색, "자동 n초" 마다 저절로 (0 = 끔)
//  - 정지 표지판 : 검은 바탕에 흰 테두리 + 빨간 팔각형 + 흰 STOP (ESP 는 3초 서 있다가 출발)
//  - 켜져 있는 동안 화면 안 꺼짐, 밝기 최대
class LightActivity : Activity() {
    private val lights = intArrayOf(Color.rgb(0, 255, 0), Color.rgb(255, 255, 0), Color.rgb(255, 0, 0))
    private var k = 2              // 빨강부터
    private var secs = 5
    private var last = SystemClock.uptimeMillis()
    private var sign = false
    private lateinit var face: Face
    private lateinit var autoLabel: TextView
    private lateinit var modeBtn: Button
    private val ui = Handler(Looper.getMainLooper())
    private val tick = object : Runnable {
        override fun run() {
            if (!sign && secs > 0 && SystemClock.uptimeMillis() - last >= secs * 1000L) flip()
            ui.postDelayed(this, 250)
        }
    }

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.attributes = window.attributes.apply { screenBrightness = 1f }   // 밝기 최대 (이 화면만)
        @Suppress("DEPRECATION")
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        val dp = resources.displayMetrics.density
        face = Face().apply { setOnClickListener { if (!sign) flip() } }
        autoLabel = TextView(this).apply { setTextColor(Color.WHITE); textSize = 16f }
        val minus = Button(this).apply { text = "−"; setOnClickListener { secs = maxOf(0, secs - 1); last = SystemClock.uptimeMillis(); showAuto() } }
        val plus = Button(this).apply { text = "+"; setOnClickListener { secs = minOf(30, secs + 1); last = SystemClock.uptimeMillis(); showAuto() } }
        modeBtn = Button(this).apply { setOnClickListener { sign = !sign; last = SystemClock.uptimeMillis(); showAuto(); face.invalidate() } }
        val close = Button(this).apply { text = "닫기"; setOnClickListener { finish() } }
        val bar = LinearLayout(this).apply {
            gravity = Gravity.CENTER_VERTICAL
            setBackgroundColor(Color.argb(150, 0, 0, 0))
            val p = (10 * dp).toInt()
            setPadding(p, p / 2, p, p / 2)
            addView(autoLabel); addView(minus); addView(plus); addView(modeBtn); addView(close)
        }
        setContentView(FrameLayout(this).apply {
            addView(face, FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
            addView(bar, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL).apply { bottomMargin = (24 * dp).toInt() })
        })
        showAuto()
    }

    private fun showAuto() {
        autoLabel.text = if (sign) "" else "자동 ${if (secs == 0) "끔" else "${secs}초"} "
        autoLabel.visibility = if (sign) View.GONE else View.VISIBLE
        modeBtn.text = if (sign) "신호등" else "정지 표지판"
    }

    private fun flip() { k = (k + 1) % lights.size; last = SystemClock.uptimeMillis(); face.invalidate() }

    override fun onResume() { super.onResume(); ui.post(tick) }
    override fun onPause() { super.onPause(); ui.removeCallbacks(tick) }

    // 화면 그리기 : 신호등이면 한 색으로 꽉 채우고, 표지판이면 팔각형 + STOP
    private inner class Face : View(this@LightActivity) {
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private fun octagon(cx: Float, cy: Float, half: Float): Path {   // 변까지 거리 = half, 위·아래·옆 변이 평평하게
            val r = half / cos(Math.PI / 8).toFloat()
            return Path().apply {
                for (i in 0 until 8) {
                    val a = i * Math.PI / 4 + Math.PI / 8
                    val x = cx + r * cos(a).toFloat(); val y = cy + r * sin(a).toFloat()
                    if (i == 0) moveTo(x, y) else lineTo(x, y)
                }
                close()
            }
        }
        override fun onDraw(c: Canvas) {
            if (!sign) { c.drawColor(lights[k]); return }
            c.drawColor(Color.BLACK)
            val cx = width / 2f; val cy = height / 2f; val half = min(width, height) / 2f - 20 * resources.displayMetrics.density
            paint.style = Paint.Style.FILL
            paint.color = Color.WHITE; c.drawPath(octagon(cx, cy, half), paint)
            paint.color = Color.rgb(255, 0, 0); c.drawPath(octagon(cx, cy, half * 0.9f), paint)
            paint.color = Color.WHITE
            paint.typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
            paint.textAlign = Paint.Align.CENTER
            paint.textSize = half * 0.55f
            c.drawText("STOP", cx, cy - (paint.descent() + paint.ascent()) / 2, paint)
        }
    }
}
