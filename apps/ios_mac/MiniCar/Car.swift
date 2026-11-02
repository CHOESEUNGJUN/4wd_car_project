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
    // 아래 셋은 차가 200ms마다 보내는 상태 줄("ag,23" = 자동, 직진, 23cm)로만 바꾼다
    @Published var auto = false
    @Published var act: Character = "s"
    @Published var dist: Int?
    @Published var held: Character?
    @Published private(set) var peripheral: CBPeripheral?

    private var central: CBCentralManager!
    private var tx: CBCharacteristic?
    private var rxBuf = ""
    private var timer: Timer?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
        // 누르고 있는 동안 200ms마다 반복 전송. 차의 LINK_MS(600ms) 안전장치와 짝이다.
        var tick = 0
        timer = Timer.scheduledTimer(withTimeInterval: 0.2, repeats: true) { [weak self] _ in
            guard let self else { return }
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

    // "ag,23" → 모드 a, 동작 g, 거리 23
    private func onLine(_ line: String) {
        let c = Array(line)
        guard c.count >= 2, c[0] == "a" || c[0] == "m" else { return }
        auto = c[0] == "a"
        act = c[1]
        dist = line.split(separator: ",").last.flatMap { Int($0) }
    }
}
