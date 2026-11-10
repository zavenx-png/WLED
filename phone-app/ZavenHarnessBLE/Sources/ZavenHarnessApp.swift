import SwiftUI
import CoreBluetooth
import WebKit

// Self-contained Bluetooth companion for WLED Harness v5.3 BLE.
// This is a separate app: Apple's official WLED app does not expose BLE transport.
struct NamedItem: Identifiable, Hashable {
    let id: Int
    let name: String
}

final class HarnessBluetooth: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let serviceID = CBUUID(string: "7B7B5A10-6D88-4E45-A782-CCFD3B418800")
    private let writeID   = CBUUID(string: "7B7B5A11-6D88-4E45-A782-CCFD3B418800")
    private let notifyID  = CBUUID(string: "7B7B5A12-6D88-4E45-A782-CCFD3B418800")
    private var central: CBCentralManager!
    private var device: CBPeripheral?
    private var writeChar: CBCharacteristic?
    private var notifyChar: CBCharacteristic?
    private var inbox = Data()
    private var writeQueue: [Data] = []
    private var isWriting = false
    private var waitingForReply = false
    private var pendingCommands: [[String: Any]] = []

    @Published var status = "Bluetooth starting…"
    @Published var connected = false
    @Published var brightness = 127.0
    @Published var isOn = false
    @Published var effect = 0
    @Published var palette = 0
    @Published var preset = 0
    @Published var speed = 128.0
    @Published var intensity = 128.0
    @Published var custom1 = 128.0
    @Published var custom2 = 128.0
    @Published var custom3 = 16.0
    @Published var option1 = false
    @Published var option2 = false
    @Published var option3 = false
    @Published var effects: [NamedItem] = []
    @Published var palettes: [NamedItem] = []

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn: status = "Ready to scan"; scan()
        case .poweredOff: status = "Turn on iPhone Bluetooth"
        case .unauthorized: status = "Allow Bluetooth access in iOS Settings"
        default: status = "Waiting for Bluetooth"
        }
    }

    func scan() {
        guard central.state == .poweredOn else { return }
        status = "Scanning for Zaven Harness BLE…"
        central.scanForPeripherals(withServices: [serviceID], options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false
        ])
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        central.stopScan()
        device = peripheral
        peripheral.delegate = self
        status = "Connecting…"
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        status = "Discovering controls…"
        peripheral.discoverServices([serviceID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        status = "Connection failed: \(error?.localizedDescription ?? "try again")"
        connected = false
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        connected = false
        device = nil
        writeChar = nil
        notifyChar = nil
        writeQueue.removeAll()
        pendingCommands.removeAll()
        waitingForReply = false
        isWriting = false
        inbox.removeAll()
        status = "Disconnected. Tap Connect to retry."
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil, let services = peripheral.services else {
            status = "Unable to discover BLE service"
            return
        }
        for service in services where service.uuid == serviceID {
            peripheral.discoverCharacteristics([writeID, notifyID], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard error == nil else { status = "BLE characteristic discovery failed"; return }
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == writeID { writeChar = characteristic }
            if characteristic.uuid == notifyID { notifyChar = characteristic }
        }
        if let tx = notifyChar {
            peripheral.setNotifyValue(true, for: tx)
        } else {
            status = "Notifications are unavailable"
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil, characteristic.isNotifying, writeChar != nil else {
            status = "Unable to subscribe to BLE notifications"
            return
        }
        connected = true
        status = "Connected — pairing may be requested on first command"
        refresh()
        loadEffects(offset: 0)
        loadPalettes(offset: 0)
    }

    // Each JSON message ends with newline; writes are 120 bytes and acknowledged.
    private func command(_ dictionary: [String: Any]) {
        pendingCommands.append(dictionary)
        sendNextRequest()
    }

    private func sendNextRequest() {
        guard connected, !waitingForReply, !pendingCommands.isEmpty else { return }
        let dictionary = pendingCommands.removeFirst()
        waitingForReply = true
        guard let json = try? JSONSerialization.data(withJSONObject: dictionary, options: [.fragmentsAllowed]) else {
            status = "Cannot encode control command"
            waitingForReply = false
            sendNextRequest()
            return
        }
        var frame = json
        frame.append(0x0A)
        guard frame.count <= 1024 else { status = "BLE command is too long"; waitingForReply = false; sendNextRequest(); return }
        let maxChunk = min(device?.maximumWriteValueLength(for: .withResponse) ?? 120, 120)
        guard maxChunk > 0 else { return }
        for start in stride(from: 0, to: frame.count, by: maxChunk) {
            writeQueue.append(frame.subdata(in: start..<min(start + maxChunk, frame.count)))
        }
        writeNext()
    }

    private func writeNext() {
        guard !isWriting, !writeQueue.isEmpty,
              let peripheral = device, let characteristic = writeChar else { return }
        isWriting = true
        let next = writeQueue.removeFirst()
        peripheral.writeValue(next, for: characteristic, type: .withResponse)
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        isWriting = false
        if let error {
            status = "BLE write failed: \(error.localizedDescription)"
            writeQueue.removeAll()
        } else {
            writeNext()
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil, let data = characteristic.value else { return }
        inbox.append(data)
        if inbox.count > 8192 {
            inbox.removeAll()
            status = "Reply exceeded expected length"
            return
        }
        while let newline = inbox.firstIndex(of: 0x0A) {
            let packet = Data(inbox.prefix(upTo: newline))
            inbox.removeSubrange(...newline)
            guard let object = try? JSONSerialization.jsonObject(with: packet),
                  let json = object as? [String: Any] else { continue }
            receive(json)
        }
    }

    private func receive(_ json: [String: Any]) {
        let type = json["type"] as? String ?? ""
        waitingForReply = false
        if type == "error" {
            status = "Harness error: \(json["reason"] as? String ?? "unknown")"
        } else if type == "state" {
            status = "Connected to Zaven Harness"
            if let n = json["bri"] as? NSNumber { brightness = n.doubleValue }
            if let n = json["on"] as? Bool { isOn = n }
            if let n = json["ps"] as? NSNumber { preset = n.intValue }
            if let n = json["fx"] as? NSNumber { effect = n.intValue }
            if let n = json["pal"] as? NSNumber { palette = n.intValue }
            if let n = json["sx"] as? NSNumber { speed = n.doubleValue }
            if let n = json["ix"] as? NSNumber { intensity = n.doubleValue }
            if let n = json["c1"] as? NSNumber { custom1 = n.doubleValue }
            if let n = json["c2"] as? NSNumber { custom2 = n.doubleValue }
            if let n = json["c3"] as? NSNumber { custom3 = n.doubleValue }
            if let b = json["o1"] as? Bool { option1 = b }
            if let b = json["o2"] as? Bool { option2 = b }
            if let b = json["o3"] as? Bool { option3 = b }
        } else if type == "fx" || type == "pal" {
            guard let items = json["items"] as? [[String: Any]],
                  let offset = json["offset"] as? Int,
                  let total = json["total"] as? Int else { return }
            let page = items.compactMap { entry -> NamedItem? in
                guard let id = entry["id"] as? Int, let name = entry["name"] as? String else { return nil }
                return NamedItem(id: id, name: name)
            }
            if type == "fx" {
                if offset == 0 { effects.removeAll() }
                effects.append(contentsOf: page)
                if !page.isEmpty && offset + page.count < total { loadEffects(offset: offset + page.count) }
            } else {
                if offset == 0 { palettes.removeAll() }
                palettes.append(contentsOf: page)
                if !page.isEmpty && offset + page.count < total { loadPalettes(offset: offset + page.count) }
            }
        }
        sendNextRequest()
    }

    private func set(_ state: [String: Any]) {
        command(["op": "set", "s": state])
    }
    func refresh() { command(["op": "get"]) }
    private func loadEffects(offset: Int) { command(["op": "fx", "offset": offset]) }
    private func loadPalettes(offset: Int) { command(["op": "pal", "offset": offset]) }
    func toggle() { set(["on": !isOn]) }
    func setBrightness() { set(["bri": Int(brightness), "on": true]) }
    func selectEffect(_ id: Int) {
        effect = id
        set(["seg": ["fx": id], "hfx": ["op": "select", "fx": id]])
    }
    func selectPalette(_ id: Int) {
        palette = id
        set(["seg": ["pal": id]])
    }
    func loadPreset(_ id: Int) { set(["ps": id]) }
    func applySettings() {
        set(["seg": [
            "sx": Int(speed), "ix": Int(intensity),
            "c1": Int(custom1), "c2": Int(custom2), "c3": Int(custom3),
            "o1": option1, "o2": option2, "o3": option3
        ], "hfx": ["op": "saveSettings", "fx": effect]])
    }
    func resetEffectSettings() { set(["hfx": ["op": "defaultSettings", "fx": effect]]) }
    func savePaletteDefault() { set(["hfx": ["op": "savePaletteDefault", "fx": effect]]) }
    func resetPaletteDefault() { set(["hfx": ["op": "nativePaletteDefault", "fx": effect]]) }
}

@main
struct ZavenHarnessApp: App {
    @StateObject private var model = HarnessBluetooth()
    @State private var transport = 0
    var body: some Scene {
        WindowGroup {
            VStack(spacing: 0) {
                Picker("Connection", selection: $transport) {
                    Text("Bluetooth").tag(0)
                    Text("Wi-Fi").tag(1)
                }
                .pickerStyle(.segmented)
                .padding(.horizontal)
                .padding(.top, 8)
                if transport == 0 {
                    HarnessScreen().environmentObject(model)
                } else {
                    WLEDWiFiScreen()
                }
            }
            .preferredColorScheme(.dark)
        }
    }
}

// Native WLED UI over Wi-Fi is loaded inside the same companion app.
// BLE has separate native controls because a normal HTTP WebView cannot
// communicate directly with the iPhone's CoreBluetooth API.
struct WLEDWiFiScreen: View {
    @State private var address = "http://4.3.2.1"
    @State private var url = URL(string: "http://4.3.2.1")!

    var body: some View {
        VStack(spacing: 8) {
            HStack {
                TextField("WLED device IP or URL", text: $address)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .keyboardType(.URL)
                    .textFieldStyle(.roundedBorder)
                Button("Go") {
                    let entered = address.trimmingCharacters(in: .whitespacesAndNewlines)
                    let full = entered.contains("://") ? entered : "http://" + entered
                    guard let parsed = URL(string: full),
                          let host = parsed.host, !host.isEmpty,
                          parsed.scheme == "http" || parsed.scheme == "https" else { return }
                    url = parsed
                }
                .buttonStyle(.borderedProminent)
            }
            .padding(.horizontal)
            Text("Connect your iPhone to WLED-AP for 4.3.2.1, or enter your harness's network IP.")
                .font(.caption).foregroundStyle(.secondary)
                .padding(.horizontal)
            WLEDWebView(url: url)
        }
        .navigationTitle("WLED via Wi-Fi")
    }
}

struct WLEDWebView: UIViewRepresentable {
    let url: URL
    final class Coordinator {
        var lastRequested: URL?
    }
    func makeCoordinator() -> Coordinator { Coordinator() }
    func makeUIView(context: Context) -> WKWebView {
        let web = WKWebView(frame: .zero)
        web.allowsBackForwardNavigationGestures = true
        return web
    }
    func updateUIView(_ webView: WKWebView, context: Context) {
        if context.coordinator.lastRequested != url {
            context.coordinator.lastRequested = url
            webView.load(URLRequest(url: url))
        }
    }
}

struct HarnessScreen: View {
    @EnvironmentObject private var model: HarnessBluetooth
    @State private var search = ""
    @State private var showEffects = false
    @State private var showPalettes = false

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text("ZAVEN HARNESS").font(.headline)
                            Text(model.status).font(.footnote).foregroundStyle(.secondary)
                        }
                        Spacer()
                        Circle()
                            .fill(model.connected ? Color.green : Color.gray)
                            .frame(width: 12, height: 12)
                    }
                    HStack {
                        Button("Connect") { model.scan() }
                        Spacer()
                        Button("Refresh") { model.refresh() }.disabled(!model.connected)
                    }
                }
                Section("POWER & BRIGHTNESS") {
                    Toggle("Harness on", isOn: Binding(
                        get: { model.isOn },
                        set: { _ in model.toggle() }
                    )).disabled(!model.connected)
                    Slider(value: $model.brightness, in: 1...255, step: 1,
                           onEditingChanged: { isEditing in
                        if !isEditing { model.setBrightness() }
                    }).disabled(!model.connected)
                    HStack {
                        Text("Brightness")
                        Spacer()
                        Text("\(Int(model.brightness)) / 255").foregroundStyle(.secondary)
                    }
                }
                Section("EFFECTS") {
                    HStack {
                        Text("Selected")
                        Spacer()
                        Text(model.effects.first(where: { $0.id == model.effect })?.name ?? "#\(model.effect)")
                            .foregroundStyle(.secondary).lineLimit(1)
                    }
                    Button(showEffects ? "Hide effects" : "Choose effect (\(model.effects.count) loaded)") {
                        showEffects.toggle()
                    }
                    if showEffects {
                        TextField("Search effects", text: $search)
                        ForEach(model.effects.filter { search.isEmpty || $0.name.localizedCaseInsensitiveContains(search) }) { entry in
                            Button {
                                model.selectEffect(entry.id)
                                showEffects = false
                            } label: {
                                HStack {
                                    Text(entry.name)
                                    Spacer()
                                    if model.effect == entry.id { Image(systemName: "checkmark") }
                                }
                            }
                        }
                    }
                }
                Section("PALETTES") {
                    HStack {
                        Text("Selected")
                        Spacer()
                        Text(model.palettes.first(where: { $0.id == model.palette })?.name ?? "#\(model.palette)")
                            .foregroundStyle(.secondary).lineLimit(1)
                    }
                    Button(showPalettes ? "Hide palettes" : "Choose palette (\(model.palettes.count) loaded)") {
                        showPalettes.toggle()
                    }
                    if showPalettes {
                        ForEach(model.palettes) { entry in
                            Button {
                                model.selectPalette(entry.id)
                                showPalettes = false
                            } label: {
                                HStack {
                                    Text(entry.name)
                                    Spacer()
                                    if model.palette == entry.id { Image(systemName: "checkmark") }
                                }
                            }
                        }
                    }
                    Button("Select as default palette") { model.savePaletteDefault() }
                    Button("Reset to original default palette") { model.resetPaletteDefault() }
                }
                Section("EFFECT SETTINGS") {
                    setting("Speed", value: $model.speed)
                    setting("Intensity", value: $model.intensity)
                    setting("Custom 1", value: $model.custom1)
                    setting("Custom 2", value: $model.custom2)
                    setting("Custom 3", value: $model.custom3, maximum: 31)
                    Toggle("Option 1", isOn: $model.option1)
                    Toggle("Option 2", isOn: $model.option2)
                    Toggle("Option 3", isOn: $model.option3)
                    Button("Apply & set as effect defaults") { model.applySettings() }
                    Button("Reset effect settings to factory defaults") { model.resetEffectSettings() }
                }
                Section("PRESETS") {
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 3)) {
                        ForEach(1...9, id: \.self) { value in
                            Button("Preset \(value)") { model.loadPreset(value) }
                                .buttonStyle(.bordered)
                                .padding(.vertical, 3)
                        }
                    }
                }
                Section {
                    Text("This app uses authenticated BLE directly. The official WLED app still uses Wi‑Fi. The 6-digit pairing code is set under WLED → Config → Usermods; restart the harness after enabling Bluetooth.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Harness Control")
        }
    }

    @ViewBuilder
    private func setting(_ name: String, value: Binding<Double>, maximum: Double = 255) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack { Text(name); Spacer(); Text("\(Int(value.wrappedValue))").foregroundStyle(.secondary) }
            Slider(value: value, in: 0...maximum, step: 1)
        }
    }
}
