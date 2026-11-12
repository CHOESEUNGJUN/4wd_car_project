import SwiftUI

// 폰을 신호등으로 (ESP32-CAM 신호등 인식 시험용) : 화면 전체가 초록 -> 노랑 -> 빨강 -> 초록 ... (진짜 신호등 순서)
// 화면을 누르면 바로 다음 색, 아래 간격(초)마다 자동으로 바뀜 (0 = 자동 끔). 켜져 있는 동안 화면 꺼짐 없음, 밝기 최대.
// 순수 초록 #00ff00 / 노랑 #ffff00 / 빨강 #ff0000 (ESP 색 범위에 맞춤). 빨강 = 정지, 초록 = 주행, 노랑 = 서행
private let lights: [(r: Double, g: Double)] = [(0, 1), (1, 1), (1, 0)]

struct LightView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var k = 2   // 빨강부터
    @State private var secs = 5
    @State private var last = Date()
    private let tick = Timer.publish(every: 0.25, on: .main, in: .common).autoconnect()

    var body: some View {
        Color(red: lights[k].r, green: lights[k].g, blue: 0)
            .ignoresSafeArea()
            .contentShape(Rectangle())
            .onTapGesture { flip() }
            .overlay(alignment: .bottom) {
                HStack(spacing: 16) {
                    Stepper("자동 \(secs == 0 ? "끔" : "\(secs)초")", value: $secs, in: 0...30)
                        .onChange(of: secs) { last = Date() }
                    Button("닫기") { dismiss() }.buttonStyle(.borderedProminent)
                }
                .padding(12)
                .background(.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 12))
                .foregroundStyle(.white)
                .padding()
            }
            .onReceive(tick) { now in
                if secs > 0 && now.timeIntervalSince(last) >= Double(secs) { flip() }
            }
            #if os(iOS)
            .statusBarHidden()
            .onAppear {
                UIApplication.shared.isIdleTimerDisabled = true
                UIScreen.main.brightness = 1
            }
            .onDisappear { UIApplication.shared.isIdleTimerDisabled = false }
            #endif
    }

    private func flip() { k = (k + 1) % lights.count; last = Date() }
}
