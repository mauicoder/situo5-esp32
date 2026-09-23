# Technical Plan

## Architecture

Add a project-owned configuration layer between the existing Web Serial
firmware flashing flow and the runtime `ConfigManager`/Wi-Fi/MQTT startup
logic. The browser and CLI will share a documented binary schema and command
semantics, while firmware remains the authority for NVS read/modify/write.
Neither client will write partition sectors or depend on undocumented
`WiFiProv` keys.

The configured flow will be:

1. Validate local form/CLI values as UTF-8 byte strings and port values.
2. Flash the selected existing manifest.
3. Wait for the firmware's USB serial boot/protocol readiness, reset and
   reconnect as required by Web Serial.
4. Issue `READ_CONFIG`, merge the requested fields with the current
   project-owned record, issue a transactional `WRITE_CONFIG`, and issue
   `READ_CONFIG`/verification.
5. Report success only after firmware confirms durable readback.

Firmware-only flashing will retain the current manifest selection and will not
enter the configuration protocol. Boot will load a valid project-owned Wi-Fi
record first; otherwise it will use the existing BLE provisioning flow.

## Existing code affected

* `src/ConfigManager.h/.cpp`: retain existing `iohome/devices` behavior and
  add the versioned project configuration record, validation, transactional
  persistence, readback, migration/synchronization policy, and protocol
  service.
* `src/main_IoHome.cpp` and `src/UIManager.cpp`: replace unconditional
  provisioning startup with bounded stored-Wi-Fi connection/recovery logic,
  while keeping the documented BLE QR flow as fallback and keeping OLED and
  serial status consistent.
* `src/MqttManager.h/.cpp`: consume the authoritative project MQTT values
  without changing runtime discovery/command behavior; preserve the existing
  `MqttConfig` interface where possible.
* `src/IoHomeWebSniffer.cpp`: keep `/mqttcfg` operational and route its
  updates through the documented synchronization policy rather than silently
  creating incompatible records.
* `platformio.ini`, `scripts/build_web_flasher.py`, and generated flasher
  artifacts: support all four environments and derive/check effective
  PlatformIO partition/flash information rather than relying only on hardcoded
  assumptions.
* `docs/flasher.md` and `README.md`: document both setup modes, recovery,
  supported boards/browsers, security boundaries, and recorded validation
  versions.
* Existing host tests in `tests/`: remain passing; add host-side protocol,
  validation, transaction, and compatibility coverage without real secrets.

## New components

* A firmware configuration model and validator for Wi-Fi, optional MQTT,
  schema version, record length, transaction state, generation/sequence, and
  integrity checksum.
* A small framed USB serial command handler with explicit commands for
  capability/read, write preparation/commit, verification, and optional
  project-config reset. Frames include magic, protocol/schema version, command,
  payload length, request identifier, payload, and checksum; malformed,
  unsupported, incomplete, or checksum-invalid frames receive explicit errors.
* A browser-side configured-flash module embedded in the flasher documentation
  or its generated site asset, with local form state, UTF-8 byte validation,
  Web Serial lifecycle management, bounded command retries, status/error
  rendering, and firmware-only bypass.
* A local Python CLI under `scripts/` using the same schema constants and
  framing rules, with serial selection, local validation, read/modify/write,
  verification, retry/timeout handling, and no network credential endpoint.
* Host-test fakes for serial transport, NVS storage, reset/disconnect, power
  interruption, malformed records, and readback failure.
* Build/package inspection helpers that record effective partition tables,
  NVS regions, image offsets, and manifest correctness for each environment.

## Data structures and interfaces

Define one packed/explicitly serialized wire record rather than serializing
compiler-dependent C++ padding. The record contains:

* fixed magic and protocol/schema version;
* record type and encoded payload length;
* generation/transaction state and checksum;
* Wi-Fi SSID and password with explicit byte lengths;
* MQTT enabled state, server, username, password, base topic, and uint16 port.

