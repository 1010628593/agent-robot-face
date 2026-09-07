import SwiftUI
// Explicit Core wrapper avoids the macOS 27 SDK State macro requiring Xcode-only plugins.
typealias ViewState<Value> = SwiftUI.State<Value>
import AppKit
import Observation
import ServiceManagement

indirect enum JSON: Decodable, Sendable {
    case object([String: JSON]), array([JSON]), string(String), number(Double), bool(Bool), null
    init(from decoder: Decoder) throws {
        let c = try decoder.singleValueContainer()
        if c.decodeNil() { self = .null }
        else if let v = try? c.decode(Bool.self) { self = .bool(v) }
        else if let v = try? c.decode(Double.self) { self = .number(v) }
        else if let v = try? c.decode(String.self) { self = .string(v) }
        else if let v = try? c.decode([String: JSON].self) { self = .object(v) }
        else { self = .array(try c.decode([JSON].self)) }
    }
    subscript(_ key: String) -> JSON { if case .object(let o) = self { return o[key] ?? .null }; return .null }
    var string: String? { if case .string(let s) = self { return s }; return nil }
    var text: String { string ?? (number.map { String(format: "%g", $0) }) ?? "未知" }
    var number: Double? { if case .number(let n) = self { return n }; return nil }
    var flag: Bool { if case .bool(let b) = self { return b }; return false }
    var sourceID: String { self["id"].text }
    var metricID: String { self["key"].text }
    var items: [JSON] { if case .array(let a) = self { return a }; return [] }
}
func translated(_ value: String) -> String {
    ["working":"进行中", "tool":"工具调用", "waiting":"等待处理", "done":"已完成", "error":"错误", "cancelled":"已取消", "unknown":"未知", "idle":"空闲", "none":"无", "unobserved":"尚未观测", "input":"等待输入", "approval":"等待批准", "completed":"完成", "failed":"失败", "observed":"已观测", "unverified":"未验证", "unsupported":"不支持", "unavailable":"不可用", "partial":"部分覆盖", "exact":"精确", "estimated":"估计", "complete":"完整", "since_bridge_start":"自服务启动", "manual":"手动", "healthy":"正常"][value] ?? value
}
func productName(_ id: String) -> String {
    ["codex": "Codex", "cursor": "Cursor", "hermes": "Hermes", "workbuddy": "WorkBuddy"][id] ?? id
}
func sourceExplanation(_ code: String) -> String {
    [
        "desktop_rollout": "桌面任务记录",
        "cli_start_tool_success_failure_interrupt": "CLI 开始、工具、成功、失败及中断",
        "native_completed_aborted": "原生完成与终止记录",
        "cli_db_success": "CLI 数据库成功记录",
        "desktop_jsonl_start_tool_success": "桌面记录中的开始、工具及成功事件",
        "official_quota_unavailable": "官方配额尚不可用",
        "waiting_unverified": "等待输入或批准状态尚未验证",
        "bounded_2day_backfill_partial": "仅回溯最近两天，历史覆盖不完整",
        "silent_active_liveness_unverified": "静默任务的持续活动状态尚未验证",
        "source_syncing_history": "正在同步来源记录，当前任务暂不可判定",
        "oversized_record_skipped": "已跳过超大记录，部分事件可能缺失",
        "native_terminal_unverified": "原生任务终态尚未验证",
        "cancellation_ambiguous": "取消与其他终止原因暂无法区分",
        "reader_error": "读取来源记录失败，请检查文件访问权限"
    ][code] ?? "未识别的来源状态（\(code)）"
}
func diagnosticExplanation(_ kind: String) -> String {
    ["ValueError": "来源数据格式无效", "TypeError": "来源数据类型不匹配", "KeyError": "来源记录缺少必要字段", "AttributeError": "来源结构与预期不一致", "OSError": "本地文件或设备访问失败", "PermissionError": "本地访问权限不足", "FileNotFoundError": "所需本地文件不存在", "OperationalError": "账本数据库操作失败", "DatabaseError": "账本数据库异常", "JSONDecodeError": "来源记录无法解析", "UnicodeDecodeError": "来源记录编码无法解析", "TimeoutError": "来源读取超时", "RuntimeError": "Bridge 运行时异常"][kind] ?? "Bridge 观测异常"
}
struct DiagnosticError: Identifiable {
    let kind: String
    let timestamp: Double?
    var id: String { kind + ":" + (timestamp.map(String.init(describing:)) ?? "unknown") }
}
func age(_ milliseconds: Double?) -> String {
    guard let milliseconds else { return "未知" }
    let seconds = max(0, Int(milliseconds / 1000))
    return seconds < 60 ? "\(seconds) 秒" : seconds < 3600 ? "\(seconds / 60) 分钟" : "\(seconds / 3600) 小时"
}
func since(_ timestamp: Double?) -> String { timestamp.map { age(Date().timeIntervalSince1970 * 1000 - $0) + "前" } ?? "尚无记录" }

