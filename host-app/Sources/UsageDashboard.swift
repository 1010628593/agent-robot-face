import SwiftUI
import Observation

private let usageAgents = ["codex", "cursor", "hermes", "workbuddy"]
func usageNumber(_ value: Double?) -> String {
    guard let value, value.isFinite else { return "—" }
    for (scale, suffix) in [(1e9, "B"), (1e6, "M"), (1e3, "K")] where value >= scale {
        return String(format: value / scale >= 100 ? "%.0f%@" : "%.1f%@", value / scale, suffix)
    }
    return value.formatted(.number.precision(.fractionLength(0...2)))
}
private func usageHue(_ value: Double?) -> Color {
    guard let value else { return Color(white: 0.4) }
    let hex = value < 25 ? 0x2563EB : value < 50 ? 0x22C55E : value < 75 ? 0xEAB308 : value < 90 ? 0xF97316 : 0xEF4444
    return Color(red: Double((hex >> 16) & 255) / 255, green: Double((hex >> 8) & 255) / 255, blue: Double(hex & 255) / 255)
}
private func quotaTitle(_ value: JSON) -> String {
    ["plan":"套餐额度", "on_demand":"按量使用", "team_pool":"团队额度", "Session":"当前周期", "Weekly":"每周额度", "Spark Session":"Spark 当前周期", "Spark Weekly":"Spark 每周额度", "session":"当前周期", "weekly":"每周额度", "unknown":"账户额度"][value["label"].text] ?? (usageAgents.contains(value["label"].text) ? "账户额度" : value["label"].text)
}
private func quotaReason(_ value: JSON) -> String {
    switch value["reason"].text {
    case "network_timeout": return "官方查询超时，可稍后刷新"
    case "quota_probe_failed", "network_error": return "官方额度查询失败，可稍后重试"
    case "provider_adapter_not_configured": return "当前模型提供方尚无额度渠道"
    case "unsupported": return "该来源尚无可靠的官方额度渠道"
    case "login_expired", "needs_auth", "missing_auth", "not_logged_in": return "请先在来源应用中登录，再刷新"
    case "": return value["availability"].text == "available" ? "官方账户额度" : "暂无官方来源"
    default: return value["availability"].text == "needs_auth" ? "请在来源应用中检查登录状态" : "暂未取得可靠的官方额度"
    }
}
private func quotaFresh(_ quota: JSON) -> Bool {
    quota["availability"].text == "available" && (quota["as_of_ms"].number.map { Date().timeIntervalSince1970 * 1000 - $0 <= 900_000 } ?? false)
}
private func quotaIdentity(_ q: JSON) -> String { q["provider"].text + ":" + q["account_hash"].text + ":" + q["window_key"].text }
private func actualCost(_ row: JSON) -> String {
    let costs = row["actual_costs"].items
    guard !costs.isEmpty else { return "—" }
    guard costs.count == 1, let n = costs[0]["amount"].number else { return "多币种" }
    return n.formatted(.currency(code: costs[0]["currency"].text).precision(.fractionLength(2)))
}