The logical limits are SSID 1–32 bytes, Wi-Fi password 0–63 bytes, MQTT server
1–63 bytes when enabled, username 0–31 bytes, password 0–63 bytes, base topic
1–31 bytes, and port 1–65535. Empty MQTT server produces a disabled MQTT
record; enabled defaults are base topic `iown` and port `1883`.

Expose typed firmware operations equivalent to:

* `readProjectConfig()`;
* `validateProjectConfig(record)`;
* `prepareProjectConfig(record)`;
* `commitProjectConfig(generation)`;
* `verifyProjectConfig(expected)`;
* `resetProjectConfig()` (only through an explicit, confirmed command).

The serial protocol returns structured status/error codes, request IDs, and
readback data. It must not echo secrets in normal logs or error strings.
Browser and CLI tests will use golden vectors to prove byte-for-byte schema
compatibility.

## Persistence / NVS design

Use a dedicated key in the existing `iohome` namespace (for example,
`project_config`) owned exclusively by this feature. The existing `devices`
key remains untouched, including migration and all learned keys, addresses,
descriptions, and sequence counters. The existing `mqtt` key remains readable
and writable for `/mqttcfg` compatibility.

Use two versioned transaction slots or an equivalent staged record plus
commit marker/generation. Write the complete candidate and integrity metadata,
verify the stored candidate, then advance the commit marker; load only the
latest complete, checksum-valid committed record. On interruption, malformed
data, failed commit, or failed verification, retain and load the prior
committed record. Never erase the full NVS partition.

At boot, project Wi-Fi is authoritative when valid. MQTT synchronization is
explicit: a valid project MQTT value is applied to runtime and legacy
`iohome/mqtt` is updated only through a deliberate compatibility adapter;
legacy-only devices continue to load legacy MQTT unchanged. Incompatible
legacy bytes are rejected rather than reinterpreted. `/mqttcfg` continues to
update the existing runtime/legacy structure and synchronizes the project
record using the same validator and transaction path, with precedence and
failure behavior documented.

Project-config reset deletes/invalidates only the project-owned record,
requires protocol/UI confirmation, and leaves `iohome/devices`, legacy MQTT,
and unrelated keys intact.

## Browser / web behavior where applicable

Extend the existing Web Flasher UI with separate SSID/password fields and
optional MQTT server, username, password, base-topic, and port fields.
Defaults are shown locally; no import file is required. Values are held only
in page memory, never put in URLs, manifests, generated assets, or default
logs.

Use Web Serial directly after ESP Web Tools finishes flashing. Detect the
supported Chromium desktop environment, select/retain the same serial port,
perform the required reset/reconnect sequence programmatically where
possible, and expose one clearly defined reconnect step only if the browser
requires it. Configure only when the user supplied Wi-Fi credentials; retain
firmware-only behavior otherwise. Use a 5-second command timeout and at most
3 retries, with explicit statuses for permission, reset, connection,
timeout, disconnect, write, verification, and unsupported browser failures.

## CLI behavior where applicable

Provide a desktop Python command that accepts board/port and local Wi-Fi/MQTT
arguments, validates before opening/writing the device, and performs the same
read/modify/write/readback sequence and bounded retry policy as the browser.
Support a read/status operation and deliberate project-config reset with
confirmation. Mask secrets in output, never contact a remote service, and
return nonzero exit status plus actionable diagnostics on any failed
validation, transport, commit, or verification step.

## Firmware behavior where applicable

Register the serial protocol early enough after boot for the post-flash client
to connect, while allowing normal radio/application initialization to
continue. Load and validate the project record before Wi-Fi startup. For a
valid record, attempt Wi-Fi at most 3 times, each for 15 seconds, then wait
2 seconds before bounded recovery handling. A successful connection skips BLE
provisioning. Empty MQTT starts normally and retains the current
unconfigured-MQTT display/runtime behavior; configured MQTT failures cannot
invalidate stored Wi-Fi or block radio startup.