@MainActor @Observable final class HostModel {
    var state: JSON = .null
    var online = false
    var pending = false
    var message = ""
    var lastUpdate: Date?
    var visible = false
    var appLogin = SMAppService.mainApp.status == .enabled
    @ObservationIgnored private var pollTask: Task<Void, Never>?
    @ObservationIgnored private var fetching = false
    @ObservationIgnored private let session: URLSession
    init() {
        let config = URLSessionConfiguration.ephemeral
        config.connectionProxyDictionary = [:]
        config.timeoutIntervalForRequest = 3
        config.timeoutIntervalForResource = 4
        session = URLSession(configuration: config)
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                await self.refresh()
                for tick in 0..<(self.visible ? 2 : 20) {
                    try? await Task.sleep(for: .milliseconds(750))
                    if Task.isCancelled { return }
                    if self.visible && tick >= 1 { break }
                }
            }
        }
    }
    var canControl: Bool { online && !pending }
    func refresh() async {
        guard !fetching else { return }; fetching = true; defer { fetching = false }
        do {
            let (data, response) = try await session.data(from: URL(string: "http://127.0.0.1:17940/v1/state")!)
            guard (response as? HTTPURLResponse)?.statusCode == 200 else { throw URLError(.badServerResponse) }
            let decoded = try JSONDecoder().decode(JSON.self, from: data)
            guard decoded["api_version"].number == 1, !decoded["bridge"]["demo"].flag else { throw URLError(.cannotParseResponse) }
            state = decoded; online = true; lastUpdate = Date()
        } catch { online = false }
    }
    func control(_ action: String, agent: String? = nil, enabled: Bool? = nil) {
        guard canControl else { return }; pending = true; message = "正在等待 Bridge 确认…"
        Task {
            defer { pending = false }
            do {
                var body: [String: Any] = ["action": action]
                if action == "pin" || action == "auto" {
                    guard let rev = state["selection"]["selection_rev"].number else { throw URLError(.cannotParseResponse) }
                    body["expected_selection_rev"] = Int(rev)
                }
                if let agent { body["agent_id"] = agent }; if let enabled { body["enabled"] = enabled }
                let tokenURL = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".local/share/agent-robot-face/control.token")
                let token = try String(contentsOf: tokenURL, encoding: .utf8).trimmingCharacters(in: .whitespacesAndNewlines)
                var request = URLRequest(url: URL(string: "http://127.0.0.1:17940/v1/control")!)
                request.httpMethod = "POST"; request.setValue("application/json", forHTTPHeaderField: "Content-Type")
                request.setValue("Bearer " + token, forHTTPHeaderField: "Authorization")
                request.httpBody = try JSONSerialization.data(withJSONObject: body)
                let (_, response) = try await session.data(for: request)
                let code = (response as? HTTPURLResponse)?.statusCode ?? 0
                message = code == 200 ? "Bridge 已确认" : code == 409 ? "选择已被其他端更新，已重新读取；请重试。" : "操作未完成（HTTP \(code)），请检查本地服务与授权文件。"
                await refresh()
            } catch { message = "操作未确认，请检查服务与本地授权文件。"; await refresh() }
        }
    }
    var project: URL? {
        let plist = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/LaunchAgents/com.agentrobotface.bridge.plist")
        if let data = try? Data(contentsOf: plist), let p = try? PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any], let args = p["ProgramArguments"] as? [String], let tool = args.first(where: { $0.hasSuffix("/tools/bridge") }) { return URL(fileURLWithPath: tool).deletingLastPathComponent().deletingLastPathComponent() }
        if let path = Bundle.main.object(forInfoDictionaryKey: "BridgeProjectPath") as? String { return URL(fileURLWithPath: path) }
        return nil
    }
    func openConfig(_ source: String) {
        let paths = ["codex":".codex/hooks.json", "cursor":".cursor/hooks.json", "hermes":".hermes/config.yaml", "workbuddy":".workbuddy/settings.json"]
        guard let path = paths[source] else { return }
        let url = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(path)
        if FileManager.default.fileExists(atPath: url.path) { NSWorkspace.shared.open(url) }
        else { message = "配置文件尚不存在；请先安装或修复来源接入。" }
    }
    func openDocs() { if let project { NSWorkspace.shared.open(project.appendingPathComponent("docs/mac-app.md")) } }
    func command(_ kind: String) {
        guard !pending else { return }
        let home = FileManager.default.homeDirectoryForCurrentUser.path
        let domain = "gui/\(getuid())"
        var commands: [(String, [String])] = []
        if kind == "start" {
            commands = [("/bin/launchctl", ["kickstart", domain + "/com.agentrobotface.bridge"]), ("/bin/launchctl", ["bootstrap", domain, home + "/Library/LaunchAgents/com.agentrobotface.bridge.plist"])]
        } else if kind == "recover" {
            commands = [("/bin/launchctl", ["kickstart", "-k", domain + "/com.agentrobotface.bridge"])]
        } else if ["doctor", "repair-hooks", "install"].contains(kind), let project {
            commands = [(project.appendingPathComponent("tools/bridge").path, [kind])]
        } else { message = "未找到 Bridge 安装路径，请从项目运行安装脚本。"; return }
        pending = true; message = "正在执行本地维护…"
        let executionCommands = commands
        Task {
            let result = await Task.detached { () -> Int32 in
                var code: Int32 = -1
                for (path, args) in executionCommands {
                    let p = Process(); p.executableURL = URL(fileURLWithPath: path); p.arguments = args
                    // Never retain CLI output: diagnostics are read from the metadata API.
                    p.standardOutput = FileHandle.nullDevice; p.standardError = FileHandle.nullDevice
                    do { try p.run(); let deadline = Date().addingTimeInterval(30); while p.isRunning && Date() < deadline { try? await Task.sleep(for: .milliseconds(100)) }; if p.isRunning { p.terminate(); code = -2 } else { code = p.terminationStatus } } catch { code = -1 }
                    if code == 0 { break }
                }
                return code
            }.value
            message = result == 0 ? (kind == "repair-hooks" ? "接入配置已修复；来源重载后仍需实际事件验证。" : "维护命令已完成；当前状态见面板。") : "维护失败（退出码 \(result)）；请查看安装文档。"
            await refresh(); pending = false
        }
    }
    func setAppLogin(_ enabled: Bool) {
        do { if enabled { try SMAppService.mainApp.register() } else { try SMAppService.mainApp.unregister() }; appLogin = SMAppService.mainApp.status == .enabled
            message = SMAppService.mainApp.status == .requiresApproval ? "请在系统设置 → 登录项中允许启动。" : "菜单应用登录启动设置已更新。"
        } catch { message = "登录项设置失败，请检查系统设置中的登录项。" }
    }
}