@MainActor @Observable final class UsageModel {
    var snapshot: JSON = .null
    var subject = "current"
    var period = "today"
    var toolsOverview = false
    var requestedQuotaID: String?
    var online = false
    var loading = false
    var refreshPending = false
    var notice = ""
    private var generation = 0
    private var refreshRevision: Double?
    private var refreshStarted: Date?
    private let session: URLSession
    init() {
        let config = URLSessionConfiguration.ephemeral
        config.connectionProxyDictionary = [:]
        config.timeoutIntervalForRequest = 4
        config.timeoutIntervalForResource = 5
        session = URLSession(configuration: config)
    }
    var querySubject: String { toolsOverview ? "all" : subject }
    var key: String { querySubject + ":" + period }
    func observe() async {
        generation += 1
        let ticket = generation, scope = querySubject, range = period
        snapshot = .null; loading = true; online = false
        while !Task.isCancelled && ticket == generation {
            do {
                var components = URLComponents(string: "http://127.0.0.1:17940/v1/usage")!
                components.queryItems = [URLQueryItem(name: "subject", value: scope), URLQueryItem(name: "period", value: range)]
                let (data, response) = try await session.data(from: components.url!)
                guard (response as? HTTPURLResponse)?.statusCode == 200, data.count <= 2_000_000 else { throw URLError(.badServerResponse) }
                let decoded = try JSONDecoder().decode(JSON.self, from: data)
                guard decoded["version"].number == 1, decoded["period"].text == range else { throw URLError(.cannotParseResponse) }
                guard !Task.isCancelled, ticket == generation else { return }
                snapshot = decoded; online = true
                if refreshPending, let before = refreshRevision, let revision = decoded["revision"].number, revision != before, !decoded["collecting"].flag {
                    refreshPending = false; notice = decoded["status"].text == "ready" ? "已收到新一批来源数据" : "采集未完成，保留上次数据"
                }
            } catch {
                guard !Task.isCancelled, ticket == generation else { return }
                online = false
            }
            loading = false
            if refreshPending, let started = refreshStarted, Date().timeIntervalSince(started) > 300 { refreshPending = false; notice = "采集仍未完成，可在诊断中查看来源状态" }
            do { try await Task.sleep(for: .seconds(2)) } catch { return }
        }
    }
    func refresh() {
        guard !refreshPending else { return }
        refreshPending = true; refreshRevision = snapshot["revision"].number ?? 0; refreshStarted = Date(); notice = "正在请求采集…"
        Task {
            do {
                let tokenURL = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".local/share/agent-robot-face/control.token")
                let token = try String(contentsOf: tokenURL, encoding: .utf8).trimmingCharacters(in: .whitespacesAndNewlines)
                var request = URLRequest(url: URL(string: "http://127.0.0.1:17940/v1/usage/refresh")!)
                request.httpMethod = "POST"; request.httpBody = Data("{}".utf8)
                request.setValue("application/json", forHTTPHeaderField: "Content-Type")
                request.setValue("Bearer " + token, forHTTPHeaderField: "Authorization")
                let (_, response) = try await session.data(for: request)
                guard (response as? HTTPURLResponse)?.statusCode == 202 else { throw URLError(.badServerResponse) }
                notice = "已排队，等待来源更新…"
            } catch { refreshPending = false; notice = "刷新请求未确认，请检查 Bridge" }
        }
    }
}

struct Panel: View {
    let model: HostModel
    @ViewState<String> private var tab = "usage"
    @ViewState<UsageModel> private var usage = UsageModel()
    @ViewState<Bool> private var expanded = false
    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Image(systemName: "eye").font(.title3)
                Text("Agent Robot Face").font(.system(size: 14, weight: .medium))
                Spacer()
                Button { tab = tab == "control" ? "usage" : "control" } label: { Image(systemName: tab == "control" ? "xmark" : "gearshape").frame(width: 28, height: 28) }.buttonStyle(.plain).help("设备与接入")
            }.padding(.horizontal, 18).padding(.top, 12).padding(.bottom, 8)
            if tab == "control" { ControlPanel(model: model) }
            else if expanded {
                Button { expanded = false } label: { Label("返回状态卡", systemImage: "chevron.left").frame(maxWidth: .infinity, alignment: .leading) }.buttonStyle(.plain).padding(.horizontal, 18)
                UsageDashboard(model: usage, host: model)
            } else { UsageStatusCard(model: usage, host: model, expanded: $expanded) }
        }.frame(width: expanded || tab == "control" ? 380 : 340).foregroundStyle(.white).background(.black).environment(\.colorScheme, .dark).preferredColorScheme(.dark)
        .onAppear { model.visible = true }
        .onDisappear { model.visible = false }
        .onChange(of: tab) { model.visible = true }
    }
}

