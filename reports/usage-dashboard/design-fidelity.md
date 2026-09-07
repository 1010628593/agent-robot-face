# Approved reference fidelity — correction batch

Reference: approved-design-03.png for device information hierarchy; user reattached design02 for Mac compact status card. Their later solid stage-color/USED requirement supersedes design02 gradient/remaining styling.

## Device visual gate

- Overview: centered title, four equal cells, centered monochrome product icon/name, small quota ring or unbounded numeric Token total, one metric caption, thin cross dividers, bottom tools/dimensions capsule.
- Three-row overview: left small ring, middle tool/window, right primary value, thin separators, restrained page dots.
- Quota detail: top back/Agent/window, dominant 270-degree gauge and large percentage, one used caption, reset countdown, direct quota/Token/cost tabs at bottom.
- Font, geometry and spacing must be reviewed in actual 466×466 LVGL output; artifacts use explicit sample inputs, not claimed hardware data.
- Preserve fixed widgets, equality guards, latest touch and IMU interactions.

## Mac visual gate

- Default panel: product title plus settings gear, four tool tabs, large open quota arc, reset countdown, Today Token row, one detail entry.
- Default width340pt; expanded detailed usage and controls380pt. Black canvas, white primary text, restrained gray secondary text.
- No permanent scope/range/category controls on the status card. They remain reachable inside full details.
- Actual unavailable quotas remain gray dashes; no sample numbers or synthetic totals are used.

## Validation state

Design correction in progress. Native Mac build and local install of first correction passed; final proportion adjustment awaits deployment. Device reference correction and native rendering review in progress. Hardware visual acceptance pending. No fidelity completion claimed.

Additional user reference: https://echarts.apache.org/examples/zh/editor.html?c=gauge-ring . Read the Apache source https://raw.githubusercontent.com/apache/echarts-examples/gh-pages/public/examples/ts/gauge-ring.ts and inspected its live rendering. It is a360-degree, pointerless, non-overlapping concentric progress-ring example with rounded ends. Candidate adaptation is a maximum of three real quota windows per tool, with the user's fixed stage hues; no unbounded Token percentages. Placement clarification pending; existing03 correction continues without replacing its hierarchy.

User confirmed concentric rings belong to multiple quota periods of the SAME tool. Implementing up to three stable-order quota rings per visible group, with per-ring stage color and click-through period details; tool overview remains separate. Unknown/unbounded periods have no colored progress. Native Mac implementation builds successfully; device implementation underway.
