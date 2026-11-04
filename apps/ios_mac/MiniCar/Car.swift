import Combine
import CoreBluetooth
import Foundation

// HM-10(BLE) 연결. 아이폰과 맥에서 같은 코드로 동작한다.
// 차량 스케치(team2_bt_mode)는 HC-06과 같은 3·4번 핀, 9600으로 그대로 쓴다.
final class Car: NSObject, ObservableObject {
    // HM-10 기본 UUID. 클론 모듈은 다를 수 있다(LightBlue 같은 앱으로 확인).
    static let service = CBUUID(string: "FFE0")
    static let char = CBUUID(string: "FFE1")

    @Published var status = "블루투스 준비 중"
    @Published var found: [CBPeripheral] = []
    @Published var scanning = false
    @Published var connected = false
    // 아래는 차가 200ms마다 보내는 상태 줄로만 바꾼다
    // "ag,23,010,LINE" = 자동, 직진, 23cm, 라인센서 L C R, 단계 (뒤 두 칸은 없을 수도 있음)
    @Published var auto = false
    @Published var act: Character = "s"
    @Published var dist: Int?
    @Published var sensors = ""
    @Published var stage = ""
    @Published var held: Character?
    @Published private(set) var peripheral: CBPeripheral?
    // 원격 제어 가능 여부 : 차가 0.2초마다 보내는 상태 줄이 1초 넘게 안 오면 false
    @Published var linkOK = false
    // 자동 전환 안전 지연 중 (차가 보내는 단계가 HOLD 이거나, 방금 전환을 눌렀을 때)
    @Published var holding = false

    private var central: CBCentralManager!
    private var tx: CBCharacteristic?
    private var rxBuf = ""
    private var timer: Timer?
    private var lastStatusAt: Date?
    private var everConnected = false
    private var holdShownUntil = Date.distantPast
    static let linkTimeout: TimeInterval = 1.0
    static let switchHold: TimeInterval = 0.5

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
        // 누르고 있는 동안 200ms마다 반복 전송. 차의 LINK_MS(600ms) 안전장치와 짝이다.
        var tick = 0
        timer = Timer.scheduledTimer(withTimeInterval: 0.2, repeats: true) { [weak self] _ in
            guard let self else { return }
            self.updateLink()
            if let c = self.held, c != "s" { self.send(c) }
            // 0.6초마다 'h': 차가 "앱이 연결되어 있다"를 알 수 있게 (주행에는 영향 없음)
            tick += 1
            if tick % 3 == 0 { self.send("h") }
        }
    }

    func scan() {
        guard central.state == .poweredOn else { return }
        found = []
        scanning = true
        central.scanForPeripherals(withServices: nil)
        DispatchQueue.main.asyncAfter(deadline: .now() + 8) { [weak self] in self?.stopScan() }
    }

    func stopScan() {
        central.stopScan()
        scanning = false
    }

    func connect(_ p: CBPeripheral) {
        stopScan()
        peripheral = p
        status = "연결 중…"
        central.connect(p)
    }

    func disconnect() {
        if let p = peripheral { central.cancelPeripheralConnection(p) }
    }

    func press(_ c: Character) {
        held = c
        send(c)
    }

    // 자동 전환 (화면에서 확인을 누른 뒤). 수동 전환은 send("m") 으로 바로
    func requestAuto() {
        send("a")
        holdShownUntil = Date().addingTimeInterval(Self.switchHold)
        updateLink()
    }

    // 0.2초마다: 원격 제어 가능한지, 안전 지연 중인지
    func updateLink() {
        let ok = connected && lastStatusAt.map { Date().timeIntervalSince($0) < Self.linkTimeout } == true
        if linkOK != ok { linkOK = ok }
        let h = ok && (Date() < holdShownUntil || stage == "HOLD")
        if holding != h { holding = h }
        if !ok && held != nil { held = nil }   // 누르고 있던 버튼은 놓은 것으로 (차는 0.6초 무명령이면 스스로 멈춤)
    }

    var bannerText: String {
        if holding { return "자동 모드 전환 중 · 0.5초 안전 지연" }
        if connected { return "⚠ 차 응답 없음 · 원격 제어 불가" }
        return everConnected ? "⚠ 블루투스 끊김 · 원격 제어 불가" : "연결 안 됨 · 원격 제어 불가"
    }

    func release() {
        guard held != nil else { return }
        held = nil
        // 차가 상태 줄을 보내는 순간에는 받은 글자를 놓칠 수 있어서 정지는 두 번 보낸다
        send("s")
        send("s")
    }

    static let actNames: [Character: String] = [
        "g": "직진", "b": "후진", "l": "좌회전", "r": "우회전",
        "q": "제자리 좌회전", "w": "제자리 우회전", "s": "정지", "o": "장애물 정지",
    ]

    var statusText: String { (auto ? "자동" : "수동") + " · " + (Self.actNames[act] ?? "정지") }

    var distText: String {
        guard let dist else { return "" }
        return dist >= 999 ? "앞: 측정 안 됨" : "앞: \(dist)cm"
    }

    static let stageNames: [String: String] = [
        "MAN": "수동 조종", "HOLD": "전환 대기 (0.5초)", "CAL": "회전 시간 측정",
        "LINE": "라인 추종", "CORNER": "코너 회전", "NOLINE": "선 놓침",
        "DEAD": "데드존: 선 찾는 중", "SWAY": "좌우로 흔들며 선 찾기", "LOST": "선 못 찾음 (정지)",
        "OBST": "장애물 대기",
    ]

    // 단계 설명. 회피는 "AV3" → "회피 3단계"
    var stageText: String {
        if stage.hasPrefix("AV") { return "회피 \(stage.dropFirst(2))단계" }
        return Self.stageNames[stage] ?? stage
    }

    // 라인센서 "010" → "○ ● ○" (● = 1)
    var sensorText: String {
        sensors.isEmpty ? "" : "센서 " + sensors.map { $0 == "1" ? "●" : "○" }.joined(separator: " ")
    }

    func send(_ c: Character) {
        guard let p = peripheral, let tx else { return }
        let type: CBCharacteristicWriteType = tx.properties.contains(.writeWithoutResponse) ? .withoutResponse : .withResponse
        p.writeValue(Data(String(c).utf8), for: tx, type: type)
    }

    private func reset(_ msg: String) {
        connected = false
        tx = nil
        held = nil
        peripheral = nil
        status = msg
    }
}