private struct UsageStatusCard: View {
    let model: UsageModel
    let host: HostModel
    @Binding var expanded: Bool
    private var agent: String { model.subject == "current" ? host.state["focus"]["agent_id"].text : model.subject }
    private var quotas: [JSON] {
        Array(model.snapshot["quotas"].items.filter { $0["agents"].items.contains { $0.text == agent } }.sorted {
            let left = $0["window_key"].text.hasPrefix("codex:"), right = $1["window_key"].text.hasPrefix("codex:")
            if left != right { return left }
            return ($0["reset_ms"].number ?? .greatestFiniteMagnitude) < ($1["reset_ms"].number ?? .greatestFiniteMagnitude)
        }.prefix(3))
    }
    private var quota: JSON? { quotas.first }
    private var percent: Double? { quota.flatMap { quotaFresh($0) ? $0["used_pct"].number : nil } }
    private var reset: String {
        guard let q = quota, quotaFresh(q), let ms = q["reset_ms"].number else { return quota.map(quotaReason) ?? "暂无官方额度来源" }
        let minutes = max(0, Int((ms / 1000 - Date().timeIntervalSince1970) / 60))
        if minutes >= 1440 { return "\(minutes / 1440) 天 \(minutes % 1440 / 60) 小时后重置" }
        return "\(minutes / 60) 小时 \(minutes % 60) 分后重置"
    }
    private func openQuota(_ q: JSON) { model.requestedQuotaID = "quota:" + quotaIdentity(q); expanded = true }
    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 4) {
                ForEach(usageAgents, id: \.self) { id in
                    Button { model.subject = id; model.toolsOverview = false; model.period = "today" } label: {
                        VStack(spacing: 8) { ProductIcon(id: id).scaleEffect(1.25).frame(height: 28); Text(productName(id)).font(.system(size: 11)) }
                            .frame(maxWidth: .infinity).padding(.vertical, 12).background(agent == id ? Color(white: 0.13) : .clear, in: .rect(cornerRadius: 13))
                    }.buttonStyle(.plain)
                }
            }.padding(.vertical, 8)
            Divider().overlay(Color(white: 0.15))
            VStack(spacing: 12) {
                ZStack {
                    if quotas.count > 1 {
                        ForEach(Array(quotas.enumerated()), id: \.element.quotaID) { index, q in
                            let p = quotaFresh(q) ? q["used_pct"].number : nil
                            Circle().stroke(Color(white: 0.16), lineWidth: 8).padding(CGFloat(index) * 14)
                            if let p, p > 0 {
                                Circle().trim(from: 0, to: min(1, max(0, p / 100))).stroke(usageHue(p), style: StrokeStyle(lineWidth: 8, lineCap: .round)).rotationEffect(.degrees(-90)).padding(CGFloat(index) * 14)
                            }
                        }
                        VStack(spacing: 8) {
                            ForEach(quotas, id: \.quotaID) { q in
                                Button { openQuota(q) } label: {
                                    HStack(spacing: 6) {
                                        Text(quotaTitle(q)).font(.system(size: 10)).lineLimit(1)
                                        Text(quotaFresh(q) ? q["used_pct"].number.map { String(format: "%.0f%%", $0) } ?? "—" : "—").font(.system(size: 13, weight: .medium)).monospacedDigit()
                                    }.foregroundStyle(quotaFresh(q) ? usageHue(q["used_pct"].number) : .gray)
                                }.buttonStyle(.plain)
                            }
                        }.frame(width: 132)
                    } else {
                        Circle().trim(from: 0, to: 0.75).stroke(Color(white: 0.16), style: StrokeStyle(lineWidth: 9, lineCap: .round)).rotationEffect(.degrees(135))
                        if let percent, percent > 0 {
                            Circle().trim(from: 0, to: 0.75 * min(1, max(0, percent / 100))).stroke(usageHue(percent), style: StrokeStyle(lineWidth: 9, lineCap: .round)).rotationEffect(.degrees(135))
                        }
                        VStack(spacing: 2) {
                            Text(percent.map { String(format: "%.0f%%", $0) } ?? "—").font(.system(size: 48, weight: .light)).monospacedDigit()
                            Text(percent == nil ? "暂无额度" : "已用额度").font(.system(size: 13)).foregroundStyle(.secondary)
                        }
                    }
                }.frame(width: 196, height: 196).padding(.top, 22)
                    .simultaneousGesture(SpatialTapGesture().onEnded { event in
                        let dx = event.location.x - 98, dy = event.location.y - 120
                        let radius = sqrt(dx * dx + dy * dy)
                        if radius >= 65, radius <= 104, !quotas.isEmpty {
                            let index = quotas.count > 1 ? max(0, min(quotas.count - 1, Int(((98 - radius) / 14).rounded()))) : 0
                            openQuota(quotas[index])
                        }
                    })
                Text(quotas.count > 1 ? "已用额度 · 点击查看周期" : reset).font(.system(size: 12)).foregroundStyle(.secondary).lineLimit(2).multilineTextAlignment(.center).padding(.top, -18).padding(.bottom, 18)
            }.frame(maxWidth: .infinity).frame(height: 285)
            Divider().overlay(Color(white: 0.15))
            HStack { Text("今日 Token").foregroundStyle(.secondary); Spacer(); Text(usageNumber(model.snapshot["summary"]["total_tokens"].number)).monospacedDigit() }.font(.system(size: 14)).padding(.vertical, 14)
            Divider().overlay(Color(white: 0.15))
            HStack {
                Button { expanded = true } label: { HStack(spacing: 5) { Text("查看用量与额度详情"); Image(systemName: "chevron.right").font(.caption2) } }.buttonStyle(.plain)
                Spacer()
                Button { model.refresh() } label: { Image(systemName: model.refreshPending ? "hourglass" : "arrow.clockwise") }.buttonStyle(.plain).disabled(model.refreshPending).help("刷新真实来源")
            }.font(.system(size: 12)).padding(.vertical, 14)
            if !model.online && !model.loading { Text("Bridge 暂未连接").font(.caption2).foregroundStyle(.secondary).padding(.bottom, 10) }
        }.padding(.horizontal, 18)
        .task(id: model.key + ":" + agent) { await model.observe() }
        .onAppear { model.toolsOverview = false; model.period = "today"; if !usageAgents.contains(agent) { model.subject = "codex" } }
    }
}

