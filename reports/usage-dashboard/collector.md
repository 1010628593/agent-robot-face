# Collector delivery / 2026-09-07

Implemented standalone `usage-collector/index.js` with once/watch JSON protocol 1, native fs.watch, stdin refresh, serialized one-graph collection, private caches, and bounded output. Package requires Node >=22.15 and has zero npm runtime dependencies. macOS arm64 Tokscale binary SHA256 matches the reference pin `3cab6986cd23f6fd9c4173ccf61213f1d123f4490a5ce56945e7a7d78c575771`. It uses Token Monitor 0.46.0's minimal MIT collector/config pattern and pinned Tokscale 4.13.0 fork; no runtime reads of the reference checkout, Electron, or Hub.

Three real local 30-day graph/worker runs succeeded before the final canonical-input refinement, yielding 179 unique daily agent/model rows and all four clients, with no local collection errors. Current native Hermes database inspection found 353 sessions and no non-null actual_cost_usd values; null/estimated/unknown were the only cost status groups. Native actual cost code is implemented but no non-null real-data acceptance was possible. Only ended sessions confined to a single local day with explicit actual/exact/reported/billed status qualify; other cases remain null.

Codex and Cursor official quota adapters run only against their fixed official origins with native existing login read in memory. Normal sandbox runs returned quota_probe_failed and all unknown quota metrics remained null. This proves honest unavailable behavior, not successful live official quota retrieval; root will run elevated network acceptance separately. Hermes currently uses a local omlx endpoint/configuration and has no official quota adapter. WorkBuddy quota unsupported. Cost for Codex/Cursor/WorkBuddy remains unknown because no native actual-cost path is implemented; graph price estimates are always discarded.

Syntax checks pass for index.js, quotas.js, hermes-actual.js and the vendor headless collector. No new unit tests. An attempted standalone Python store integration was blocked by the system Python missing pydantic; the parent owns the Bridge environment/integration acceptance. Watch stdin, filtering, serial scans, quota TTL and manual 30-second floor were code-checked; sustained live watch acceptance remains to the parent.

Protocol and deployment details: `docs/usage-collector.md`. Pin provenance and licenses: `usage-collector/vendor-manifest.json`, `usage-collector/vendor/*/LICENSE`. A sanitized final real snapshot excerpt will be appended after the final run; no raw graph or sensitive native stores are delivered.

Final real run passed: unique keys; canonical input + output = total for every fully numeric row; all actual costs and unavailable quota fields stayed null. Full output was 83696 bytes. Sanitized excerpt:

```json
{
  "version": 1,
  "as_of_ms": 1788772204459,
  "timezone": "Asia/Shanghai",
  "range": {
    "since": "2026-08-09",
    "until": "2026-09-07"
  },
  "errors": [],
  "row_count": 179,
  "source_ids": [
    "codex",
    "cursor",
    "hermes",
    "workbuddy"
  ],
  "days_excerpt": [
    {
      "date": "2026-09-07",
      "agent": "workbuddy",
      "model": "mimo-v2.5",
      "input_tokens": 3538961,
      "output_tokens": 2616,
      "cache_read_tokens": 3151168,
      "cache_write_tokens": null,
      "total_tokens": 3541577,
      "actual_cost": null,
      "coverage": {
        "tokens": "partial",
        "cost": "unknown"
      },
      "provenance": "tokscale.local_logs",
      "cache_definition": "input includes cache_read and known cache_write; cache fields are input subsets; cache_write availability unknown"
    },
    {
      "date": "2026-09-07",
      "agent": "workbuddy",
      "model": "mimo-v2.5-pro",
      "input_tokens": 5344756,
      "output_tokens": 23827,
      "cache_read_tokens": 5106752,
      "cache_write_tokens": null,
      "total_tokens": 5368583,
      "actual_cost": null,
      "coverage": {
        "tokens": "partial",
        "cost": "unknown"
      },
      "provenance": "tokscale.local_logs",
      "cache_definition": "input includes cache_read and known cache_write; cache fields are input subsets; cache_write availability unknown"
    }
  ],
  "quotas": [
    {
      "id": "codex",
      "window_key": "unknown",
      "provider": "codex",
      "account_hash": null,
      "agents": [
        "codex"
      ],
      "label": "codex",
      "used_pct": null,
      "remaining": null,
      "limit": null,
      "used": null,
      "unit": "percent",
      "reset_ms": null,
      "as_of_ms": 1788772204488,
      "availability": "unavailable",
      "reason": "quota_probe_failed"
    },
    {
      "id": "cursor",
      "window_key": "unknown",
      "provider": "cursor",
      "account_hash": null,
      "agents": [
        "cursor"
      ],
      "label": "cursor",
      "used_pct": null,
      "remaining": null,
      "limit": null,
      "used": null,
      "unit": "percent",
      "reset_ms": null,
      "as_of_ms": 1788772204490,
      "availability": "unavailable",
      "reason": "quota_probe_failed"
    },
    {
      "id": "hermes",
      "window_key": "unknown",
      "provider": "hermes",
      "account_hash": null,
      "agents": [
        "hermes"
      ],
      "label": "hermes",
      "used_pct": null,
      "remaining": null,
      "limit": null,
      "used": null,
      "unit": "percent",
      "reset_ms": null,
      "as_of_ms": 1788772204490,
      "availability": "unavailable",
      "reason": "provider_adapter_not_configured"
    },
    {
      "id": "workbuddy",
      "window_key": "unknown",
      "provider": "workbuddy",
      "account_hash": null,
      "agents": [
        "workbuddy"
      ],
      "label": "workbuddy",
      "used_pct": null,
      "remaining": null,
      "limit": null,
      "used": null,
      "unit": "percent",
      "reset_ms": null,
      "as_of_ms": 1788772204490,
      "availability": "unavailable",
      "reason": "unsupported"
    }
  ]
}
```
