# Tasks: Web Firmware Flasher

## Task 01-tracer-bullet-heltec-flasher

Implement an end-to-end tracer bullet for browser-based flashing targeting the Heltec WiFi LoRa 32 V3 board. This slice builds the core manifest compilation script for a single hardware target and integrates the ESP Web Tools button into the MkDocs documentation site.

### Implementation steps

- [x] Create a build automation script (`scripts/build_web_flasher.py`) that runs PlatformIO compilation for `heltec_wifi_lora_32_V3` and packages binaries (`bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin`) into `docs/firmware/heltec_wifi_lora_32_V3/`.
- [x] Generate an ESP Web Tools compliant `manifest.json` with `ESP32-S3` chip family and correct memory offsets (`0x0000` bootloader, `0x8000` partitions, `0xe000` boot_app0, `0x10000` firmware).
- [x] Create `docs/flasher.md` embedding the `<esp-web-install-button>` web component script (`https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module`).
- [x] Update `mkdocs.yml` to include `Web Flasher: flasher.md` under `nav` and adjust `exclude_docs` so compiled firmware manifests/binaries are published with the site.

### Acceptance criteria

- [x] `scripts/build_web_flasher.py` compiles `heltec_wifi_lora_32_V3` and outputs non-empty `.bin` files and a valid `manifest.json`.
- [x] The generated `manifest.json` specifies chip family `ESP32-S3` and offset mappings `0x0000`, `0x8000`, `0xe000`, `0x10000`.
- [x] `docs/flasher.md` exists, contains the ESP Web Tools install button, and links to the generated manifest.
- [x] `mkdocs.yml` includes "Web Flasher" in `nav` and builds without stripping `docs/firmware/` assets.
- [x] Local `mkdocs build` generates the complete site with the flasher page accessible at `/flasher/`.

### Quality gates

- [x] `python3 -m py_compile scripts/build_web_flasher.py` passes with zero syntax errors.
- [x] `mkdocs build` finishes with exit code 0 and no broken link warnings.

---

## Task 02-multi-board-matrix-and-dynamic-ui

Extend the build script to compile the complete multi-board matrix across all `platformio.ini` environments and build an interactive board/variant selector UI in the Web Flasher page.

### Implementation steps

- [ ] Update `scripts/build_web_flasher.py` to compile all 4 project environments: `heltec_wifi_lora_32_V3`, `heltec_wifi_lora_32_V3_debug`, `lilygo_t3_v16`, and `lilygo_t3_v16_debug`.
- [ ] Support chip family detection and memory offset differentiation between `ESP32-S3` (bootloader at `0x0000`) and `ESP32` (LilyGo T3, bootloader at `0x1000`).
- [ ] Enhance `docs/flasher.md` with responsive HTML/JS selector controls for Target Hardware (Heltec V3 vs. LilyGo T3) and Build Profile (Release vs. Debug).
- [ ] Implement client-side JavaScript in `docs/flasher.md` that dynamically updates the `manifest` attribute on the `<esp-web-install-button>` upon dropdown selection.
- [ ] Add Web Serial compatibility check (`'serial' in navigator`) that displays a prominent warning banner on unsupported browsers (Firefox, Safari, iOS).

### Acceptance criteria

- [ ] Build script generates 4 separate manifest files and binary directories corresponding to each hardware environment.
- [ ] LilyGo T3 (`ESP32`) manifest specifies chip family `ESP32` and bootloader offset `0x1000`.
- [ ] Heltec V3 (`ESP32-S3`) manifest specifies chip family `ESP32-S3` and bootloader offset `0x0000`.
- [ ] `docs/flasher.md` UI allows toggling between Heltec V3 and LilyGo T3, and between Release and Debug variants.
- [ ] Toggling selection updates the `<esp-web-install-button manifest="...">` attribute to load the corresponding manifest URL.
- [ ] Visiting the page on a browser lacking `navigator.serial` renders an error banner pointing users to Google Chrome or Microsoft Edge.

### Quality gates

- [ ] All 4 generated `manifest.json` files pass JSON syntax and ESP Web Tools schema structure validation.
- [ ] `mkdocs build` succeeds with zero errors.

---

## Task 03-release-workflow-and-pages-deployment

Automate the firmware build, release asset packaging, and GitHub Pages deployment in a tag-triggered GitHub Actions CI/CD workflow.

### Implementation steps

- [ ] Create `.github/workflows/release-firmware.yml` configured to trigger on git tag pushes (`refs/tags/v*`).
- [ ] Configure CI environment to install Python, PlatformIO, and repository dependencies.
- [ ] Run `scripts/build_web_flasher.py` during the workflow to produce all target binaries and `manifest.json` files.
- [ ] Use `softprops/action-gh-release` (or `gh` CLI) to attach compiled firmware zip archives to the GitHub Release.
- [ ] Integrate MkDocs deployment (`mkdocs gh-deploy --force`) into the workflow so the flasher web page, manifests, and binaries are published together to GitHub Pages.

### Acceptance criteria

- [ ] `.github/workflows/release-firmware.yml` triggers exclusively on release tags matching `v*`.
- [ ] The workflow compiles all 4 board environments and builds the documentation site.
- [ ] Compiled binaries are attached as downloadable assets to the GitHub Release.
- [ ] The generated site on GitHub Pages serves `docs/flasher.md` along with all firmware binaries and manifest JSON files.
- [ ] Standard commits to `main` without tags do NOT trigger the release firmware workflow.

### Quality gates

- [ ] Workflow YAML file passes structural validation against GitHub Actions JSON schema.
- [ ] Workflow permissions strictly adhere to principle of least privilege (`contents: write`, `pages: write`, `id-token: write`).
