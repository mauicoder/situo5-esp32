# Product Requirements Document (PRD) - Web Firmware Flasher

## Problem Statement

Users of the iown-homecontrol project who purchase microcontrollers (such as the Heltec WiFi LoRa 32 V3 or LilyGO T3 V1.6) often face friction when attempting to flash the firmware onto their physical boards. Currently, flashing requires cloning the git repository, setting up Python and PlatformIO locally, installing appropriate USB drivers, and running command-line build/upload tools. This technical barrier prevents non-developer users or quick evaluators from easily setting up and updating their iown-homecontrol hardware.

## Solution

Provide a web-based firmware flasher directly hosted on GitHub Pages alongside the project documentation. Utilizing ESP Web Tools and the browser's Web Serial API, users will be able to plug their microcontrollers into their computer via USB and flash the latest released firmware binaries with a single click in Chrome, Edge, or Opera. The release process will be fully automated through GitHub Actions, triggering on release tags to compile all target environment binaries, generate `manifest.json` metadata, and deploy them alongside the MkDocs documentation site.

## User Stories

1. As an end user with a Heltec WiFi LoRa 32 V3 board, I want to flash the latest release firmware directly from my web browser without installing PlatformIO or Python locally.
2. As an end user with a LilyGO T3 V1.6 board, I want to select my specific hardware model from a dropdown menu so that the flasher installs the exact binary designed for my hardware.
3. As a developer troubleshooting issues, I want to select the Debug build variant from the flasher interface so that I can flash a build with extra log outputs over serial.
4. As a new user visiting the documentation, I want to see clear browser compatibility requirements and connection instructions on the flasher page so that I know how to prepare my board before flashing.
5. As a maintainer releasing a new software version via git tags, I want GitHub Actions to automatically compile all hardware binaries and generate the Web Serial manifest files without manual intervention.
6. As a maintainer, I want main branch commits to skip automatic firmware binary generation so that release artifacts remain stable and tied strictly to official releases.
7. As a web page visitor on an unsupported browser (such as Safari or Firefox), I want to see an informative banner explaining Web Serial requirements and suggesting a compatible browser.

## Implementation Decisions

1. **Flashing Interface & Integration**:
   - Integrate ESP Web Tools (`esp-web-tools` web components) into a dedicated page within the MkDocs documentation site (`docs/flasher.md`).
   - Add a "Web Flasher" entry to `mkdocs.yml` navigation.
   - Include interactive UI controls (dropdowns/cards) for selecting target hardware (`heltec_wifi_lora_32_V3` vs `lilygo_t3_v16`) and build profile (`Release` vs `Debug`).
   - Dynamically update the `<esp-web-install-button>` element's manifest attribute upon selector change.

2. **Automated CI/CD Release Workflow**:
   - Create a dedicated GitHub Actions workflow triggered exclusively on tag creation (`refs/tags/v*`).
   - Execute `platformio run` for all target environments specified in `platformio.ini`.
   - Use a post-compile script/step to package offset binaries (`bootloader.bin`, `partitions.bin`, `firmware.bin`, `boot_app0.bin`) or generate merged factory binaries.
   - Generate `manifest.json` metadata referencing the tag's compiled artifacts.
   - Publish compiled firmware binaries and manifest files to GitHub Pages / GitHub Release assets.

3. **Modules & Contracts**:
   - **`release-firmware` CI Workflow**: Deep module encapsulating multi-environment compilation, artifact collection, and manifest rendering.
   - **`Flasher UI` Script Component**: Lightweight DOM controller embedded in `docs/flasher.md` managing selection state and updating the `<esp-web-install-button>` manifest pointer.

## Testing Decisions

1. **CI Pipeline Testing**:
   - Validate GitHub Actions workflow execution on tag pushes.
   - Ensure all `platformio.ini` environments build without compilation errors.
   - Verify that output `.bin` files and `manifest.json` exist and are formatted according to the ESP Web Tools schema.

2. **Schema & Web Interface Testing**:
   - Validate that `manifest.json` contains correct chip family tags (`ESP32`, `ESP32-S3`), memory offsets, and valid binary URL paths.
   - Verify dynamic manifest switching in the browser console upon selecting different dropdown options.

## Out of Scope

- Over-The-Air (OTA) wireless flashing over Wi-Fi.
- Live serial monitoring terminal replacement inside the web browser (serial monitor will rely on ESP Web Tools basic logs if enabled).
- Building firmware binaries automatically on un-tagged commits in the `main` branch.

## Further Notes

- Web Serial API requires HTTPS hosting (which GitHub Pages provides by default) and modern Chromium-based browsers (Chrome 89+, Edge 89+, Opera 75+, Brave).
