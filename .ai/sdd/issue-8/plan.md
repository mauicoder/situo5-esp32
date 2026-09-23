# Technical Plan

## Architecture

Add a browser-managed configuration layer between the existing Web Serial flashing flow and the firmware's runtime Wi-Fi/MQTT startup logic. The browser flow will collect local Wi-Fi and optional MQTT values, flash the selected firmware, and then use a compact versioned serial protocol to write a project-owned configuration record to the device's NVS. Firmware remains the authority for reading, validating, and committing the stored configuration; the browser never writes raw partition data or depends on undocumented `WiFiProv` keys.

This implementation deliberately stays in the browser/Web Serial workflow. The shared serial protocol and configuration schema remain structured so they can be reused by a future CLI, but that CLI is out of scope for this issue.

The configured flow will be:

1. Validate local form values as UTF-8 byte strings and port values.
2. Flash the selected existing firmware manifest.
3. Wait for the ESP32 to boot into the serial protocol, reset/reconnect as required by Web Serial.
4. Issue `READ_CONFIG`, merge the requested fields with the current project-owned record, issue a transactional `WRITE_CONFIG`, and then read back and verify the stored value.
5. Report success only after durable verification succeeds.

Firmware-only flashing will retain the current manifest-selection behavior and will not enter the configuration protocol. On boot, a valid project-owned Wi-Fi record will take precedence over BLE provisioning. If no valid record exists, the current BLE QR flow remains the fallback.

## Existing code affected

* `src/ConfigManager.h/.cpp`: retain existing `iohome/devices` and `mqtt` behavior, and add a versioned project configuration record with validation, transactional persistence, readback, and compatibility handling.
* `src/main_IoHome.cpp`: replace unconditional provisioning startup with bounded startup precedence and recovery logic, while preserving the QR/OLED BLE flow as the fallback path.
* `src/MqttManager.h/.cpp`: consume authoritative project MQTT values without changing the runtime discovery/command model or the existing portal behavior.
* `src/IoHomeWebSniffer.cpp`: keep `/mqttcfg` operational and synchronize its state with the new project-owned config rules instead of introducing incompatible storage.
* `platformio.ini`, `scripts/build_web_flasher.py`, and the generated flasher artifacts: support all four board/profile environments and validate the effective PlatformIO partition/config information rather than relying on hardcoded assumptions.
* `docs/flasher.md` and `README.md`: document the configured-web setup, firmware-only mode, recovery flow, supported browsers, and validation evidence.
* Existing host tests in `tests/`: remain green; add new host-side protocol, validation, transaction, and compatibility coverage without real credentials.

## New components

* A firmware configuration model and validator for Wi-Fi, optional MQTT, schema version, record length, state metadata, generation/sequence, and integrity checksum.
* A compact framed USB serial command handler with explicit commands for capability, read, prepare/write, commit/verify, and explicit project reset. Frames include magic, protocol/schema version, command type, payload length, transaction identifier, payload, and checksum. Malformed, incomplete, or checksum-invalid frames return explicit errors.
* A browser-side configured-flash module embedded in `docs/flasher.md` or the generated flasher page, with local form state, UTF-8 byte validation, Web Serial lifecycle management, bounded retries, status/error rendering, and firmware-only bypass.
* A build/helper step to generate and validate a configuration NVS image or companion config binary for the selected board/profile, tied to the same manifest generation flow used by the web flasher.
* Host-side fake serial/NVS tests for disconnects, power interruption, malformed records, failed commits, and readback mismatch.
* Build/package inspection helpers that record effective partition tables, NVS regions, image offsets, and manifest correctness for each environment.

## Data structures and interfaces

Define one packed, explicitly serialized wire record rather than serializing compiler-dependent C++ padding. The record must contain:

* fixed magic and protocol/schema version;
* record type and encoded payload length;
* generation/transaction state and checksum;
* Wi-Fi SSID and password with explicit lengths;
* MQTT enabled state, server, username, password, base topic, and uint16 port.

The logical limits are SSID 1–32 bytes, Wi‑Fi password 0–63 bytes, MQTT server 1–63 bytes when enabled, username 0–31 bytes, password 0–63 bytes, base topic 1–31 bytes, and port 1–65535. Empty MQTT server means disabled MQTT; enabled defaults are base topic `iown` and port `1883`.

Expose typed firmware operations equivalent to:

* `readProjectConfig()`;
* `validateProjectConfig(record)`;
* `prepareProjectConfig(record)`;
* `commitProjectConfig(generation)`;
* `verifyProjectConfig(expected)`;
* `resetProjectConfig()` only via an explicit, confirmed command.