struct ProductIcon: View {
    let id: String
    var body: some View {
        Group {
            if let url = Bundle.main.url(forResource: id, withExtension: "png"), let image = NSImage(contentsOf: url) {
                Image(nsImage: image).resizable().interpolation(.high).scaledToFit().saturation(0)
            } else { Image(systemName: "cpu") }
        }.frame(width: 23, height: 23).padding(3).background(.black, in: .rect(cornerRadius: 6)).accessibilityHidden(true)
    }
}
struct DetailRow: View {
    let label: String; let value: String
    var body: some View { HStack(alignment: .top) { Text(label).foregroundStyle(.secondary); Spacer(minLength: 12); Text(value).multilineTextAlignment(.trailing).textSelection(.enabled) }.font(.caption) }
}
struct SourceRow: View {
    let source: JSON; let model: HostModel
    @ViewState<Bool> private var expanded = false
    let caps = [("start","开始"),("tool","工具"),("success","成功"),("failure","失败"),("cancellation","取消"),("waiting","等待"),("usage","用量"),("quota","配额")]
    var body: some View {
        DisclosureGroup(isExpanded: $expanded) {
            VStack(spacing: 7) {
                if source["synchronizing"].flag { DetailRow(label: "同步来源记录", value: source["backlog_bytes"].text + " 字节待处理") }
                DetailRow(label: "活动会话", value: source["active_sessions"].text)
                DetailRow(label: "最近事件 / 扫描", value: since(source["last_event_ms"].number) + " / " + since(source["last_scan_ms"].number))
                ForEach(caps, id: \.0) { key, label in DetailRow(label: label, value: translated(source["capabilities"][key].text)) }
                Text("已验证：" + (source["validated_coverage"].items.isEmpty ? "尚无验证记录" : source["validated_coverage"].items.map { sourceExplanation($0.text) }.joined(separator: "、"))).frame(maxWidth: .infinity, alignment: .leading)
                Text("缺口：" + (source["gaps"].items.isEmpty ? "API 未报告缺口" : source["gaps"].items.map { sourceExplanation($0.text) }.joined(separator: "、"))).foregroundStyle(.secondary).frame(maxWidth: .infinity, alignment: .leading)
                Button("打开 \(source["label"].text) 配置") { model.openConfig(source["id"].text) }.frame(maxWidth: .infinity, alignment: .leading)
            }.font(.caption).padding(.top, 5)
        } label: {
            HStack(spacing: 8) {
                ProductIcon(id: source["id"].text)
                VStack(alignment: .leading, spacing: 2) {
                    Text(source["label"].text).font(.system(size: 12, weight: .semibold))
                    Text("\(source["installed"].flag ? "已安装" : "未检测安装") · \(source["configured"].flag ? "已配置" : "未配置") · \(source["observed"].flag ? "已观测" : "未观测")").font(.system(size: 10)).foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
                Text(!model.online ? "缓存" : source["synchronizing"].flag ? "同步中" : source["stale"].flag ? "过期" : source["running"].flag ? "活动" : translated(source["health"].text)).font(.caption2).foregroundStyle(source["stale"].flag ? Color.orange : Color.secondary)
            }
        }
    }
}
struct ControlPanel: View {
    let model: HostModel
    @ViewState<Bool> private var statsExpanded = false
    @ViewState<Bool> private var diagnosticsExpanded = false
    @ViewState<Bool> private var settingsExpanded = false
    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Image(systemName: "eyes").font(.title2)
                VStack(alignment: .leading) { Text("Agent Robot Face").font(.headline); Text(model.online ? "本地 Bridge 在线" : "服务不可用 · 缓存仅供参考").font(.caption).foregroundStyle(model.online ? Color.secondary : Color.orange) }
                Spacer()
                Button { Task { await model.refresh() } } label: { Image(systemName: "arrow.clockwise") }.help("刷新状态").disabled(model.pending)
            }.padding(14)
            Divider()
            ScrollView {
                VStack(alignment: .leading, spacing: 13) {
                    device
                    Divider()
                    focus
                    Divider()
                    VStack(alignment: .leading, spacing: 9) {
                        Text("来源接入").font(.subheadline.bold())
                        if model.state["sources"].items.isEmpty { Text("等待 Bridge 返回真实来源状态").font(.caption).foregroundStyle(.secondary) }
                        ForEach(model.state["sources"].items, id: \.sourceID) { SourceRow(source: $0, model: model) }
                    }
                    Divider()
                    DisclosureGroup("今日统计 · \(productName(model.state["stats"]["agent_id"].text))", isExpanded: $statsExpanded) { stats.padding(.top, 6) }
                    DisclosureGroup("接入与启动设置", isExpanded: $settingsExpanded) { settings.padding(.top, 6) }
                    DisclosureGroup("诊断与恢复", isExpanded: $diagnosticsExpanded) { diagnostics.padding(.top, 6) }
                    if !model.message.isEmpty { Text(model.message).font(.caption).foregroundStyle(.secondary).textSelection(.enabled) }
                }.padding(14)
            }
            Divider()
            HStack {
                if model.pending { ProgressView().controlSize(.small) }
                Text(model.lastUpdate.map { "更新于 " + $0.formatted(date: .omitted, time: .standard) } ?? "尚未收到状态").font(.caption2).foregroundStyle(.secondary)
                Spacer()
                Button("文档") { model.openDocs() }
                Button("退出") { NSApplication.shared.terminate(nil) }
            }.buttonStyle(.borderless).padding(12)
        }.frame(width: 380, height: 560)
        .onAppear { model.appLogin = SMAppService.mainApp.status == .enabled; model.visible = true; Task { await model.refresh() } }
        .onDisappear { model.visible = false }
    }
    private var device: some View {
        VStack(spacing: 7) {
            HStack { Label(model.online && model.state["device"]["connected"].flag ? "设备已连接" : "设备未连接", systemImage: "cable.connector").font(.subheadline.bold()); Spacer(); Text(model.state["bridge"]["paused"].flag ? "USB 已暂停" : "USB 自动连接").font(.caption).foregroundStyle(.secondary) }
            DetailRow(label: "固件 / 协议", value: model.state["device"]["firmware"].text + " / v" + model.state["device"]["protocol_version"].text)
            if let error = model.state["device"]["error"].string, !error.isEmpty { Text(error).font(.caption).foregroundStyle(.orange).frame(maxWidth: .infinity, alignment: .leading) }
        }
    }
    private var focus: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack { Text("当前焦点").font(.subheadline.bold()); Spacer(); Text(model.state["selection"]["mode"].text == "pinned" ? "手动固定" : "自动选择").font(.caption).foregroundStyle(.secondary) }
            HStack { ProductIcon(id: model.state["focus"]["agent_id"].text); Text(productName(model.state["focus"]["agent_id"].text)).fontWeight(.semibold); Spacer(); Text(translated(model.state["focus"]["state"].text)).foregroundStyle(.secondary) }
            DetailRow(label: "任务", value: model.state["focus"]["run_id"].string.map { String($0.prefix(12)) } ?? "尚无任务")
            DetailRow(label: "原因 / 新鲜度", value: translated(model.state["focus"]["reason"].text) + " · " + age(model.state["focus"]["source_age_ms"].number) + (model.state["focus"]["stale"].flag ? "（过期）" : ""))
            HStack {
                Button("自动选择") { model.control("auto") }
                Menu("固定 Agent") { ForEach(["codex","cursor","hermes","workbuddy"], id: \.self) { id in Button(productName(id)) { model.control("pin", agent: id) } } }
                Spacer()
            }.disabled(!model.canControl)
        }
    }
    private var stats: some View {
        VStack(alignment: .leading, spacing: 9) {
            Text("时区：" + model.state["stats"]["scope"]["timezone"].text).font(.caption).foregroundStyle(.secondary)
            ForEach(model.state["stats"]["metrics"].items, id: \.metricID) { metric in
                VStack(alignment: .leading, spacing: 3) {
                    DetailRow(label: ["turns":"轮次","tool_calls":"工具调用","total_tokens":"Token","cost_usd_micros":"费用（美元）"][metric["key"].text] ?? metric["label"].text, value: metric["unit"].text == "usd_micros" ? metric["value"].number.map { String(format: "%.4f", $0 / 1_000_000) } ?? "未知" : metric["value"].text)
                    Text("\(translated(metric["quality"].text)) · \(translated(metric["coverage"].text)) · \(since(metric["as_of_ms"].number))").font(.caption2).foregroundStyle(.secondary)
                    Text("来源：" + metric["source"].text + ((metric["as_of_ms"].number.map { Date().timeIntervalSince1970 * 1000 - $0 > (metric["stale_after_ms"].number ?? 120000) } ?? false) ? " · 数据过期" : "")).font(.caption2).foregroundStyle(.secondary)
                }
            }
            Text(model.state["stats"]["quotas"].items.isEmpty ? "配额：暂无官方数据" : "配额：请参见来源官方账户页面").font(.caption).foregroundStyle(.secondary)
        }
    }
    private var settings: some View {
        VStack(alignment: .leading, spacing: 9) {
            Toggle("菜单应用登录时启动", isOn: Binding(get: { model.appLogin }, set: { model.setAppLogin($0) })).disabled(model.pending)
            Toggle("Bridge 登录时启动", isOn: Binding(get: { model.state["bridge"]["launch_at_login"].flag }, set: { model.control("launch_at_login", enabled: $0) })).disabled(!model.canControl)
            HStack { Button(model.state["bridge"]["paused"].flag ? "恢复 USB" : "暂停 USB") { model.control(model.state["bridge"]["paused"].flag ? "resume" : "pause") }.disabled(!model.canControl); Text("暂停后来源观测继续").foregroundStyle(.secondary) }
            HStack { Button("安装接入") { model.command("install") }; Button("修复 Hooks") { model.command("repair-hooks") } }.disabled(model.pending)
        }.font(.caption)
    }
    private var diagnostics: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack { Button("启动服务") { model.command("start") }; Button("重启恢复") { model.command("recover") }; Button("Doctor") { model.command("doctor") } }.disabled(model.pending)
            DetailRow(label: "Bridge 版本", value: model.state["bridge"]["version"].text)
            DetailRow(label: "设备端口", value: model.state["device"]["port"].text)
            DetailRow(label: "设备最近响应", value: since(model.state["device"]["last_seen_ms"].number))
            DetailRow(label: "渲染提交均值 / 最大", value: model.state["device"]["diagnostics"]["avg_us"].text + " / " + model.state["device"]["diagnostics"]["max_us"].text + " µs")
            DetailRow(label: "记录事件数", value: model.state["diagnostics"]["event_count"].text)
            ForEach(diagnosticErrors) { error in
                VStack(alignment: .leading, spacing: 3) {
                    Text(diagnosticExplanation(error.kind)).foregroundStyle(.orange)
                    Text(error.kind + " · " + (error.timestamp.map { Date(timeIntervalSince1970: $0 / 1000).formatted(date: .numeric, time: .standard) } ?? "时间未知")).foregroundStyle(.secondary)
                }.textSelection(.enabled)
            }
            Text("最近元数据事件").fontWeight(.semibold)
            ForEach(eventRows, id: \.self) { Text($0).font(.system(size: 10, design: .monospaced)).textSelection(.enabled) }
            if eventRows.isEmpty { Text("尚无事件").foregroundStyle(.secondary) }
        }.font(.caption)
    }
    private var diagnosticErrors: [DiagnosticError] {
        var seen = Set<String>()
        return model.state["diagnostics"]["errors"].items.map {
            DiagnosticError(kind: $0["kind"].string ?? $0.string ?? "unknown", timestamp: $0["at_ms"].number)
        }.filter { seen.insert($0.id).inserted }
    }
    private var eventRows: [String] { model.state["diagnostics"]["recent_events"].items.prefix(10).map { "\(productName($0["agent"].text)) · \($0["kind"].text) · \(since($0["ts"].number))\n\(String($0["run"].text.prefix(12))) · \($0["channel"].text)" }.uniqued() }
}
extension Array where Element: Hashable { func uniqued() -> [Element] { var seen = Set<Element>(); return filter { seen.insert($0).inserted } } }
@main struct RobotFaceApp: App {
    @ViewState<HostModel> private var model = HostModel()
    var body: some Scene {
        MenuBarExtra { Panel(model: model) } label: { Image(systemName: "eyes").accessibilityLabel("Agent Robot Face") }.menuBarExtraStyle(.window)
    }
}