If no valid project record exists, or stored Wi-Fi exhausts retries, report a
clear serial/OLED recovery state, avoid indefinite blocking, and expose the
documented path to re-enter BLE provisioning or replace the record. Preserve
`PROV_IoHome`, PoP `iown1234`, QR/OLED rendering, and existing portal behavior.

## Error handling

Reject invalid UTF-8, empty required fields, over-limit fields, invalid ports,
unsupported versions, wrong lengths, bad checksums, incomplete transactions,
stale generations, and invalid commands before persistence. Return explicit
machine-readable errors and human-actionable text without secret contents.

Treat serial disconnects, timeouts, reset failures, power loss, retry
exhaustion, failed NVS commit, and readback mismatch as failures. Do not
advance the commit marker until verification succeeds, and never display a
success result after a failed verification. Firmware recovery must continue
to radio/application startup and provide provisioning/configuration recovery
instead of looping indefinitely.

## Compatibility

Support `heltec_wifi_lora_32_V3`, `heltec_wifi_lora_32_V3_debug`,
`lilygo_t3_v16`, and `lilygo_t3_v16_debug` without changing existing board
selection semantics. Inspect effective PlatformIO output for each environment
and generate/check manifests from those outputs.

Existing unconfigured devices, BLE provisioning, `iohome/devices` migration,
legacy `iohome/mqtt`, `/mqttcfg`, MQTT discovery, radio behavior, and device
profile enrollment remain supported. No OTA, mobile-browser, raw browser NVS
rewrite, full NVS erase, new board, or replacement MQTT/radio implementation
is included.

## Testing strategy

* Add host unit tests for UTF-8 byte counts, all field limits, defaults,
  optional MQTT, ports, schema golden vectors, version/length/checksum
  rejection, and secret masking.
* Add protocol tests with mocked serial for framing, command sequencing,
  5-second timeout/3-retry behavior, reset/reconnect, malformed requests,
  disconnects, interrupted writes, failed commits, readback mismatch, and
  CLI/browser parity.
* Add mocked NVS read/modify/write tests and before/after snapshots proving
  only the project-owned record changes; include project reset and legacy
  MQTT synchronization/precedence cases.
* Add firmware integration tests for boot precedence, 3 x 15-second Wi-Fi
  retry policy, 2-second recovery delay, MQTT-disabled startup, MQTT failure
  isolation, BLE fallback, OLED/serial recovery status, and non-blocking
  startup.
* Add browser integration tests for all four selection combinations, valid
  Wi-Fi-only and Wi-Fi-plus-MQTT flows, firmware-only mode, unsupported
  browser messaging, no URL secret leakage, and no-success-after-failure.
* Add CLI integration tests using fake serial/NVS and scan source, fixtures,
  logs, manifests, generated documentation, release archives, and workflow
  output for credentials using clearly fake values only.
* Run existing host tests and all four PlatformIO builds. Inspect effective
  partition/NVS layout and generated artifact offsets independently for every
  environment.

## Hardware validation strategy

Run release and debug as applicable on both Heltec WiFi LoRa 32 V3
(ESP32-S3) and LilyGo T3 V1.6 (ESP32). On each family verify:

* flash, automatic reset, USB permission, serial reconnect, configured
  write/readback, and firmware-only flow;
* Wi-Fi-only and Wi-Fi-plus-MQTT setup, reset/power-cycle persistence, and
  firmware-only update persistence;
* empty-MQTT startup, broker connection attempt, portal update, BLE QR
  fallback, failed-Wi-Fi recovery, and OLED/serial statuses;
* disconnect, timeout, retry exhaustion, interrupted write, and power-loss
  behavior with confirmation that the prior valid record survives.

Record the exact desktop Chromium browser and Windows 11/Linux versions used
for AC-015 in the documentation and test evidence. Hardware checks must use
only fake/non-production credentials and must not publish them.

## Requirement traceability

