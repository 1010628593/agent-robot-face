"""Bridge runtime v2 plus retained v1 contract validation for historical fixtures.

Production USB uses bot_bridge.usb and docs/protocol-v2.md. The exported
parse_device_message is the legacy fixture validator, never the production link.
"""
from .models import (
    AgentEvent,
    CapabilityReport,
    ContractError,
    DeviceMessage,
    parse_agent_event,
    parse_capability_report,
    parse_device_message,
    strict_load,
)

__all__ = [
    "AgentEvent",
    "CapabilityReport",
    "ContractError",
    "DeviceMessage",
    "parse_agent_event",
    "parse_capability_report",
    "parse_device_message",
    "strict_load",
]