private extension JSON { var quotaID: String { quotaIdentity(self) } }

private struct UsageItem: Identifiable {
    let id: String
    let title: String
    let value: String
    let subtitle: String
    let number: Double?
    let data: JSON
    init(_ id: String, _ title: String, _ value: String, _ subtitle: String = "", number: Double? = nil, data: JSON = .null) {
        self.id = id; self.title = title; self.value = value; self.subtitle = subtitle; self.number = number; self.data = data
    }
}
private struct UsageRing: View {
    let percent: Double?
    var size: CGFloat = 60
    var body: some View {
        ZStack {
            Circle().trim(from: 0, to: size > 80 ? 0.75 : 1).stroke(Color(white: 0.15), style: StrokeStyle(lineWidth: size > 80 ? 10 : 5, lineCap: .round)).rotationEffect(.degrees(size > 80 ? 135 : -90))
            if let percent, percent > 0 {
                Circle().trim(from: 0, to: (size > 80 ? 0.75 : 1) * max(0, min(1, percent / 100))).stroke(usageHue(percent), style: StrokeStyle(lineWidth: size > 80 ? 10 : 5, lineCap: .round)).rotationEffect(.degrees(size > 80 ? 135 : -90))
            }
            Text(percent.map { String(format: "%.0f%%", $0) } ?? "—").font(.system(size: size > 80 ? 48 : 15, weight: size > 80 ? .light : .medium)).monospacedDigit()
        }.frame(width: size, height: size).accessibilityElement(children: .ignore).accessibilityLabel("已用额度").accessibilityValue(percent.map { "\($0.formatted()) 百分比" } ?? "暂无数据")
    }
}

