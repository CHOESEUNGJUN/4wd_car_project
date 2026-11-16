package kr.ac.minicar.team2

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.WindowInsets
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

// ESP32-CAM 카메라 화면 (웹 화면을 앱 안에서). 안드로이드 크롬은 gulno-cam.local 같은 .local 주소를 못 찾을 때가 많아서,
// 안드로이드가 직접 같은 와이파이(핫스팟)에서 ESP32 가 알리는 이름 "gulno-cam" (_http._tcp, mDNS) 을 찾아 그 주소로 연다.
// 못 찾으면 gulno-cam.local 로 한 번 더 시도. 폰과 ESP32 가 같은 핫스팟에 붙어 있어야 함
class CamActivity : Activity() {
    private lateinit var web: WebView
    private lateinit var info: TextView
    private var nsd: NsdManager? = null
    private var lock: WifiManager.MulticastLock? = null
    private var listener: NsdManager.DiscoveryListener? = null
    private val ui = Handler(Looper.getMainLooper())
    private var found = false
    private var triedAp = false
    private var failed = false

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        val dp = resources.displayMetrics.density
        info = TextView(this).apply {
            textSize = 15f
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        val again = Button(this).apply { text = "다시 찾기"; setOnClickListener { find() } }
        val close = Button(this).apply { text = "닫기"; setOnClickListener { finish() } }
        web = WebView(this).apply {
            settings.javaScriptEnabled = true          // 웹 화면이 자바스크립트·웹소켓을 씀
            settings.domStorageEnabled = true
            webViewClient = object : WebViewClient() {
                override fun onPageFinished(v: WebView, url: String) { if (!failed) info.text = "연결됨 : $url" }
                // 이름으로 못 열면 마지막으로 직접 연결 주소 (폰이 ESP 와이파이 GULNO-CAM-xxxx 에 붙어 있을 때)
                override fun onReceivedError(v: WebView, req: WebResourceRequest, err: WebResourceError) {
                    if (!req.isForMainFrame) return
                    if (!triedAp) { triedAp = true; info.text = "못 열음 → http://192.168.4.1 (ESP 와이파이에 붙어 있을 때)"; v.loadUrl("http://192.168.4.1/") }
                    else { failed = true; info.text = "카메라를 못 찾음 : 같은 핫스팟에 붙거나, 와이파이 GULNO-CAM-xxxx 에 붙고 다시 찾기" }
                }
            }
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            if (Build.VERSION.SDK_INT >= 30) setOnApplyWindowInsetsListener { v, insets ->   // 상태바·내비게이션 바 밑에 깔리지 않게
                val b = insets.getInsets(WindowInsets.Type.systemBars())
                v.setPadding(b.left, b.top, b.right, b.bottom)
                insets
            }
            addView(LinearLayout(this@CamActivity).apply {
                gravity = Gravity.CENTER_VERTICAL
                val p = (8 * dp).toInt()
                setPadding(p, p, p, p)
                addView(info); addView(again); addView(close)
            })
            addView(web, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        })
        nsd = getSystemService(Context.NSD_SERVICE) as NsdManager
        find()
    }

    // 같은 와이파이에서 "gulno-cam" 을 찾음 (8초). 못 찾으면 gulno-cam.local 로
    private fun find() {
        stopFind()
        found = false; triedAp = false; failed = false
        info.text = "카메라 찾는 중… (폰과 카메라가 같은 핫스팟인지 확인)"
        // 와이파이 멀티캐스트 받기 (mDNS 가 이걸로 옴. 절전 때문에 꺼져 있는 폰이 있음)
        lock = (applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager)
            .createMulticastLock("gulno").apply { setReferenceCounted(false); acquire() }
        listener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(t: String) {}
            override fun onDiscoveryStopped(t: String) {}
            override fun onStartDiscoveryFailed(t: String, e: Int) { ui.post { fallback("찾기 실패 ($e)") } }
            override fun onStopDiscoveryFailed(t: String, e: Int) {}
            override fun onServiceLost(s: NsdServiceInfo) {}
            override fun onServiceFound(s: NsdServiceInfo) {
                if (found || !s.serviceName.startsWith("gulno-cam")) return
                found = true
                @Suppress("DEPRECATION")
                nsd?.resolveService(s, object : NsdManager.ResolveListener {
                    override fun onResolveFailed(s: NsdServiceInfo, e: Int) { ui.post { found = false; fallback("주소 확인 실패 ($e)") } }
                    override fun onServiceResolved(s: NsdServiceInfo) {
                        @Suppress("DEPRECATION")
                        val host = s.host?.hostAddress ?: return
                        val url = if (host.contains(':')) "http://[$host]:${s.port}/" else "http://$host:${s.port}/"
                        ui.post { info.text = "찾음 : $url"; web.loadUrl(url); stopFind() }
                    }
                })
            }
        }
        nsd?.discoverServices("_http._tcp", NsdManager.PROTOCOL_DNS_SD, listener)
        ui.postDelayed({ if (!found) fallback("8초 안에 못 찾음") }, 8000)
    }

    private fun fallback(why: String) {
        if (found) return
        stopFind()
        info.text = "$why → gulno-cam.local 로 시도"
        web.loadUrl("http://gulno-cam.local/")
    }

    private fun stopFind() {
        listener?.let { runCatching { nsd?.stopServiceDiscovery(it) } }
        listener = null
        lock?.let { if (it.isHeld) it.release() }
        lock = null
    }

    override fun onDestroy() {
        stopFind()
        web.destroy()
        super.onDestroy()
    }
}
