"""Shared pytest fixtures for contract tests.

Fixtures point at the handoff package contracts (the authority). Tests must
not copy contract JSON into this repo — always read from the source of truth.
"""
from __future__ import annotations

from pathlib import Path

import pytest

# repo root = bridge/.. ; handoff contracts live there
REPO_ROOT = Path(__file__).resolve().parents[2]
HANDOFF = REPO_ROOT / "Bot_Status_v1_Handoff"
CONTRACTS = HANDOFF / "contracts"
EXAMPLES = CONTRACTS / "examples"
EVENT_EXAMPLES = CONTRACTS / "event-examples"
INVALID = CONTRACTS / "invalid"
CAPABILITY_TEMPLATES = HANDOFF / "acceptance"
REAL_CAPABILITY_REPORTS = REPO_ROOT / "reports" / "capabilities"


def _json_files(folder: Path) -> list[Path]:
    return sorted(p for p in folder.glob("*.json") if p.name != "README.json")


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


@pytest.fixture(scope="session")
def contracts_dir() -> Path:
    assert (CONTRACTS / "device-message.schema.json").exists(), "handoff contracts missing"
    return CONTRACTS


@pytest.fixture(scope="session")
def device_example_files() -> list[Path]:
    files = _json_files(EXAMPLES)
    assert len(files) == 18, f"expected 18 device examples, got {len(files)}"
    return files


@pytest.fixture(scope="session")
def event_example_files() -> list[Path]:
    files = _json_files(EVENT_EXAMPLES)
    assert len(files) == 3, f"expected 3 event examples, got {len(files)}"
    return files


@pytest.fixture(scope="session")
def invalid_files() -> list[Path]:
    files = _json_files(INVALID)
    assert len(files) == 9, f"expected 9 invalid examples, got {len(files)}"
    return files


@pytest.fixture(scope="session")
def capability_template_files() -> list[Path]:
    files = sorted(CAPABILITY_TEMPLATES.glob("capability-*.template.json"))
    assert len(files) == 4, f"expected 4 capability templates, got {len(files)}"
    return files


@pytest.fixture(scope="session")
def real_capability_report_files() -> list[Path]:
    files = sorted(REAL_CAPABILITY_REPORTS.glob("*.json"))
    assert len(files) == 4, f"expected 4 real capability reports, got {len(files)}"
    return files