| Requirement | Design decision | Files | Test |
|---|---|---|---|
| FR-001 | Add separate SSID/password fields; no import-file path. | `docs/flasher.md`, new browser flasher module | Browser form test |
| FR-002 | Validate UTF-8 byte length 1–32/0–63 before serial write; never truncate. | Browser module, CLI, firmware validator | Validation boundary tests |
| FR-003 | Empty MQTT server serializes as disabled MQTT. | Shared schema, `ConfigManager`, `MqttManager` | Wi-Fi-only/empty-MQTT tests |
| FR-004 | Validate enabled MQTT limits and uint16 port; default `iown`/1883. | Shared schema, browser, CLI, firmware | Field/default/port tests |
| FR-005 | Use explicit version, lengths, transaction metadata, and checksum in shared framing/record. | New protocol/schema files, CLI, browser | Golden vectors and malformed-record tests |
| FR-006 | Browser performs post-flash firmware protocol read/merge/write; firmware owns NVS access. | Browser module, `ConfigManager`, `main_IoHome.cpp` | Mock serial/NVS integration; AC-001 |
| FR-007 | Restrict writes to dedicated project record and preserve devices/legacy/unrelated keys. | `ConfigManager` | NVS snapshot/diff tests; AC-008 |
| FR-008 | Two-slot/staged commit with prior-generation fallback. | `ConfigManager` protocol persistence | Fault injection; AC-007 |
| FR-009 | Verify durable readback and return explicit status; clients never report false success. | Firmware protocol, browser, CLI | Verification-failure and UI/CLI tests |
| FR-010 | Automate ESP Web Tools-to-serial reset/reconnect with one defined fallback step. | Browser module, `docs/flasher.md` | Browser integration; AC-015 |
| FR-011 | Load valid project Wi-Fi, skip BLE, and enforce 3 attempts × 15 seconds + 2 seconds. | `main_IoHome.cpp`, `UIManager.cpp`, `ConfigManager` | Firmware timing/integration; AC-002 |
| FR-012 | Preserve BLE provisioning constants and QR/OLED flow when no valid record exists. | `main_IoHome.cpp`, `UIManager.cpp` | Firmware fallback; AC-005 |
| FR-013 | Bounded recovery status and documented provisioning/replacement path. | Firmware startup/UI, docs | Recovery hardware and timeout tests |
| FR-014 | Empty MQTT remains unconfigured; MQTT failures cannot reject Wi-Fi. | `MqttManager`, `ConfigManager`, startup | MQTT-disabled/unreachable tests; AC-004 |
| FR-015 | Keep legacy `iohome/mqtt` and `/mqttcfg`; document explicit precedence/sync. | `ConfigManager`, `IoHomeWebSniffer.cpp`, docs | Compatibility/portal tests; AC-009 |
| FR-016 | Python CLI shares schema/protocol, validates locally, verifies readback, retries, and stays offline. | New `scripts/configure_device.py`, shared test vectors | CLI integration; AC-010 |
| FR-017 | Build/package matrix covers four environments and inspects effective layouts. | `platformio.ini`, `scripts/build_web_flasher.py`, CI | Four build/layout checks; AC-011 |
| FR-018 | Keep no-config firmware-only manifest selection for board/profile. | Browser module, `docs/flasher.md` | Four-mode flasher tests; AC-012 |
| FR-019 | Never full-erase NVS; optional confirmed reset invalidates only project record. | `ConfigManager`, protocol, CLI/browser | NVS diff/reset tests; AC-013 |
| FR-020 | Document configured/firmware-only/recovery flows, supported combinations, and exact validation versions. | `README.md`, `docs/flasher.md` | Documentation review and AC-015 walkthrough |
| FR-021 | Keep credentials in local memory/serial operation; mask logs and scan all artifacts. | Browser, CLI, docs, build/release scripts | Secret scans; AC-014 |
| FR-022 | Centralize 5-second/3-retry serial and 3×15-second/2-second Wi-Fi constants. | Shared protocol constants, browser/CLI, firmware | Timing and interruption tests |
| AC-001 | Complete configured browser flow and gate success on verified readback. | Browser module, firmware protocol | Browser/host integration |
| AC-002 | Validate Wi-Fi-only persistence and no BLE on both physical board families. | Firmware, hardware procedure/docs | Reset/power-cycle hardware run |
| AC-003 | Persist all MQTT fields and connect/attempt without portal. | `ConfigManager`, `MqttManager`, browser | Hardware broker test |
| AC-004 | Treat empty MQTT as normal unconfigured startup. | Firmware startup/display, `MqttManager` | Firmware and hardware test |
| AC-005 | Preserve firmware-only BLE QR/device flow. | Browser bypass, firmware provisioning | Four-manifest and hardware fallback test |
| AC-006 | Reject invalid inputs/records and all transport/verification failures with no success. | Shared validator, protocol, browser, CLI | Negative/fault-injection suite |
| AC-007 | Preserve prior committed record under disconnect/timeout/power/retry faults. | Transactional NVS layer | Fault-injection and power-loss hardware test |
| AC-008 | Prove only project record changes using NVS snapshots/diffs. | NVS test harness, `ConfigManager` | Snapshot/diff suite |
| AC-009 | Keep `/mqttcfg` usable and verify documented sync/precedence. | `IoHomeWebSniffer.cpp`, config adapter, docs | Portal compatibility test |
| AC-010 | Match browser validation/write/readback/failure behavior in offline CLI. | CLI and shared schema | CLI integration/fake serial suite |
| AC-011 | Build all environments and verify board-specific effective layout/offsets. | PlatformIO config and packaging script | Matrix build/artifact inspection |
| AC-012 | Retain firmware-only operation for all four board/profile choices. | Browser flasher/docs | Browser manifest matrix test |
| AC-013 | Require confirmed project-only reset and preserve devices/unrelated data. | Protocol, CLI/browser, `ConfigManager` | Reset confirmation/NVS diff test |
| AC-014 | Ensure no credentials in repository or generated/published artifacts. | Scanning script/CI, docs/build outputs | Automated secret scan |
| AC-015 | Validate continuous Windows 11/Linux Chromium flow and unsupported-browser message. | Browser module/docs | Recorded hardware walkthrough |
| AC-016 | Preserve existing host tests and complete build/hardware coverage. | Tests, CI, firmware/build files | Host suite, four builds, hardware results |

