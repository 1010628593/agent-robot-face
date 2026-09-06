"""Contract models and semantic validation (T03).

Layered validation, shared by HTTP and serial paths:
  1. strict_load  — parser-level rules (8192B, UTF-8, no dup keys, no NaN/Inf,
                    finite numbers, depth <= 12). Mirrors tools/validate_contracts.py.
  2. Pydantic strict models — extra="forbid" everywhere; enums/patterns/ranges
                    mirror the handoff JSON schemas (contracts/*.schema.json).
  3. Semantic rules — per-type business rules that JSON Schema cannot express
                    (metric key/unit match, terminal reason match, quota fabrications,
                    used_pct consistency, ACK status/reason pairing, event id rules).
                    These mirror semantic_message/semantic_event/semantic_capability
                    in tools/validate_contracts.py, which is the contract authority.

Nothing here may silently accept: every rule that validate_contracts.py enforces
must also be enforced here. If the two disagree, fix THIS file to match the
validator, not the other way around.
"""
from __future__ import annotations

import json
import math
from typing import Any, Literal, Optional, Union
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator


# ---------------------------------------------------------------------------
# Parser layer
# ---------------------------------------------------------------------------

class ContractError(ValueError):
    """Raised for any contract violation (parser, schema, or semantic)."""


FRAME_LIMIT_BYTES = 8192
DEPTH_LIMIT = 12
INT64_SAFE = 9007199254740991


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def _no_constant(value: str) -> None:
    raise ContractError(f"Non-finite JSON constant: {value}")


def _unique_object(pairs: list[tuple[str, Any]]) -> dict:
    result: dict[str, Any] = {}
    for key, value in pairs:
        _require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _depth(value: Any) -> int:
    if isinstance(value, dict):
        return 1 + max((_depth(v) for v in value.values()), default=0)
    if isinstance(value, list):
        return 1 + max((_depth(v) for v in value), default=0)
    return 0


def _finite(value: Any) -> None:
    if isinstance(value, float):
        _require(math.isfinite(value), "Non-finite numeric value")
    elif isinstance(value, dict):
        for v in value.values():
            _finite(v)
    elif isinstance(value, list):
        for v in value:
            _finite(v)