The serial protocol returns structured status/error codes, request IDs, and readback data. It must not echo secrets in normal logs or error strings. Browser test vectors and firmware validation tests will prove byte-for-byte schema compatibility.

## Persistence / NVS design

Use a dedicated key in the existing `iohome` namespace (for example, `project_config`) owned exclusively by this feature. The existing `devices` key remains untouched, including migration and all learned keys, addresses, descriptions, and sequence counters. The existing `mqtt` key remains readable and writable for `/mqttcfg` compatibility.

Use two versioned transaction slots or an equivalent staged record plus commit marker/generation. Write the complete candidate and integrity metadata, verify the stored candidate, and only then advance the commit marker. Load only the latest complete, checksum-valid committed record. On interruption, malformed data, failed commit, or failed verification, retain and load the prior committed record. Never erase the full NVS partition.

At boot, project Wi‑Fi is authoritative when valid. MQTT synchronization remains explicit: a valid project MQTT value is applied to runtime state and legacy `iohome/mqtt` is updated only through a deliberate compatibility adapter; legacy-only devices continue to load their existing MQTT values unchanged. Incompatible legacy bytes are rejected rather than reinterpreted. `/mqttcfg` will continue to update the existing runtime/legacy structure and synchronize the project record using the same validator and transaction path, with precedence and failure behavior documented.

Project-config reset invalidates only the project-owned record, requires protocol/UI confirmation, and leaves `iohome/devices`, legacy MQTT, and unrelated keys intact.

## Browser / web behavior where applicable

Extend the existing Web Flasher UI with a configuration section that includes SSID/password fields and optional MQTT server, username, password, base-topic, and port fields. Defaults are displayed locally; no import file is required. Values are held only in page memory, never placed in URLs, manifests, generated assets, or default logs.

Use Web Serial directly after ESP Web Tools finishes flashing. Detect the supported Chromium desktop environment, select and retain the same serial port, perform the required reset/reconnect sequence programmatically where possible, and expose a single defined reconnect step only if the browser requires it. Configure only when the user supplies Wi‑Fi credentials; otherwise retain the existing firmware-only behavior. Use a 5-second command timeout and at most 3 retries, with explicit statuses for permission, reset, connection, timeout, disconnect, write, verification, and unsupported-browser failures.

The browser flow must support both the configured-flash path and the current firmware-only path without violating the existing board/profile selection model or the existing BLE provisioning fallback.

## Firmware behavior where applicable

Register the serial protocol early enough after boot for the post-flash client to connect, while allowing normal radio/application initialization to continue. Load and validate the project record before Wi‑Fi startup. For a valid record, attempt Wi‑Fi at most 3 times, each for 15 seconds, then wait 2 seconds before bounded recovery handling. A successful connection skips BLE provisioning. Empty MQTT starts normally and retains the current unconfigured-MQTT display/runtime handling; configured MQTT failures cannot invalidate stored Wi‑Fi or block radio startup.

If no valid project record exists, or stored Wi‑Fi exhausts retries, report a clear serial/OLED recovery state, avoid indefinite blocking, and expose the documented path to re-enter BLE provisioning or replace the record. Preserve `PROV_IoHome`, PoP `iown1234`, QR/OLED rendering, and existing portal behavior.

## Error handling

Reject invalid UTF-8, empty required fields, over-limit fields, invalid ports, unsupported versions, wrong lengths, bad checksums, incomplete transactions, stale generations, and invalid commands before persistence. Return explicit machine-readable errors and human-actionable text without secret contents.

Treat serial disconnects, timeouts, reset failures, power loss, retry exhaustion, failed NVS commit, and readback mismatch as failures. Do not advance the commit marker until verification succeeds, and never display a success result after a failed verification. Firmware recovery must continue to radio/application startup and provide provisioning/configuration recovery instead of looping indefinitely.

## Compatibility

Support `heltec_wifi_lora_32_V3`, `heltec_wifi_lora_32_V3_debug`, `lilygo_t3_v16`, and `lilygo_t3_v16_debug` without changing existing board selection semantics. Inspect effective PlatformIO output for each environment and generate/check manifests from those outputs.

Existing unconfigured devices, BLE provisioning, `iohome/devices` migration, legacy `iohome/mqtt`, `/mqttcfg`, MQTT discovery, radio behavior, and device profile enrollment remain supported. No OTA, mobile-browser, raw browser NVS rewrite, full NVS erase, new board, or replacement MQTT/radio implementation is included.