struct UsageDashboard: View {
    let model: UsageModel
    let host: HostModel
    @ViewState<String?> private var category: String?
    @ViewState<String?> private var detail: String?
    @ViewState<Int> private var page = 0
    @ViewState<Bool> private var metadata = false
    private var summary: JSON { model.snapshot["summary"] }
    private var items: [UsageItem] {
        switch category {
        case "quota":
            return model.snapshot["quotas"].items.map { q in
                UsageItem("quota:" + quotaIdentity(q), productName(q["provider"].text) + " · " + quotaTitle(q), quotaFresh(q) ? q["used_pct"].number.map { String(format: "%.0f%%", $0) } ?? "—" : "—", quotaFresh(q) ? (q["used_pct"].number == nil ? "未提供额度上限" : "已用额度") : quotaReason(q), number: quotaFresh(q) ? q["used_pct"].number : nil, data: q)
            }
        case "cost":
            let costs = summary["actual_costs"].items
            return costs.isEmpty ? [UsageItem("cost:unknown", "实际费用", "—", "暂无实际账单来源")] : costs.map { cost in UsageItem("cost:" + cost["currency"].text, cost["currency"].text + " 实际费用", cost["amount"].number.map { $0.formatted(.currency(code: cost["currency"].text)) } ?? "—", translated(cost["coverage"].text), data: cost) }
        case "cache":
            return [UsageItem("cache_read_tokens", "缓存读取", usageNumber(summary["cache_read_tokens"].number)), UsageItem("cache_write_tokens", "缓存写入", usageNumber(summary["cache_write_tokens"].number)), UsageItem("input_tokens", "全部输入", usageNumber(summary["input_tokens"].number), "包含已知缓存读取与写入")]
        case "models":
            return model.snapshot["models"].items.map { UsageItem("model:" + $0["model"].text, $0["model"].text, usageNumber($0["total_tokens"].number), "Token · " + translated($0["coverage"].text), number: $0["total_tokens"].number, data: $0) }
        default:
            return [UsageItem("total_tokens", "全部 Token", usageNumber(summary["total_tokens"].number)), UsageItem("input_tokens", "输入", usageNumber(summary["input_tokens"].number), "包含已知缓存"), UsageItem("output_tokens", "输出", usageNumber(summary["output_tokens"].number)), UsageItem("models", "模型分布", usageNumber(model.snapshot["model_count"].number), "按 Token 排列")]
        }
    }
    private var selected: UsageItem? { items.first { $0.id == detail } }
    private var pageCount: Int { max(1, (items.count + 2) / 3) }
    private var title: String { category.map { ["quota":"额度", "cost":"实际费用", "cache":"缓存", "models":"模型分布", "tokens":"Token"][ $0 ] ?? "用量" } ?? "用量总览" }
    private var scopeTitle: String { model.subject == "current" ? productName(host.state["focus"]["agent_id"].text) : model.subject == "all" ? "全部工具" : productName(model.subject) }
    var body: some View {
        VStack(spacing: 15) {
            header
            if detail == nil && category != "quota" { filters }
            if model.loading && model.snapshot["revision"].number == nil { Spacer(); ProgressView("正在读取用量"); Spacer() }
            else if !model.online && model.snapshot["revision"].number == nil { Spacer(); unavailable; Spacer() }
            else {
                if let selected { detailView(selected) }
                else if category != nil { listView }
                else { overview }
                Spacer(minLength: 0)
            }
            footer
        }.padding(.horizontal, 18).padding(.top, 8).padding(.bottom, 14).frame(height: 490).background(.black)
        .task(id: model.key + ":" + (model.subject == "current" && !model.toolsOverview ? host.state["focus"]["agent_id"].text : "")) { await model.observe() }
        .onAppear { if let requested = model.requestedQuotaID { category = "quota"; detail = requested; model.requestedQuotaID = nil } }
        .onChange(of: model.period) { page = 0; detail = nil }
        .onChange(of: model.subject) { page = 0; detail = nil }
        .simultaneousGesture(DragGesture(minimumDistance: 35).onEnded { value in
            if abs(value.translation.width) > abs(value.translation.height) { shift(value.translation.width < 0 ? 1 : -1) }
            else if value.translation.height > 45 { back() }
        })
    }
    private var header: some View {
        HStack {
            if category != nil { Button { back() } label: { Image(systemName: "chevron.left").frame(width: 22, height: 28) }.buttonStyle(.plain).accessibilityLabel("返回上一层") }
            VStack(alignment: .leading, spacing: 3) {
                Text(detail == nil ? title : selected?.title ?? title).font(.system(size: 19, weight: .semibold)).lineLimit(1)
                if detail == nil { Text(model.toolsOverview ? "全部工具" : scopeTitle).font(.caption).foregroundStyle(.secondary) }
            }
            Spacer()
            Button { model.refresh() } label: { Image(systemName: model.refreshPending ? "hourglass" : "arrow.clockwise").frame(width: 28, height: 28) }.buttonStyle(.plain).disabled(model.refreshPending).help("从真实来源重新采集")
        }
    }
    private var filters: some View {
        HStack(spacing: 10) {
            Menu {
                Button("当前 Agent") { choose("current") }; Button("全部工具") { choose("all") }
                Divider()
                ForEach(usageAgents, id: \.self) { id in Button(productName(id)) { choose(id) } }
            } label: { Text(model.subject == "current" ? "当前 Agent" : model.subject == "all" ? "全部工具" : productName(model.subject)).font(.caption) }.menuStyle(.borderlessButton).fixedSize()
            Spacer(minLength: 0)
            Picker("时间范围", selection: Binding(get: { model.period }, set: { model.period = $0 })) { Text("今日").tag("today"); Text("7 天").tag("7d"); Text("30 天").tag("30d") }.pickerStyle(.segmented).labelsHidden().frame(width: 186)
        }
    }
    private var overview: some View {
        VStack(spacing: 12) {
            Picker("总览维度", selection: Binding(get: { model.toolsOverview }, set: { model.toolsOverview = $0 })) { Text("统计维度").tag(false); Text("全部工具").tag(true) }.pickerStyle(.segmented).labelsHidden()
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                if model.toolsOverview {
                    ForEach(usageAgents, id: \.self) { id in toolCard(id) }
                } else {
                    metricCard("tokens", "Token", usageNumber(summary["total_tokens"].number), "number")
                    metricCard("cost", "实际费用", actualCost(summary), "creditcard")
                    metricCard("quota", "额度", "", "gauge.with.dots.needle.67percent")
                    metricCard("cache", "缓存读取", usageNumber(summary["cache_read_tokens"].number), "externaldrive")
                }
            }
        }
    }
    private func toolCard(_ id: String) -> some View {
        let row = model.snapshot["agents"].items.first { $0["id"].text == id } ?? .null
        let quota = representative(id)
        return Button { model.subject = id; model.toolsOverview = false; category = "tokens"; detail = nil; page = 0 } label: {
            VStack(spacing: 8) {
                HStack { ProductIcon(id: id); Text(productName(id)).font(.caption.weight(.medium)); Spacer(minLength: 0) }
                HStack { UsageRing(percent: quota.flatMap { quotaFresh($0) ? $0["used_pct"].number : nil }, size: 51); Spacer(); VStack(alignment: .trailing, spacing: 4) { Text(usageNumber(row["total_tokens"].number)).font(.system(size: 19, weight: .semibold, design: .rounded)); Text("Token").font(.caption2).foregroundStyle(.secondary) } }
            }.padding(12).frame(height: 102).frame(maxWidth: .infinity).background(Color(white: 0.055), in: .rect(cornerRadius: 16))
        }.buttonStyle(.plain).accessibilityLabel(productName(id) + "，Token " + usageNumber(row["total_tokens"].number) + "，查看详情")
    }
    private func metricCard(_ id: String, _ label: String, _ value: String, _ icon: String) -> some View {
        Button { category = id; page = 0; detail = nil } label: {
            VStack(alignment: .leading, spacing: 12) {
                HStack { Image(systemName: icon).foregroundStyle(.secondary); Text(label).font(.caption); Spacer() }
                if id == "quota" { HStack { UsageRing(percent: representative(nil).flatMap { quotaFresh($0) ? $0["used_pct"].number : nil }, size: 55); Spacer(); VStack(alignment: .trailing, spacing: 4) { Text(representative(nil).map { productName($0["provider"].text) } ?? "暂无来源"); if representative(nil) != nil { Text("已用") } }.font(.caption2).foregroundStyle(.secondary) } }
                else { Text(value).font(.system(size: 29, weight: .semibold, design: .rounded)).minimumScaleFactor(0.65).lineLimit(1).frame(height: 55, alignment: .leading) }
            }.padding(14).frame(maxWidth: .infinity, alignment: .leading).frame(height: 126).background(Color(white: 0.055), in: .rect(cornerRadius: 16))
        }.buttonStyle(.plain)
    }
    private var listView: some View {
        VStack(spacing: 9) {
            if category == "tokens" { HStack { Button("额度") { category = "quota"; page = 0 }; Button("实际费用") { category = "cost"; page = 0 }; Button("缓存") { category = "cache"; page = 0 } }.buttonStyle(.borderless).font(.caption).padding(.bottom, 2) }
            if items.isEmpty { Text(category == "models" ? "这个时间范围暂无模型记录" : "暂无官方额度来源").foregroundStyle(.secondary).frame(height: 180) }
            ForEach(Array(items.dropFirst(min(page, pageCount - 1) * 3).prefix(3))) { item in
                Button { if item.id == "models" { category = "models"; page = 0 } else { detail = item.id; metadata = false } } label: {
                    HStack(spacing: 12) {
                        if item.id.hasPrefix("quota:") { UsageRing(percent: item.number, size: 48) }
                        VStack(alignment: .leading, spacing: 5) { Text(item.title).font(.system(size: 13, weight: .medium)).lineLimit(1); if !item.subtitle.isEmpty { Text(item.subtitle).font(.caption2).foregroundStyle(.secondary).lineLimit(2) } }
                        Spacer(minLength: 2)
                        if !item.id.hasPrefix("quota:") { Text(item.value).font(.system(size: 21, weight: .semibold, design: .rounded)).lineLimit(1).minimumScaleFactor(0.6) }
                        Image(systemName: "chevron.right").font(.caption2).foregroundStyle(.secondary)
                    }.padding(14).frame(height: 76).background(Color(white: 0.055), in: .rect(cornerRadius: 14))
                }.buttonStyle(.plain)
            }
            HStack { Button { shift(-1) } label: { Image(systemName: "chevron.left") }.disabled(page == 0); Spacer(); Text("\(min(page, pageCount - 1) + 1) / \(pageCount)").font(.caption2).foregroundStyle(.secondary); Spacer(); Button { shift(1) } label: { Image(systemName: "chevron.right") }.disabled(page >= pageCount - 1) }.buttonStyle(.borderless).padding(.top, 3)
        }
    }
    private func detailView(_ item: UsageItem) -> some View {
        ScrollView {
            VStack(spacing: 15) {
                if item.id.hasPrefix("quota:") {
                    UsageRing(percent: item.number, size: 210).padding(.top, 6)
                    Text(item.number == nil ? item.subtitle : "已用额度").font(.caption).foregroundStyle(.secondary)
                    Text(item.data["reset_ms"].number.map { Date(timeIntervalSince1970: $0 / 1000).formatted(date: .abbreviated, time: .shortened) + " 重置" } ?? "暂无重置时间").font(.caption).foregroundStyle(.secondary)
                } else {
                    Text(item.value).font(.system(size: 43, weight: .semibold, design: .rounded)).minimumScaleFactor(0.5).lineLimit(1).padding(.top, 8)
                    if item.id.hasPrefix("cost:") { costDetails(item.data) }
                    else if item.id.hasPrefix("model:") {
                        DetailRow(label: "输入 / 输出", value: usageNumber(item.data["input_tokens"].number) + " / " + usageNumber(item.data["output_tokens"].number))
                        DetailRow(label: "缓存读取 / 写入", value: usageNumber(item.data["cache_read_tokens"].number) + " / " + usageNumber(item.data["cache_write_tokens"].number))
                        Text("模型统计 · " + translated(item.data["coverage"].text)).font(.caption).foregroundStyle(.secondary)
                    } else {
                        Text(item.id == "input_tokens" ? "Token · 包含已知缓存读取与写入" : "Token").font(.caption).foregroundStyle(.secondary)
                        UsageHistory(history: model.snapshot["history"].items, metric: item.id)
                        HStack { Text(model.snapshot["start_date"].text); Spacer(); Text(model.snapshot["end_date"].text) }.font(.caption2).foregroundStyle(.secondary)
                    }
                }
                DisclosureGroup("来源与口径", isExpanded: $metadata) {
                    VStack(spacing: 8) {
                        if item.id.hasPrefix("quota:") { quotaDetails(item.data) }
                        DetailRow(label: "覆盖范围", value: translated(item.data["coverage"].string ?? summary["coverage"].text))
                        DetailRow(label: "数据时间", value: since(item.data["as_of_ms"].number ?? summary["as_of_ms"].number))
                        DetailRow(label: "统计时区", value: model.snapshot["timezone"].text)
                        ForEach(model.snapshot["sources"].items.filter { model.querySubject == "all" || $0["id"].text == model.snapshot["subject"].text }, id: \.sourceID) { source in
                            DetailRow(label: productName(source["id"].text), value: (source["provenance"].text == "cursor.official.usage-events" ? "官方用量记录" : source["provenance"].text == "tokscale.local_logs" ? "本地结构化记录" : "暂无来源") + " · " + translated(source["coverage"].text))
                        }
                        Text("缺失数据保留为空；费用仅包含来源明确报告的实际金额。缓存属于输入，不能再次与输入相加。").font(.caption2).foregroundStyle(.secondary).frame(maxWidth: .infinity, alignment: .leading)
                    }.padding(.top, 8)
                }.font(.caption)
            }.padding(.horizontal, 2)
        }
    }
    private func quotaDetails(_ q: JSON) -> some View {
        VStack(spacing: 9) {
            DetailRow(label: "已用 / 上限", value: usageNumber(q["used"].number) + " / " + usageNumber(q["limit"].number) + " " + (q["unit"].string ?? ""))
            DetailRow(label: "重置", value: q["reset_ms"].number.map { Date(timeIntervalSince1970: $0 / 1000).formatted(date: .abbreviated, time: .shortened) } ?? "暂无来源")
            DetailRow(label: "额度查询", value: since(q["as_of_ms"].number) + (quotaFresh(q) ? "" : " · 当前不可用"))
            if !quotaFresh(q) { Text(quotaReason(q)).font(.caption2).foregroundStyle(.secondary) }
            if q["agents"].items.count > 1 { Text("共享账户：" + q["agents"].items.map { productName($0.text) }.joined(separator: "、")).font(.caption2).foregroundStyle(.secondary) }

        }
    }
    private func costDetails(_ cost: JSON) -> some View {
        VStack(spacing: 9) {
            Text(cost["amount"].number == nil ? "暂无实际账单来源" : translated(cost["coverage"].text)).font(.caption).foregroundStyle(.secondary)
            if !cost["provenance"].items.isEmpty { DetailRow(label: "来源", value: cost["provenance"].items.map { $0.text }.joined(separator: "、")) }
            Text("仅展示明确的实际扣费记录。不同币种分别列出；不以模型价格估算费用。").font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center)
        }
    }
    private var footer: some View {
        VStack(spacing: 5) {
            if !model.notice.isEmpty { Text(model.notice).font(.caption2).foregroundStyle(.secondary).lineLimit(2) }
            HStack {
                Circle().fill(model.online && !model.snapshot["stale"].flag && model.snapshot["status"].text == "ready" ? Color.green : Color.orange).frame(width: 5, height: 5)
                Text(!model.online ? "Bridge 离线 · 缓存" : model.snapshot["collecting"].flag ? "来源采集中" : model.snapshot["status"].text == "error" ? "采集失败 · 上次数据" : model.snapshot["stale"].flag ? "数据已过期" : since(model.snapshot["as_of_ms"].number) + "更新").font(.caption2).foregroundStyle(.secondary)
                Spacer()
                if category != nil { Button("总览") { category = nil; detail = nil; page = 0 }.buttonStyle(.borderless).font(.caption2) }
            }
        }
    }
    private var unavailable: some View { VStack(spacing: 10) { Image(systemName: "cable.connector.slash").font(.largeTitle).foregroundStyle(.secondary); Text("暂时无法读取用量").font(.headline); Text("请在“设备与接入”检查 Bridge 状态").font(.caption).foregroundStyle(.secondary) } }
    private func choose(_ scope: String) { model.subject = scope; model.toolsOverview = false; category = nil; detail = nil; page = 0 }
    private func representative(_ agent: String?) -> JSON? {
        model.snapshot["quotas"].items.filter { (agent == nil || $0["agents"].items.contains { $0.string == agent }) && quotaFresh($0) && $0["used_pct"].number != nil }.sorted { a, b in
            let aMain = a["provider"].text == "codex" && a["window_key"].text.hasPrefix("codex:")
            let bMain = b["provider"].text == "codex" && b["window_key"].text.hasPrefix("codex:")
            let aSupplemental = a["provider"].text == "codex" && !aMain
            let bSupplemental = b["provider"].text == "codex" && !bMain
            if aSupplemental != bSupplemental { return !aSupplemental }
            let left = a["reset_ms"].number ?? Double.greatestFiniteMagnitude, right = b["reset_ms"].number ?? Double.greatestFiniteMagnitude
            return left == right ? quotaIdentity(a) < quotaIdentity(b) : left < right
        }.first
    }
    private func back() { if detail != nil { detail = nil } else if category == "models" { category = "tokens"; page = 0 } else { category = nil; page = 0 } }
    private func shift(_ direction: Int) {
        if let detail, let index = items.firstIndex(where: { $0.id == detail }) { let next = index + direction; if items.indices.contains(next), items[next].id != "models" { self.detail = items[next].id; page = next / 3; metadata = false } }
        else if category != nil { page = max(0, min(pageCount - 1, page + direction)) }
        else { model.toolsOverview = direction > 0 }
    }
}