def strict_load(raw: bytes, *, bounded: bool = True) -> dict:
    """Strict JSON parse enforcing the frame budget and parser-level rejects."""
    if bounded:
        _require(len(raw) <= FRAME_LIMIT_BYTES, "JSON body exceeds 8192 bytes")
    try:
        text = raw.decode("utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        raise ContractError("Invalid UTF-8") from exc
    try:
        value = json.loads(
            text,
            object_pairs_hook=_unique_object,
            parse_constant=_no_constant,
        )
    except json.JSONDecodeError as exc:
        raise ContractError(f"Invalid JSON: {exc}") from exc
    _require(isinstance(value, dict), "Expected JSON object")
    _finite(value)
    if bounded:
        _require(_depth(value) <= DEPTH_LIMIT, "JSON nesting exceeds 12 containers")
    return value


def _compact(value: dict) -> bytes:
    return json.dumps(
        value, separators=(",", ":"), ensure_ascii=False, allow_nan=False
    ).encode("utf-8")


# ---------------------------------------------------------------------------
# Shared enums / atoms
# ---------------------------------------------------------------------------

AgentId = Literal["codex", "workbuddy", "cursor", "hermes"]
StateEnum = Literal["idle", "working", "tool", "waiting", "done", "error", "cancelled", "unknown"]
QualityObserved = Literal["observed", "inferred", "reported", "manual", "simulated"]
ReasonEnum = Literal["none", "approval", "input", "completed", "failed", "cancelled", "unobserved"]
ASCII_PRINTABLE = r"^[ -~]*$"
HEX32 = r"^[0-9a-f]{32}$"

_MAX_INT = INT64_SAFE


class StrictBase(BaseModel):
    model_config = ConfigDict(extra="forbid")


# ---------------------------------------------------------------------------
# Device message bodies (one per $def in device-message.schema.json)
# ---------------------------------------------------------------------------

class DisplaySize(StrictBase):
    width: Literal[466]
    height: Literal[466]


class HelloBody(StrictBase):
    device_id: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    boot_id: str = Field(pattern=HEX32)
    firmware: str = Field(max_length=32, pattern=ASCII_PRINTABLE)
    display: DisplaySize
    min_version: Literal[1]
    max_version: Literal[1]
    handshake_id: str = Field(pattern=HEX32)


class WelcomeBody(StrictBase):
    bridge_epoch: str = Field(pattern=HEX32)
    selected_agent: AgentId
    selection_rev: int = Field(ge=0, le=2147483647)
    heartbeat_ms: Literal[2000]
    offline_after_ms: Literal[6000]
    max_frame_bytes: Literal[8192]
    demo: bool


class AgentCapabilities(StrictBase):
    state: Literal["observed", "reported", "manual", "none"]
    usage: Literal["automatic", "import", "manual", "none"]
    quota: Literal["official", "import", "manual", "none"]
    open_agent: bool
    open_usage: bool


class CatalogAgent(StrictBase):
    id: AgentId
    label: str = Field(max_length=16, pattern=ASCII_PRINTABLE)
    state: StateEnum
    health: Literal["ready", "partial", "unavailable", "needs_auth", "disabled"]
    active_sessions: int = Field(ge=0, le=16)
    attention_count: int = Field(ge=0, le=99)
    capabilities: AgentCapabilities


class CatalogBody(StrictBase):
    agents: list[CatalogAgent] = Field(min_length=4, max_length=4)

    @field_validator("agents")
    @classmethod
    def _four_unique_agents(cls, agents: list[CatalogAgent]) -> list[CatalogAgent]:
        ids = [a.id for a in agents]
        _require(len(ids) == 4 and set(ids) == {"codex", "workbuddy", "cursor", "hermes"},
                 "Catalog must contain four unique agents")
        return agents


class FocusBody(StrictBase):
    agent_id: AgentId
    selection_rev: int = Field(ge=0, le=2147483647)
    session_key: Optional[str] = Field(default=None, max_length=64, pattern=ASCII_PRINTABLE)
    run_id: Optional[str] = Field(default=None, max_length=64, pattern=ASCII_PRINTABLE)
    state: StateEnum
    reason: ReasonEnum
    quality: QualityObserved
    source_age_ms: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    stale: bool
    tool: str = Field(default="", max_length=24, pattern=ASCII_PRINTABLE)
    detail: str = Field(default="", max_length=48, pattern=ASCII_PRINTABLE)
    run_elapsed_ms: int = Field(ge=0, le=_MAX_INT)
    active_sessions: int = Field(ge=0, le=16)
    progress: Optional[float] = Field(default=None, ge=0.0, le=1.0)

    @model_validator(mode="after")
    def _semantic_focus(self) -> "FocusBody":
        if self.state == "waiting":
            _require(self.reason in {"approval", "input"}, "Waiting needs explicit reason")
        required_reason = {"done": "completed", "error": "failed", "cancelled": "cancelled"}
        if self.state in required_reason:
            _require(self.reason == required_reason[self.state], "Terminal reason mismatch")
        return self


class StatsScope(StrictBase):
    kind: Literal["today", "session"]
    timezone: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    start_ms: int = Field(ge=0, le=_MAX_INT)
    end_ms: int = Field(ge=0, le=_MAX_INT)

    @model_validator(mode="after")
    def _semantic_scope(self) -> "StatsScope":
        _require(self.end_ms > self.start_ms, "Invalid statistics time interval")
        try:
            ZoneInfo(self.timezone)
        except (ZoneInfoNotFoundError, ValueError):
            raise ContractError("Invalid IANA timezone")
        return self


METRIC_UNIT = {
    "turns": "turn",
    "tool_calls": "call",
    "requests": "request",
    "input_tokens": "token",
    "output_tokens": "token",
    "total_tokens": "token",
    "active_time_ms": "ms",
    "cost_usd_micros": "usd_micros",
    "context_tokens": "token",
}


class Metric(StrictBase):
    key: Literal[
        "turns", "tool_calls", "requests", "input_tokens", "output_tokens",
        "total_tokens", "active_time_ms", "cost_usd_micros", "context_tokens",
    ]
    label: str = Field(max_length=20, pattern=ASCII_PRINTABLE)
    value: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    unit: Literal["turn", "call", "request", "token", "ms", "usd_micros"]
    quality: Literal["exact", "estimated", "manual", "unavailable", "simulated"]
    coverage: Literal["complete", "partial", "since_bridge_start", "unknown"]
    source: str = Field(max_length=40, pattern=ASCII_PRINTABLE)
    as_of_ms: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    stale_after_ms: int = Field(ge=1000, le=86400000)

    @model_validator(mode="after")
    def _semantic_metric(self) -> "Metric":
        _require(METRIC_UNIT[self.key] == self.unit, "Metric key/unit mismatch")
        if self.quality == "unavailable":
            _require(self.value is None, "Unknown metric must be null")
        else:
            _require(self.value is not None, "Known metric needs a value")
            _require(self.as_of_ms is not None, "Known metric needs observation time")
        return self


class Quota(StrictBase):
    id: str = Field(max_length=40, pattern=ASCII_PRINTABLE)
    account_key: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    scope: Literal["account", "provider", "organization", "local_budget"]
    label: str = Field(max_length=24, pattern=ASCII_PRINTABLE)
    kind: Literal["rate_window", "credits", "spend_budget", "unlimited", "unknown"]
    unit: Literal["percent", "credit", "usd_micros", "token", "request", "none"]
    used: Optional[float] = Field(default=None, ge=0)
    limit: Optional[float] = Field(default=None, gt=0)
    remaining: Optional[float] = Field(default=None, ge=0)
    used_pct: Optional[float] = Field(default=None, ge=0)
    resets_at_ms: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    quality: Literal["exact", "estimated", "manual", "unavailable", "simulated"]
    coverage: Literal["complete", "partial", "since_bridge_start", "unknown"]
    source: str = Field(max_length=40, pattern=ASCII_PRINTABLE)
    as_of_ms: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    stale_after_ms: int = Field(ge=1000, le=604800000)
    availability: Literal["available", "needs_auth", "unsupported", "error"]
    reason: str = Field(max_length=40, pattern=ASCII_PRINTABLE)
    shared_with: list[AgentId] = Field(default_factory=list, max_length=4)

    @field_validator("shared_with")
    @classmethod
    def _shared_unique(cls, v: list[str]) -> list[str]:
        _require(len(v) == len(set(v)), "Duplicate agent in shared_with")
        return v


class StatsBody(StrictBase):
    agent_id: AgentId
    selection_rev: int = Field(ge=0, le=2147483647)
    scope: StatsScope
    metrics: list[Metric] = Field(default_factory=list, max_length=6)
    quotas: list[Quota] = Field(default_factory=list, max_length=2)
    sparkline: list[Optional[int]] = Field(default_factory=list, max_length=24)

    @field_validator("sparkline")
    @classmethod
    def _sparkline_range(cls, v: list[Optional[int]]) -> list[Optional[int]]:
        for x in v:
            if x is not None:
                _require(0 <= x <= _MAX_INT, "sparkline value out of range")
        return v

    @model_validator(mode="after")
    def _semantic_stats(self) -> "StatsBody":
        keys = [m.key for m in self.metrics]
        _require(len(keys) == len(set(keys)), "Duplicate metric key")
        qids = [q.id for q in self.quotas]
        _require(len(qids) == len(set(qids)), "Duplicate quota id")
        for q in self.quotas:
            vals = [q.used, q.limit, q.remaining, q.used_pct]
            if q.kind in {"unknown", "unlimited"}:
                _require(all(v is None for v in vals),
                         "Unknown/unlimited cannot use fabricated numbers")
                _require(q.unit == "none", "Unknown/unlimited must have unit=none")
            if q.quality == "unavailable":
                _require(all(v is None for v in vals),
                         "Unavailable quota cannot provide numbers")
            if q.kind == "credits":
                _require(q.unit == "credit", "Credits unit mismatch")
            if q.kind == "spend_budget":
                _require(q.unit == "usd_micros", "Budget unit mismatch")
            _require(self.agent_id not in q.shared_with,
                     "shared_with lists other agents, not self")
            if q.used is not None and q.limit is not None and q.used_pct is not None:
                expected = q.used / q.limit * 100
                _require(abs(expected - q.used_pct) <= 0.1, "Inconsistent percentage")
        return self


class ActionBody(StrictBase):
    action_id: str = Field(pattern=HEX32)
    action: Literal["select_agent", "refresh_stats", "open_agent", "open_usage"]
    agent_id: AgentId
    base_selection_rev: int = Field(ge=0, le=2147483647)


class AckBody(StrictBase):
    action_id: str = Field(pattern=HEX32)
    status: Literal["accepted", "rejected"]
    selected_agent: AgentId
    selection_rev: int = Field(ge=0, le=2147483647)
    reason: Literal["ok", "conflict", "offline", "unsupported", "rate_limited", "invalid", "internal_error"]

    @model_validator(mode="after")
    def _semantic_ack(self) -> "AckBody":
        _require((self.status == "accepted") == (self.reason == "ok"),
                 "ACK result/reason mismatch")
        return self


class NoticeBody(StrictBase):
    notice_id: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    agent_id: AgentId
    run_id: Optional[str] = Field(default=None, max_length=64, pattern=ASCII_PRINTABLE)
    kind: Literal["waiting", "done", "error", "quota_low"]
    label: str = Field(max_length=40, pattern=ASCII_PRINTABLE)
    expires_in_ms: int = Field(ge=500, le=10000)


class PingBody(StrictBase):
    monotonic_ms: int = Field(ge=0, le=_MAX_INT)


class PongBody(StrictBase):
    echo_monotonic_ms: int = Field(ge=0, le=_MAX_INT)
    device_uptime_ms: int = Field(ge=0, le=_MAX_INT)


AnyBody = Union[
    HelloBody, WelcomeBody, CatalogBody, FocusBody, StatsBody,
    ActionBody, AckBody, NoticeBody, PingBody, PongBody,
]

_BODY_FOR_TYPE: dict[str, type[StrictBase]] = {
    "hello": HelloBody,
    "welcome": WelcomeBody,
    "catalog": CatalogBody,
    "focus": FocusBody,
    "stats": StatsBody,
    "action": ActionBody,
    "ack": AckBody,
    "notice": NoticeBody,
    "ping": PingBody,
    "pong": PongBody,
}

MessageType = Literal[
    "hello", "welcome", "catalog", "focus", "stats",
    "action", "ack", "notice", "ping", "pong",
]


class DeviceMessage(StrictBase):
    """Validated device message envelope (shared by HTTP and serial)."""

    v: Literal[1]
    type: MessageType
    link_id: Optional[str] = Field(default=None, pattern=HEX32)
    seq: int = Field(ge=0, le=2147483647)
    body: AnyBody

    @model_validator(mode="after")
    def _link_rule(self) -> "DeviceMessage":
        # mirrors schema allOf[10] and 05_PROTOCOL §2: hello is the only
        # exception allowed to use link_id=null + seq=0
        if self.type == "hello":
            _require(self.link_id is None, "hello must use link_id=null")
            _require(self.seq == 0, "hello must use seq=0")
        else:
            _require(self.link_id is not None, "Missing link for non-hello message")
            _require(self.seq >= 1, "Non-hello seq must start at 1")
        return self

    @model_validator(mode="after")
    def _frame_budget(self) -> "DeviceMessage":
        dumped = self.model_dump(by_alias=False, mode="python")
        _require(len(_compact(dumped)) <= FRAME_LIMIT_BYTES,
                 "Compacted JSON exceeds frame budget")
        _require(_depth(dumped) <= DEPTH_LIMIT, "Nesting exceeds frame budget")
        return self


# ---------------------------------------------------------------------------
# Canonical agent event
# ---------------------------------------------------------------------------

class UsageRecord(StrictBase):
    native_id: str = Field(max_length=96, pattern=ASCII_PRINTABLE)
    account_key: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    provider: str = Field(max_length=40, pattern=ASCII_PRINTABLE)
    model: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    mode: Literal["delta", "cumulative", "authoritative_window"]
    counter_key: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    counter_epoch: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    input_tokens: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    output_tokens: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    total_tokens: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    cache_read_tokens: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    cache_write_tokens: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    reasoning_tokens: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    input_includes_cached: Optional[bool] = None
    cost_usd_micros: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    as_of_ms: int = Field(ge=0, le=_MAX_INT)
    quality: Literal["exact", "estimated", "manual", "unavailable", "simulated"]
    coverage: Literal["complete", "partial", "since_bridge_start", "unknown"]
    window_start_ms: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    window_end_ms: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)

    @model_validator(mode="after")
    def _semantic_usage(self) -> "UsageRecord":
        if self.mode == "authoritative_window":
            _require(self.window_start_ms is not None and self.window_end_ms is not None,
                     "Authoritative window needs boundaries")
        if self.window_start_ms is not None or self.window_end_ms is not None:
            _require(self.window_start_ms is not None and self.window_end_ms is not None,
                     "Both window boundaries are needed")
            _require(self.window_end_ms > self.window_start_ms, "Invalid usage interval")
        if self.mode == "cumulative":
            _require(bool(self.counter_key) and bool(self.counter_epoch),
                     "Cumulative counter needs scope and epoch")
        return self