## Testing strategy

* Add host unit tests for UTF-8 byte counts, all field limits, defaults, optional MQTT, ports, schema golden vectors, version/length/checksum rejection, and secret masking.
* Add protocol tests with mocked serial for framing, command sequencing, 5-second timeout/3-retry behavior, reset/reconnect, malformed requests, disconnects, interrupted writes, failed commits, and readback mismatch.
* Add mocked NVS read/modify/write tests and before/after snapshots proving only the project-owned record changes; include project reset and legacy MQTT synchronization/precedence cases.
* Add firmware integration tests for boot precedence, 3 × 15-second Wi‑Fi retry policy, 2-second recovery delay, MQTT-disabled startup, MQTT failure isolation, BLE fallback, OLED/serial recovery status, and non-blocking startup.
* Add browser integration tests for all four selection combinations, valid Wi‑Fi-only and Wi‑Fi-plus‑MQTT flows, firmware-only mode, unsupported-browser messaging, no URL secret leakage, and no-success-after-failure.
* Run existing host tests and all four PlatformIO builds. Inspect effective partition/NVS layout and generated artifact offsets independently for every environment.

## Hardware validation strategy

Run release and debug as applicable on both Heltec WiFi LoRa 32 V3 (ESP32-S3) and LilyGo T3 V1.6 (ESP32). On each family verify:

* flash, automatic reset, USB permission, serial reconnect, configured write/readback, and firmware-only flow;
* Wi‑Fi-only and Wi‑Fi-plus‑MQTT setup, reset/power-cycle persistence, and firmware-only update persistence;
* empty-MQTT startup, broker connection attempt, portal update, BLE QR fallback, failed-Wi‑Fi recovery, and OLED/serial statuses;
* disconnect, timeout, retry exhaustion, interrupted write, and power-loss behavior with confirmation that the prior valid record survives.

Record the exact desktop Chromium browser and Windows 11/Linux versions used for AC-015 in the documentation and test evidence. Hardware checks must use only fake/non-production credentials and must not publish them.

## Requirement traceability

