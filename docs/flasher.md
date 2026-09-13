# Web Firmware Flasher

Flash the situo5-esp32 firmware directly to your microcontroller from your web browser using Web Serial.

<div id="unsupported-browser-banner" style="display: none; background-color: #ffebe9; border: 1px solid #ff8888; color: #cf222e; padding: 16px; border-radius: 6px; margin-bottom: 20px;">
  <strong>⚠️ Web Serial API Not Supported:</strong> Your current browser does not support Web Serial flashing. Please open this page in <strong>Google Chrome</strong>, <strong>Microsoft Edge</strong>, <strong>Brave</strong>, or <strong>Opera</strong> on desktop to flash your device.
</div>

<div id="flasher-container" style="background: var(--md-code-bg-color, #f8f9fa); border: 1px solid var(--md-default-foreground-color--divider, #e0e0e0); border-radius: 8px; padding: 24px; margin-bottom: 24px;">
  <div style="display: flex; flex-wrap: wrap; gap: 20px; margin-bottom: 24px;">
    <div style="flex: 1; min-width: 240px;">
      <label for="board-select" style="display: block; font-weight: 600; margin-bottom: 8px;">Target Hardware</label>
      <select id="board-select" style="width: 100%; padding: 10px; border-radius: 6px; border: 1px solid #ccc; font-size: 14px; background: white; color: #333;">
        <option value="heltec_wifi_lora_32_V3">Heltec WiFi LoRa 32 V3 (ESP32-S3)</option>
        <option value="lilygo_t3_v16">LilyGo T3 V1.6 (ESP32)</option>
      </select>
    </div>
    
    <div style="flex: 1; min-width: 240px;">
      <label for="profile-select" style="display: block; font-weight: 600; margin-bottom: 8px;">Build Profile</label>
      <select id="profile-select" style="width: 100%; padding: 10px; border-radius: 6px; border: 1px solid #ccc; font-size: 14px; background: white; color: #333;">
        <option value="release">Release (Standard Firmware)</option>
        <option value="debug">Debug (Verbose Serial Logging)</option>
      </select>
    </div>
  </div>

  <div style="display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 16px; padding-top: 16px; border-top: 1px solid var(--md-default-foreground-color--divider, #e0e0e0);">
    <div>
      <span style="font-size: 13px; color: #666;">Selected Manifest:</span>
      <code id="manifest-path-display" style="display: block; font-size: 12px; margin-top: 4px; padding: 4px 8px; background: #eee; border-radius: 4px; color: #333;">../firmware/heltec_wifi_lora_32_V3/manifest.json</code>
    </div>

    <div>
      <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
      <esp-web-install-button id="flasher-install-button" manifest="../firmware/heltec_wifi_lora_32_V3/manifest.json"></esp-web-install-button>
    </div>
  </div>
</div>

<script>
  document.addEventListener("DOMContentLoaded", function () {
    var boardSelect = document.getElementById("board-select");
    var profileSelect = document.getElementById("profile-select");
    var installButton = document.getElementById("flasher-install-button");
    var manifestDisplay = document.getElementById("manifest-path-display");
    var warningBanner = document.getElementById("unsupported-browser-banner");

    if (!('serial' in navigator)) {
      if (warningBanner) {
        warningBanner.style.display = "block";
      }
    }

    function updateManifest() {
      var board = boardSelect.value;
      var profile = profileSelect.value;
      
      var env = board;
      if (profile === "debug") {
        env += "_debug";
      }

      var manifestUrl = "../firmware/" + env + "/manifest.json";
      installButton.setAttribute("manifest", manifestUrl);
      if (manifestDisplay) {
        manifestDisplay.textContent = manifestUrl;
      }
    }

    if (boardSelect && profileSelect) {
      boardSelect.addEventListener("change", updateManifest);
      profileSelect.addEventListener("change", updateManifest);
    }
  });
</script>

## Flashing Instructions

1. Connect your board to your computer using a high-quality USB data cable.
2. Select your hardware target and build profile using the dropdown menus above.
3. Click the **Install** button.
4. In the browser popup window, select the USB serial port corresponding to your device and click **Connect**.
5. Follow the on-screen prompts to complete installation.