class AgentEvent(StrictBase):
    v: Literal[1]
    agent_id: AgentId
    source_instance: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    event_id: str = Field(max_length=128, pattern=ASCII_PRINTABLE)
    session_key: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    run_id: Optional[str] = Field(default=None, max_length=64, pattern=ASCII_PRINTABLE)
    kind: Literal[
        "session_opened", "session_closed", "run_started", "tool_started",
        "tool_finished", "tool_failed", "waiting_started", "waiting_cleared",
        "run_finished", "usage_recorded", "source_status",
    ]
    occurred_at_ms: int = Field(ge=0, le=_MAX_INT)
    received_at_ms: int = Field(ge=0, le=_MAX_INT)
    source_seq: Optional[int] = Field(default=None, ge=0, le=_MAX_INT)
    quality: QualityObserved
    tool_call_id: Optional[str] = Field(default=None, max_length=64, pattern=ASCII_PRINTABLE)
    reason: ReasonEnum
    detail: str = Field(default="", max_length=48, pattern=ASCII_PRINTABLE)
    usage_record: Optional[UsageRecord] = None
    source_health: Optional[Literal["ready", "partial", "unavailable", "needs_auth", "disabled"]] = None

    @model_validator(mode="after")
    def _semantic_event(self) -> "AgentEvent":
        for key in ("source_instance", "event_id", "session_key"):
            _require(bool(getattr(self, key)), f"Empty event identifier: {key}")
        if self.kind.startswith("tool_"):
            _require(bool(self.tool_call_id), "Tool event requires id")
        if self.kind in {"run_started", "run_finished", "tool_started", "tool_finished",
                         "tool_failed", "waiting_started", "waiting_cleared"}:
            _require(bool(self.run_id), "Run event requires id")
        if self.kind == "waiting_started":
            _require(self.reason in {"approval", "input"}, "Waiting reason required")
        if self.kind == "run_finished":
            _require(self.reason in {"completed", "failed", "cancelled"},
                     "Run finish reason required")
        if self.kind == "usage_recorded":
            _require(self.usage_record is not None, "usage_recorded needs usage_record")
        return self


