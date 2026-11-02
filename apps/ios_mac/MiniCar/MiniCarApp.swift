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

    var body: some View {
        VStack(spacing: 20) {
            connection
            // 연결 전에도 보이게 두고, 누를 수만 없게 한다
            VStack(spacing: 20) {
                HStack {
                    Text(car.statusText).font(.title2.bold())
                    Spacer()
                    Button(car.auto ? "수동으로" : "자동으로") { car.send(car.auto ? "m" : "a") }
                        .buttonStyle(.borderedProminent)
                }
                pad
                Text(car.distText).font(.body.monospaced()).foregroundStyle(.secondary)
            }
            .disabled(!car.connected)
            .opacity(car.connected ? 1 : 0.4)
            Spacer()
        }
        .padding()
        .frame(minWidth: 320, minHeight: 520)
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
