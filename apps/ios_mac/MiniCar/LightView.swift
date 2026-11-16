import SwiftUI

// 폰을 신호등으로 (ESP32-CAM 신호등 인식 시험용) : 화면 전체가 초록 -> 노랑 -> 빨강 -> 초록 ... (진짜 신호등 순서)
// 화면을 누르면 바로 다음 색, 아래 간격(초)마다 자동으로 바뀜 (0 = 자동 끔). 켜져 있는 동안 화면 꺼짐 없음, 밝기 최대.
// 순수 초록 #00ff00 / 노랑 #ffff00 / 빨강 #ff0000 (ESP 색 범위에 맞춤). 빨강 = 정지, 초록 = 주행, 노랑 = 서행
// "정지 표지판" 으로 바꾸면 검은 바탕에 빨간 팔각형 + 흰 테두리 + 흰 STOP 글자 (ESP 는 빨강이 네모 칸을 덜 채우고 흰색이 있으면 표지판으로 봄 :
// 3초 서 있다가 출발). 폰 빨간 화면(신호등)과 같은 빨강이라 모양(모서리 빈 곳, 흰 글자)으로 가름
private let lights: [(r: Double, g: Double)] = [(0, 1), (1, 1), (1, 0)]

struct LightView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var k = 2   // 빨강부터
    @State private var secs = 5
    @State private var last = Date()
    @State private var sign = false   // true = 정지 표지판

    var body: some View {
        Group {
            if sign { stopSign } else { Color(red: lights[k].r, green: lights[k].g, blue: 0) }
        }
        .ignoresSafeArea()
        .contentShape(Rectangle())
        .onTapGesture { if !sign { flip() } }
        .overlay(alignment: .bottom) {
            HStack(spacing: 12) {
                if !sign {
                    Stepper("자동 \(secs == 0 ? "끔" : "\(secs)초")", value: $secs, in: 0...30)
                        .onChange(of: secs) { last = Date() }
                }
                Button(sign ? "신호등" : "정지 표지판") { sign.toggle(); last = Date() }.buttonStyle(.bordered)
                Button("닫기") { dismiss() }.buttonStyle(.borderedProminent)
            }
            .padding(12)
            .background(.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 12))
            .foregroundStyle(.white)
            .padding()
        }
        // 0.25초마다 자동 간격이 지났는지 확인 (화면이 닫히면 저절로 멈춤)
        .task {
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(250))
                if !sign && secs > 0 && Date().timeIntervalSince(last) >= Double(secs) { flip() }
            }
        }
        #if os(iOS)
        .statusBarHidden()
        .onAppear {
            UIApplication.shared.isIdleTimerDisabled = true
            // 밝기 최대 (UIScreen.main 은 iOS 26 에서 안 쓰게 됨 -> 지금 창의 화면으로)
            (UIApplication.shared.connectedScenes.first as? UIWindowScene)?.screen.brightness = 1
        }
        .onDisappear { UIApplication.shared.isIdleTimerDisabled = false }
        #endif
    }

    // 정지 표지판 : 검은 바탕, 흰 테두리, 순수 빨강 #ff0000 팔각형, 흰 STOP
    private var stopSign: some View {
        ZStack {
            Color.black
            Octagon().fill(Color.white).padding(20)
            Octagon().fill(Color(red: 1, green: 0, blue: 0)).padding(34)
            Text("STOP")
                .font(.system(size: 120, weight: .black))
                .minimumScaleFactor(0.2)
                .lineLimit(1)
                .foregroundStyle(.white)
                .padding(.horizontal, 70)
        }
    }

    private func flip() { k = (k + 1) % lights.count; last = Date() }
}

// 정팔각형 (주어진 칸 안에 가장 크게, 위·아래·옆 변이 평평하게)
struct Octagon: Shape {
    func path(in r: CGRect) -> Path {
        let R = min(r.width, r.height) / 2 / cos(.pi / 8)   // 꼭짓점까지 거리 (변까지 = 칸의 절반)
        var p = Path()
        for i in 0..<8 {
            let a = Double(i) * .pi / 4 + .pi / 8
            let pt = CGPoint(x: r.midX + R * cos(a), y: r.midY + R * sin(a))
            if i == 0 { p.move(to: pt) } else { p.addLine(to: pt) }
        }
        p.closeSubpath()
        return p
    }
}
