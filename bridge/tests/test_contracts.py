"""T03 contract tests — write first, watch fail, then implement bot_bridge.models.

Acceptance (docs/10_IMPLEMENTATION_PLAN.md T03):
- all contracts/examples/*.json parse via the shared bridge entry
- all contracts/invalid/*.json rejected — including 06/07 which are semantic errors
- parser-level rejects: duplicate keys, NaN/Infinity, depth>12, >8192 bytes
- capability templates (not_probed) and real capability reports both validate
"""
from __future__ import annotations

import json

import pytest

from bot_bridge.models import (
    ContractError,
    parse_agent_event,
    parse_capability_report,
    parse_device_message,
    strict_load,
)


# ---------- valid examples ----------

def test_all_device_examples_parse(device_example_files):
    for p in device_example_files:
        msg = parse_device_message(p.read_bytes())
        assert msg.type, f"{p.name} produced empty type"
        assert msg.v == 1


def test_all_event_examples_parse(event_example_files):
    for p in event_example_files:
        ev = parse_agent_event(p.read_bytes())
        assert ev.agent_id in {"codex", "workbuddy", "cursor", "hermes"}
        assert ev.event_id


def test_capability_templates_validate(capability_template_files):
    # templates are intentionally not_probed — still must validate structurally
    for p in capability_template_files:
        cap = parse_capability_report(p.read_bytes())
        assert cap.status == "not_probed"


def test_real_capability_reports_validate(real_capability_report_files):
    for p in real_capability_report_files:
        cap = parse_capability_report(p.read_bytes())
        assert cap.agent_id in {"codex", "workbuddy", "cursor", "hermes"}


# ---------- invalid examples must ALL be rejected (incl. 06/07 semantic) ----------

def test_all_invalid_examples_rejected(invalid_files):
    for p in invalid_files:
        with pytest.raises((ContractError, ValueError)):
            parse_device_message(p.read_bytes())


def test_invalid_06_wrong_metric_unit_rejected(invalid_files):
    p = next(f for f in invalid_files if f.name.startswith("06_"))
    with pytest.raises(ContractError):
        parse_device_message(p.read_bytes())


def test_invalid_07_duplicate_agent_rejected(invalid_files):
    p = next(f for f in invalid_files if f.name.startswith("07_"))
    with pytest.raises(ContractError):
        parse_device_message(p.read_bytes())


# ---------- parser-level rules ----------

def test_duplicate_keys_rejected():
    with pytest.raises(ContractError):
        strict_load(b'{"x":1,"x":2}')


def test_nan_rejected():
    with pytest.raises(ContractError):
        strict_load(b'{"x":NaN}')


def test_infinity_rejected():
    with pytest.raises(ContractError):
        strict_load(b'{"x":Infinity}')


def test_overflow_float_rejected():
    with pytest.raises(ContractError):
        strict_load(b'{"x":1e999}')


def test_invalid_utf8_rejected():
    with pytest.raises(ContractError):
        strict_load(b'{"x":"\xff"}')


def test_body_budget_rejected():
    with pytest.raises(ContractError):
        strict_load(b'{}' + b' ' * 8191)


def test_depth_budget_rejected():
    too_deep = None
    for _ in range(13):
        too_deep = {"x": too_deep}
    raw = json.dumps(too_deep).encode()
    with pytest.raises(ContractError):
        strict_load(raw)


def test_non_object_rejected():
    with pytest.raises(ContractError):
        strict_load(b'[1,2,3]')


# ---------- semantic spot checks on bridge entry ----------

def test_focus_terminal_reason_mismatch_rejected():
    # done state must carry reason=completed
    body = {
        "v": 1, "type": "focus", "link_id": "0" * 32, "seq": 4,
        "body": {
            "agent_id": "codex", "selection_rev": 3, "session_key": None,
            "run_id": "run-1", "state": "done", "reason": "none",
            "quality": "observed", "source_age_ms": 0, "stale": False,
            "tool": "", "detail": "", "run_elapsed_ms": 100,
            "active_sessions": 0, "progress": None,
        },
    }
    with pytest.raises(ContractError):
        parse_device_message(json.dumps(body).encode())


def test_stats_unknown_quota_numbers_rejected():
    body = {
        "v": 1, "type": "stats", "link_id": "0" * 32, "seq": 6,
        "body": {
            "agent_id": "codex", "selection_rev": 3,
            "scope": {"kind": "today", "timezone": "Asia/Shanghai",
                      "start_ms": 1, "end_ms": 2},
            "metrics": [],
            "quotas": [{
                "id": "q1", "account_key": "k", "scope": "account",
                "label": "Q", "kind": "unknown", "unit": "none",
                "used": 5, "limit": 10, "remaining": 5, "used_pct": 50,
                "resets_at_ms": None, "quality": "unavailable",
                "coverage": "unknown", "source": "x", "as_of_ms": None,
                "stale_after_ms": 1000, "availability": "unknown",
                "reason": "", "shared_with": [],
            }],
            "sparkline": [],
        },
    }
    with pytest.raises(ContractError):
        parse_device_message(json.dumps(body).encode())


def test_ack_status_reason_mismatch_rejected():
    body = {
        "v": 1, "type": "ack", "link_id": "0" * 32, "seq": 11,
        "body": {
            "action_id": "a" * 32, "status": "accepted",
            "selected_agent": "codex", "selection_rev": 4,
            "reason": "conflict",
        },
    }
    with pytest.raises(ContractError):
        parse_device_message(json.dumps(body).encode())


def test_capability_unprobed_cannot_claim_verified():
    body = {
        "schema_version": 1, "agent_id": "codex", "product": "Codex",
        "installed_version": "0.152.0", "entry_point": "cli",
        "status": "not_probed",
        "capabilities": {"state": "none", "usage": "none", "quota": "none",
                         "open_agent": False, "open_usage": False},
        "state_events": [], "usage_fields": [], "quota_source": "",
        "last_verified_at": "2026-09-06T13:12:26Z",  # must be null for not_probed
        "evidence_files": [], "limitations": [],
        "auth_required": False, "user_accepted_degradation": False,
    }
    with pytest.raises(ContractError):
        parse_capability_report(json.dumps(body).encode())


def test_usage_cumulative_counter_rules():
    body = {
        "v": 1, "agent_id": "hermes", "source_instance": "hermes-cli-0.21.0",
        "event_id": "evt-1", "session_key": "sess-1", "run_id": "run-1",
        "kind": "usage_recorded", "occurred_at_ms": 1, "received_at_ms": 2,
        "source_seq": 1, "quality": "observed", "tool_call_id": None,
        "reason": "none", "detail": "",
        "usage_record": {
            "native_id": "n1", "account_key": "k", "provider": "openai",
            "model": "m", "mode": "cumulative",
            "counter_key": "", "counter_epoch": "",  # must be non-empty for cumulative
            "input_tokens": 1, "output_tokens": 1, "total_tokens": 2,
            "cache_read_tokens": None, "cache_write_tokens": None,
            "reasoning_tokens": None, "input_includes_cached": None,
            "cost_usd_micros": None, "as_of_ms": 1,
            "quality": "exact", "coverage": "complete",
            "window_start_ms": None, "window_end_ms": None,
        },
    }
    with pytest.raises(ContractError):
        parse_agent_event(json.dumps(body).encode())
