#!/usr/bin/env python3
"""Validate the handoff contracts and synthetic fixtures, not the product.

No device, account, network or vendor configuration is accessed. Requires jsonschema.
Usage: python tools/validate_contracts.py [--report path.json]
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import sys
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

try:
    from jsonschema import Draft202012Validator, FormatChecker
except ImportError:
    raise SystemExit("Install into a dedicated venv: python -m pip install 'jsonschema>=4.22,<5'")

ROOT = Path(__file__).resolve().parents[1]
AGENTS = {"codex", "workbuddy", "cursor", "hermes"}
UNIT = {"turns":"turn", "tool_calls":"call", "requests":"request",
        "input_tokens":"token", "output_tokens":"token", "total_tokens":"token",
        "active_time_ms":"ms", "cost_usd_micros":"usd_micros", "context_tokens":"token"}
STATES = {"idle", "working", "tool", "waiting", "done", "error", "cancelled", "unknown"}

class ContractError(ValueError):
    pass

def require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)

def no_constant(value: str):
    raise ContractError(f"Non-finite JSON constant: {value}")

def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result: dict = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result

def depth(value: object) -> int:
    if isinstance(value, dict):
        return 1 + max((depth(v) for v in value.values()), default=0)
    if isinstance(value, list):
        return 1 + max((depth(v) for v in value), default=0)
    return 0

def finite(value: object) -> None:
    if isinstance(value, float):
        require(math.isfinite(value), "Non-finite numeric value")
    elif isinstance(value, dict):
        for v in value.values(): finite(v)
    elif isinstance(value, list):
        for v in value: finite(v)

def strict_load(raw: bytes, *, bounded: bool = True) -> dict:
    if bounded: require(len(raw) <= 8192, "JSON body exceeds 8192 bytes")
    value = json.loads(raw.decode("utf-8", errors="strict"),
                       object_pairs_hook=unique_object, parse_constant=no_constant)
    require(isinstance(value, dict), "Expected JSON object")
    finite(value)
    if bounded: require(depth(value) <= 12, "JSON nesting exceeds 12 containers")
    return value

def load_file(path: Path) -> dict:
    return strict_load(path.read_bytes(), bounded=False)

def compact(value: dict) -> bytes:
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False,
                      allow_nan=False).encode("utf-8")

def validator(path: str) -> Draft202012Validator:
    schema = load_file(ROOT / path)
    Draft202012Validator.check_schema(schema)
    return Draft202012Validator(schema, format_checker=FormatChecker())

def semantic_message(value: dict) -> None:
    raw = compact(value)
    require(len(raw) <= 8192, "Compacted JSON exceeds frame budget")
    require(depth(value) <= 12, "Nesting exceeds frame budget")
    finite(value)
    t, b = value["type"], value["body"]
    if t == "hello":
        require(bool(b["device_id"]) and bool(b["firmware"]), "Empty device identity")
    elif t == "catalog":
        ids = [a["id"] for a in b["agents"]]
        require(len(ids) == 4 and set(ids) == AGENTS, "Catalog must contain four unique agents")
    elif t == "focus":
        if b["state"] == "waiting":
            require(b["reason"] in {"approval", "input"}, "Waiting needs explicit reason")
        required_reason = {"done":"completed", "error":"failed", "cancelled":"cancelled"}
        if b["state"] in required_reason:
            require(b["reason"] == required_reason[b["state"]], "Terminal reason mismatch")
    elif t == "stats":
        scope = b["scope"]
        require(scope["end_ms"] > scope["start_ms"], "Invalid statistics time interval")
        try: ZoneInfo(scope["timezone"])
        except (ZoneInfoNotFoundError, ValueError):
            raise ContractError("Invalid IANA timezone")
        keys = [m["key"] for m in b["metrics"]]
        require(len(keys) == len(set(keys)), "Duplicate metric key")
        for m in b["metrics"]:
            require(UNIT[m["key"]] == m["unit"], "Metric key/unit mismatch")
            if m["quality"] == "unavailable":
                require(m["value"] is None, "Unknown metric must be null")
            else:
                require(m["value"] is not None, "Known metric needs a value")
                require(m["as_of_ms"] is not None, "Known metric needs observation time")
        qids = [q["id"] for q in b["quotas"]]
        require(len(qids) == len(set(qids)), "Duplicate quota id")
        for q in b["quotas"]:
            vals = [q[k] for k in ("used", "limit", "remaining", "used_pct")]
            if q["kind"] in {"unknown", "unlimited"}:
                require(all(v is None for v in vals), "Unknown/unlimited cannot use fabricated numbers")
                require(q["unit"] == "none", "Unknown/unlimited must have unit=none")
            if q["quality"] == "unavailable":
                require(all(v is None for v in vals), "Unavailable quota cannot provide numbers")
            if q["kind"] == "credits": require(q["unit"] == "credit", "Credits unit mismatch")
            if q["kind"] == "spend_budget": require(q["unit"] == "usd_micros", "Budget unit mismatch")
            require(b["agent_id"] not in q["shared_with"], "shared_with lists other agents, not self")
            if q["used"] is not None and q["limit"] is not None and q["used_pct"] is not None:
                expected = q["used"] / q["limit"] * 100
                require(abs(expected - q["used_pct"]) <= 0.1, "Inconsistent percentage")
    elif t == "ack":
        require((b["status"] == "accepted") == (b["reason"] == "ok"), "ACK result/reason mismatch")

def semantic_event(value: dict) -> None:
    for key in ("source_instance", "event_id", "session_key"):
        require(bool(value[key]), f"Empty event identifier: {key}")
    kind = value["kind"]
    if kind.startswith("tool_"):
        require(bool(value["tool_call_id"]), "Tool event requires id")
    if kind in {"run_started", "run_finished", "tool_started", "tool_finished",
                "tool_failed", "waiting_started", "waiting_cleared"}:
        require(bool(value["run_id"]), "Run event requires id")
    if kind == "waiting_started": require(value["reason"] in {"approval", "input"}, "Waiting reason required")
    if kind == "run_finished": require(value["reason"] in {"completed", "failed", "cancelled"}, "Run finish reason required")
    if kind == "usage_recorded":
        u = value["usage_record"]
        if u["mode"] == "authoritative_window":
            require(u["window_start_ms"] is not None and u["window_end_ms"] is not None,
                    "Authoritative window needs boundaries")
        if u["window_start_ms"] is not None or u["window_end_ms"] is not None:
            require(u["window_start_ms"] is not None and u["window_end_ms"] is not None,
                    "Both window boundaries are needed")
            require(u["window_end_ms"] > u["window_start_ms"], "Invalid usage interval")
        if u["mode"] == "cumulative":
            require(bool(u["counter_key"]) and bool(u["counter_epoch"]), "Cumulative counter needs scope and epoch")

def semantic_capability(value: dict) -> None:
    if value["status"] in {"ready", "partial"}:
        require(bool(value["installed_version"]), "Verified status needs installed version")
        require(bool(value["last_verified_at"]) and bool(value["evidence_files"]),
                "Verified status needs time and evidence")
    if value["status"] == "not_probed":
        require(value["last_verified_at"] is None, "Unprobed template cannot claim verification")
        require(value["capabilities"]["state"] == "none", "Unprobed cannot claim observed state")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()
    count: Counter = Counter()
    failures: list[str] = []
    dv = validator("contracts/device-message.schema.json")
    ev = validator("contracts/agent-event.schema.json")
    cv = validator("contracts/capability-report.schema.json")
    count["schemas"] = 3
    def check_files(folder: str, valid, semantic, label: str) -> None:
        for p in sorted((ROOT/folder).glob("*.json")):
            try:
                value = load_file(p)
                valid.validate(value); semantic(value)
                count[label] += 1
            except Exception as exc:
                failures.append(f"{p.relative_to(ROOT)}: {type(exc).__name__}: {exc}")
    check_files("contracts/examples", dv, semantic_message, "valid_device_examples")
    check_files("contracts/event-examples", ev, semantic_event, "valid_event_examples")
    for p in sorted((ROOT/"acceptance").glob("capability-*.template.json")):
        try:
            c=load_file(p); cv.validate(c); semantic_capability(c)
            count["unprobed_capability_templates"] += 1
        except Exception as exc: failures.append(f"{p.name}: {exc}")
    for p in sorted((ROOT/"contracts/invalid").glob("*.json")):
        if p.name == "README.json": continue
        try:
            v=load_file(p); dv.validate(v); semantic_message(v)
        except Exception:
            count["invalid_examples_rejected"] += 1
        else: failures.append(f"Invalid example was accepted: {p.name}")
    # Parser-level rules that JSON schema by itself does not cover.
    too_deep: object = None
    for _ in range(13): too_deep = {"x":too_deep}
    bad_raw = [b'{"x":1,"x":2}', b'{"x":NaN}', b'{"x":Infinity}',
               b'{"x":1e999}', b'{"x":"\xff"}', b'{}'+b' '*8191,
               compact(too_deep)]
    for raw in bad_raw:
        try: strict_load(raw)
        except Exception: count["invalid_raw_json_rejected"] += 1
        else: failures.append("Malformed raw JSON was accepted")
    # Test-vector integrity only: this is not execution of the firmware reducer.
    for name in ("state_cases", "gesture_cases", "usage_cases"):
        v = load_file(ROOT/f"acceptance/{name}.json")
        require(v["status"] == "NOT_RUN", "Vectors must not pretend product tests passed")
        ids = [c["id"] for c in v["cases"]]
        require(len(ids) == len(set(ids)), "Duplicate acceptance test id")
        if name == "state_cases":
            require(all(c["expected_state"] in STATES for c in v["cases"]), "Unknown expected state")
        if name == "gesture_cases":
            for c in v["cases"]:
                times = [s["t_ms"] for s in c["samples"]]
                require(times == sorted(times), "Touch sample times out of order")
                require(all(0<=s["x"]<466 and 0<=s["y"]<466 for s in c["samples"]), "Touch out of bounds")
        count[name+"_defined_not_executed"] = len(ids)
    tokens = load_file(ROOT/"design/interaction_tokens.json")
    require(tokens["hold_ms"] == 650 and tokens["swipe_min_px"] == 56, "Gesture baseline mismatch")
    count["design_token_checks"] = 1
    sizes=[len(compact(load_file(p))) for p in (ROOT/"contracts/examples").glob("*.json")]
    result={"scope":"Documentation contracts and fixtures only; no hardware or live Agent tests",
            "status":"PASS" if not failures else "FAIL", "checks":dict(count),
            "largest_sample_json_bytes":max(sizes),"frame_json_limit_bytes":8192,
            "product_tests":"NOT_RUN","failures":failures}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return 0 if not failures else 1

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ContractError, ValueError, KeyError, OSError) as exc:
        print(f"Contract validation failed: {exc}",file=sys.stderr)
        raise SystemExit(1)