private struct UsageHistory: View {
    let history: [JSON]
    let metric: String
    var body: some View {
        let values = history.map { $0[metric].number }
        let maxValue = values.compactMap { $0 }.max() ?? 0
        VStack(spacing: 8) {
            Canvas { context, size in
                let count = max(1, values.count)
                let slot = size.width / CGFloat(count)
                for index in values.indices {
                    guard let value = values[index] else { continue }
                    let height = maxValue > 0 ? size.height * value / maxValue : 0
                    let rect = CGRect(x: slot * CGFloat(index) + 2, y: size.height - height, width: max(1, slot - 4), height: height)
                    context.fill(Path(roundedRect: rect, cornerRadius: 2), with: .color(Color(red: 0.145, green: 0.388, blue: 0.922)))
                    if value == 0 { context.fill(Path(CGRect(x: rect.minX, y: size.height - 1, width: rect.width, height: 1)), with: .color(.gray)) }
                }
            }.frame(height: 94).accessibilityLabel("每日用量趋势，\(values.compactMap { $0 }.count) 天有记录，缺失日期留空")
            Text(values.compactMap { $0 }.isEmpty ? "这个时间范围暂无记录" : "每日实际记录 · 空白日期无数据").font(.caption2).foregroundStyle(.secondary)
        }
    }
}
