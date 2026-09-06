"""Bot Status bridge package (T03).

Single validation entry for HTTP and serial:
  parse_device_message(raw: bytes) -> DeviceMessage
  parse_agent_event(raw: bytes) -> AgentEvent
  parse_capability_report(raw: bytes) -> CapabilityReport
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
