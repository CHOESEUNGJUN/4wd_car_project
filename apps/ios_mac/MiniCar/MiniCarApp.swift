import SwiftUI

@main
struct MiniCarApp: App {
    @StateObject private var car = Car()
    @Environment(\.scenePhase) private var phase

    var body: some Scene {
        WindowGroup {
            ContentView().environmentObject(car)
        }
        // 앱이 뒤로 가면 누르던 버튼을 놓은 것으로 처리
        .onChange(of: phase) { _, p in if p != .active { car.release() } }
    }
}

struct ContentView: View {
    @EnvironmentObject var car: Car
    @State private var askAuto = false
    @State private var showLight = false

    var body: some View {
        VStack(spacing: 20) {
            connection
            // 원격 제어 불가(빨강) / 자동 전환 안전 지연(주황)
            if !car.linkOK || car.holding {
                Text(car.bannerText)
                    .font(.headline)
                    .foregroundStyle(.white)
                    .frame(maxWidth: .infinity)
                    .padding(10)
                    .background(car.holding ? Color.orange : Color.red, in: RoundedRectangle(cornerRadius: 10))
            }
            // 연결 전에도 보이게 두고, 원격 제어가 안 되면 누를 수만 없게 한다
            VStack(spacing: 20) {
                HStack {
                    Text(car.statusText).font(.title2.bold())
                    Spacer()
                    // 수동으로는 즉시, 자동으로는 한 번 물어본 뒤
                    Button(car.auto ? "수동으로" : "자동으로") {
                        if car.auto { car.send("m") } else { askAuto = true }
                    }
                    .buttonStyle(.borderedProminent)
                }
                pad
                VStack(spacing: 4) {
                    Text(car.stageText).font(.headline)
                    Text([car.distText, car.sensorText].filter { !$0.isEmpty }.joined(separator: "  ·  "))
                        .font(.body.monospaced())
                }
                .foregroundStyle(.secondary)
            }
            .disabled(!car.linkOK)
            .opacity(car.linkOK ? 1 : 0.4)
            Spacer()
            // 폰을 신호등으로 (카메라 신호등 인식 시험용, 차 연결 없이도 됨)
            Button("신호등 화면") { showLight = true }
        }
        .padding()
        .frame(minWidth: 320, minHeight: 520)
        #if os(iOS)
        .fullScreenCover(isPresented: $showLight) { LightView() }
        #else
        .sheet(isPresented: $showLight) { LightView().frame(minWidth: 480, minHeight: 360) }
        #endif
        .alert("자동 모드로 전환하시겠습니까?", isPresented: $askAuto) {
            Button("전환") { car.requestAuto() }
            Button("취소", role: .cancel) {}
        } message: {
            Text("차가 0.5초 멈춘 뒤 라인을 따라 출발합니다.\n차를 선 위에 올려 두세요.")
        }
    }

    @ViewBuilder var connection: some View {
        if car.connected, let p = car.peripheral {
            HStack {
                Text("연결: \(p.name ?? "?")")
                Spacer()
                Button("끊기") { car.disconnect() }
            }
        } else {
            HStack {
                Text(car.status)
                Spacer()
                Button(car.scanning ? "찾는 중…" : "기기 찾기") { car.scan() }.disabled(car.scanning)
            }
            List(car.found, id: \.identifier) { p in
                Button(p.name ?? "이름 없음") { car.connect(p) }
            }
            .frame(maxHeight: 260)
        }
    }

    var pad: some View {
        Grid(horizontalSpacing: 12, verticalSpacing: 12) {
            GridRow { key("q", "↺"); key("g", "▲"); key("w", "↻") }
            GridRow { key("l", "◀"); key("s", "■"); key("r", "▶") }
            GridRow { Color.clear.frame(width: 80, height: 80); key("b", "▼"); Color.clear.frame(width: 80, height: 80) }
        }
    }

    // 누르는 동안 명령, 떼면 s (교안 STEP 4의 TouchDown/TouchUp)
    func key(_ c: Character, _ label: String) -> some View {
        Text(label)
            .font(.largeTitle)
            .frame(width: 80, height: 80)
            .foregroundStyle(car.held == c ? .white : .primary)
            .background(car.held == c ? Color.accentColor : Color.gray.opacity(0.25),
                        in: RoundedRectangle(cornerRadius: 16))
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { _ in if car.held != c { car.press(c) } }
                .onEnded { _ in car.release() })
    }
}