# ---------------------------------------------------------------------------
# Capability report
# ---------------------------------------------------------------------------

class CapabilityReport(StrictBase):
    schema_version: Literal[1]
    agent_id: AgentId
    product: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    installed_version: Optional[str] = Field(default=None, max_length=64, pattern=ASCII_PRINTABLE)
    entry_point: Literal["not_detected", "cli", "desktop", "ide", "gateway", "mixed"]
    status: Literal["not_probed", "ready", "partial", "blocked", "not_installed"]
    capabilities: AgentCapabilities
    state_events: list[str] = Field(default_factory=list, max_length=32)
    usage_fields: list[str] = Field(default_factory=list, max_length=16)
    quota_source: str = Field(max_length=64, pattern=ASCII_PRINTABLE)
    last_verified_at: Optional[str] = None
    evidence_files: list[str] = Field(default_factory=list, max_length=32)
    limitations: list[str] = Field(default_factory=list, max_length=16)
    auth_required: bool
    user_accepted_degradation: bool

    @field_validator("state_events")
    @classmethod
    def _state_events_len(cls, v: list[str]) -> list[str]:
        for s in v:
            _require(len(s) <= 64 and all(32 <= ord(c) <= 126 for c in s),
                     "state_events item out of bounds")
        return v

    @field_validator("usage_fields")
    @classmethod
    def _usage_fields_len(cls, v: list[str]) -> list[str]:
        for s in v:
            _require(len(s) <= 40 and all(32 <= ord(c) <= 126 for c in s),
                     "usage_fields item out of bounds")
        return v

    @field_validator("evidence_files")
    @classmethod
    def _evidence_len(cls, v: list[str]) -> list[str]:
        for s in v:
            _require(len(s) <= 256, "evidence_files item too long")
        return v

    @field_validator("limitations")
    @classmethod
    def _limitations_len(cls, v: list[str]) -> list[str]:
        for s in v:
            _require(len(s) <= 300, "limitations item too long")
        return v

    @model_validator(mode="after")
    def _semantic_capability(self) -> "CapabilityReport":
        if self.status in {"ready", "partial"}:
            _require(bool(self.installed_version), "Verified status needs installed version")
            _require(bool(self.last_verified_at) and bool(self.evidence_files),
                     "Verified status needs time and evidence")
        if self.status == "not_probed":
            _require(self.last_verified_at is None,
                     "Unprobed template cannot claim verification")
            _require(self.capabilities.state == "none",
                     "Unprobed cannot claim observed state")
        return self


# ---------------------------------------------------------------------------
# Shared entry points (HTTP and serial both call these)
# ---------------------------------------------------------------------------

def _validate(model: type[StrictBase], raw: bytes, *, bounded: bool = True):
    value = strict_load(raw, bounded=bounded)
    try:
        return model.model_validate(value)
    except ContractError:
        raise
    except Exception as exc:
        raise ContractError(f"{model.__name__} validation failed: {exc}") from exc


def parse_device_message(raw: bytes) -> DeviceMessage:
    return _validate(DeviceMessage, raw, bounded=True)


def parse_agent_event(raw: bytes) -> AgentEvent:
    return _validate(AgentEvent, raw, bounded=False)


def parse_capability_report(raw: bytes) -> CapabilityReport:
    return _validate(CapabilityReport, raw, bounded=False)