## Implementation risks

* ESP Web Tools may close or re-enumerate the USB port differently across
  operating systems; keep reset/reconnect detection explicit and retain one
  bounded, documented reconnect step rather than claiming automatic success.
* The Arduino serial stream can contain boot logs and partial frames; framing,
  request IDs, checksum validation, and resynchronization are required.
* NVS wear and interrupted power can leave staged records; use bounded-size
  slots, generation/commit markers, and verify-before-commit.
* The existing legacy MQTT struct and new schema have different capacities and
  semantics; reject incompatible data and test truncation boundaries.
* Wi-Fi provisioning APIs differ between ESP32 and ESP32-S3/platform versions;
  keep fallback calls centralized and validate both board families.
* Effective partition/NVS offsets are generated by PlatformIO and may vary with
  package versions; inspect build output rather than encoding remembered
  offsets.
* Browser Web Serial is desktop-only and permission-gated; unsupported
  browsers must produce a clear non-success state.
* Secrets could leak through generated artifacts, serial diagnostics, test
  fixtures, or CI output; use fake values, masking, and automated scans.

## Explicit non-goals

This plan does not add OTA updates, remote credential services, raw browser
NVS-sector writing, undocumented `WiFiProv` key emulation, a general-purpose
serial terminal, mobile-browser support, new board/platform environments,
full-NVS erase during flashing, changes to radio/Home Assistant behavior, or
changes to the learned device-profile format and migration behavior.

PLAN READY FOR REVIEW