| Requirement | Design decision | Files | Test |
|---|---|---|---|
| FR-001 | Add separate SSID/password fields; no import-file path. | `docs/flasher.md`, browser flash module | Browser form test |
| FR-002 | Validate UTF-8 byte lengths before serial write; never truncate. | Browser module, firmware validator | Validation boundary tests |
| FR-003 | Empty MQTT server serializes as disabled MQTT. | Shared schema, `ConfigManager`, `MqttManager` | Wi‑Fi-only/empty-MQTT tests |
| FR-004 | Validate enabled MQTT limits and uint16 port; default `iown`/1883. | Shared schema, browser, firmware | Field/default/port tests |
| FR-005 | Use explicit version, lengths, transaction metadata, and checksum in shared framing/record. | New protocol/schema definitions, browser, firmware | Golden vectors and malformed-record tests |
| FR-006 | Browser performs post-flash read/merge/write; firmware owns NVS access. | Browser module, `ConfigManager`, `main_IoHome.cpp` | Mock serial/NVS integration; AC-001 |
| FR-007 | Restrict writes to a dedicated project record and preserve devices/legacy/unrelated keys. | `ConfigManager` | NVS snapshot/diff tests; AC-008 |
| FR-008 | Two-slot staged commit with prior-generation fallback. | Transaction layer in `ConfigManager` | Fault injection; AC-007 |
| FR-009 | Verify durable readback and return explicit status; never claim success after failed verification. | Firmware protocol, browser | Verification-failure tests |
| FR-010 | Automate Web Tools-to-serial reset/reconnect with one defined fallback step. | Browser module, `docs/flasher.md` | Browser integration; AC-015 |
| FR-011 | Load valid project Wi-Fi, skip BLE, and enforce 3 attempts × 15 seconds + 2 seconds. | `main_IoHome.cpp`, `ConfigManager` | Firmware timing/integration; AC-002 |
| FR-012 | Preserve BLE provisioning constants and QR/OLED flow when no valid record exists. | `main_IoHome.cpp` | Firmware fallback; AC-005 |
| FR-013 | Bounded recovery status and a documented provisioning/replacement path. | Firmware startup/UI, docs | Recovery hardware and timeout tests |
| FR-014 | Empty MQTT remains unconfigured; MQTT failures cannot reject Wi‑Fi. | `MqttManager`, `ConfigManager` | MQTT-disabled/unreachable tests; AC-004 |
| FR-015 | Keep legacy `iohome/mqtt` and `/mqttcfg`; document explicit precedence and sync. | `ConfigManager`, `IoHomeWebSniffer.cpp`, docs | Compatibility/portal tests; AC-009 |
| FR-017 | Build/package matrix covers four environments and inspects effective layouts. | `platformio.ini`, `scripts/build_web_flasher.py`, CI | Four build/layout checks; AC-011 |
| FR-018 | Keep no-config firmware-only manifest selection for board/profile. | Browser module, `docs/flasher.md` | Four-mode flasher tests; AC-012 |
| FR-019 | Never full-erase NVS; optional confirmed reset invalidates only project record. | `ConfigManager`, browser protocol | NVS diff/reset tests; AC-013 |
| FR-020 | Document configured, firmware-only, and recovery flows plus validation versions. | `README.md`, `docs/flasher.md` | Documentation review and walkthrough |
| FR-021 | Keep credentials in local browser memory/serial operations; mask logs and scan artifacts. | Browser, docs, build scripts | Secret scans; AC-014 |
| FR-022 | Centralize 5-second/3-retry serial and 3 × 15-second/2-second Wi‑Fi timing constants. | Shared protocol constants, browser, firmware | Timing and interruption tests |
| AC-001 | Complete the configured browser flow and gate success on verified readback. | Browser module, firmware protocol | Browser/host integration |
| AC-002 | Validate Wi‑Fi-only persistence and no BLE on both physical board families. | Firmware, hardware procedure/docs | Reset/power-cycle hardware run |
| AC-003 | Persist all MQTT fields and connect/attempt without portal. | `ConfigManager`, `MqttManager`, browser | Hardware broker test |
| AC-004 | Treat empty MQTT as normal unconfigured startup. | Firmware startup/display, `MqttManager` | Firmware and hardware test |
| AC-005 | Preserve firmware-only BLE QR/device flow. | Browser bypass, firmware provisioning | Four-manifest and hardware fallback test |
| AC-006 | Reject invalid inputs/records and all transport/verification failures with no success. | Shared validator, protocol, browser | Negative/fault-injection suite |
| AC-007 | Preserve prior committed record under disconnect/timeout/power/retry faults. | Transactional NVS layer | Fault-injection and power-loss hardware test |
| AC-008 | Prove only the project record changes using NVS snapshots/diffs. | NVS test harness, `ConfigManager` | Snapshot/diff suite |
| AC-009 | Keep `/mqttcfg` usable and verify documented sync/precedence. | `IoHomeWebSniffer.cpp`, config adapter, docs | Portal compatibility test |
| AC-011 | Build all environments and verify board-specific effective layout/offsets. | PlatformIO config and packaging script | Matrix build/artifact inspection |
| AC-012 | Retain firmware-only operation for all four board/profile choices. | Browser flasher/docs | Browser manifest matrix test |
| AC-013 | Require confirmed project-only reset and preserve devices/unrelated data. | Protocol, browser, `ConfigManager` | Reset confirmation/NVS diff test |
| AC-014 | Ensure no credentials in repository or generated/published artifacts. | Scanning script/CI, docs/build outputs | Automated secret scan |
| AC-015 | Validate continuous Windows 11/Linux Chromium flow and unsupported-browser message. | Browser module/docs | Recorded hardware walkthrough |
| AC-016 | Preserve existing host tests and complete relevant PlatformIO build coverage. | Tests, CI, firmware/build files | Host suite and four builds |

## Implementation risks

* The browser and ESP Web Tools toolchain may not expose a stable enough reset/reconnect contract across all Chromium builds.
* The ESP32 NVS namespace and Wi‑Fi provisioning storage layout are board- and firmware-dependent; the project-owned record must be versioned and validated before use.
* Optional MQTT settings and legacy `/mqttcfg` compatibility can conflict if precedence is not documented and tested.
* Browser serial operations are permission- and timeout-sensitive; the design must fail closed and report actionable errors.
* PlatformIO partition layouts differ by board family; manifest generation must inspect actual outputs rather than assume a single layout.

## Explicit non-goals

* No Python or desktop CLI implementation for this issue.
* No raw browser NVS writes or full partition erases.
* No OTA or cloud-based provisioning service.
* No replacement of the existing BLE provisioning flow when no valid stored config is present.
* No changes to the existing `iohome/devices` storage model or radio protocol behavior.
* No commitment to a long-term CLI path in this implementation; the protocol and schema remain reusable for a future, separate CLI work item.

PLAN READY FOR REVIEW