extension Car: CBCentralManagerDelegate, CBPeripheralDelegate {
    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        status = c.state == .poweredOn ? "기기 찾기를 누르세요" : "블루투스를 켜고 권한을 허용하세요"
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral,
                        advertisementData: [String: Any], rssi: NSNumber) {
        guard p.name != nil, !found.contains(p) else { return }
        found.append(p)
    }

    func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
        p.delegate = self
        p.discoverServices([Self.service])
    }

    func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        reset("연결 실패")
    }

    func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
        reset("연결 끊김")
    }

    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard let s = p.services?.first(where: { $0.uuid == Self.service }) else {
            status = "HM-10(FFE0) 서비스가 없습니다"
            disconnect()
            return
        }
        p.discoverCharacteristics([Self.char], for: s)
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        guard let ch = s.characteristics?.first(where: { $0.uuid == Self.char }) else { return }
        tx = ch
        p.setNotifyValue(true, for: ch)
        connected = true
        everConnected = true
        lastStatusAt = Date()   // 첫 상태 줄이 올 때까지 1초 기다려 줌
        status = "연결됨"
        send("m")  // 연결 직후 수동으로 맞춘다. 차가 돌려주는 'm'으로 표시가 맞춰진다.
    }

    func peripheral(_ p: CBPeripheral, didUpdateValueFor ch: CBCharacteristic, error: Error?) {
        guard let d = ch.value, let s = String(data: d, encoding: .utf8) else { return }
        for c in s {
            if c == "\n" {
                onLine(rxBuf)
                rxBuf = ""
            } else if c != "\r" {
                rxBuf = String((rxBuf + String(c)).suffix(64))
            }
        }
    }

    // "ag,23,010,LINE" → 모드 a, 동작 g, 거리 23, 센서 010, 단계 LINE
    private func onLine(_ line: String) {
        lastStatusAt = Date()   // 차가 살아 있음
        let f = line.split(separator: ",", omittingEmptySubsequences: false).map(String.init)
        let c = Array(f[0])
        guard c.count >= 2, c[0] == "a" || c[0] == "m" else { return }
        auto = c[0] == "a"
        act = c[1]
        dist = f.count > 1 ? Int(f[1]) : nil
        sensors = f.count > 2 ? f[2] : ""
        stage = f.count > 3 ? f[3] : ""
    }
}
