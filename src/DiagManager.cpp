#include "DiagManager.h"
#include <esp_attr.h>
#include <esp_wifi.h>
#include <string.h>

namespace DiagManager {

namespace {

constexpr uint32_t kDiagMagic = 0x44494147; // "DIAG"

struct DiagRtcState {
    uint32_t magic;
    uint32_t bootCount;
    uint32_t provCount;
    uint8_t lastResetReason;
    uint8_t prevResetReason;
    bool hadWifiCredsAtBoot;
    bool wifiConnectedOnce;
    bool provStarted;
    bool provCredSuccess;
    bool healingTriggered;
    bool mqttConnectedOnce;
    uint8_t wifiDisconnectCount;
    uint8_t lastDisconnectReason;
    uint8_t prevDisconnectReason;
    bool pendingPublish;
};

RTC_DATA_ATTR DiagRtcState rtcState;

} // namespace

const char* resetReasonToString(uint8_t reason) {
    switch (static_cast<esp_reset_reason_t>(reason)) {
        case ESP_RST_UNKNOWN:   return "UNKNOWN";
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "OTHER";
    }
}

void init() {
    uint8_t currentReason = static_cast<uint8_t>(esp_reset_reason());

    if (rtcState.magic != kDiagMagic) {
        memset(&rtcState, 0, sizeof(rtcState));
        rtcState.magic = kDiagMagic;
        rtcState.bootCount = 1;
        rtcState.lastResetReason = currentReason;
    } else {
        rtcState.bootCount++;
        rtcState.prevResetReason = rtcState.lastResetReason;
        rtcState.lastResetReason = currentReason;
        if (rtcState.lastDisconnectReason != 0) {
            rtcState.prevDisconnectReason = rtcState.lastDisconnectReason;
        }
        // Reset per-boot transient state; preserve healingTriggered, prevDisconnectReason,
        // prevResetReason, bootCount, and provCount across the reset.
        rtcState.hadWifiCredsAtBoot = false;
        rtcState.wifiConnectedOnce = false;
        rtcState.provStarted = false;
        rtcState.provCredSuccess = false;
        rtcState.mqttConnectedOnce = false;
        rtcState.wifiDisconnectCount = 0;
        rtcState.lastDisconnectReason = 0;
    }

    // One-shot publish on the next MQTT connection after boot
    rtcState.pendingPublish = true;

    Serial.printf("[DIAG] Boot #%lu | Reset: %s (%u) | Prev: %s (%u) | HealingRestart: %s\n",
                  static_cast<unsigned long>(rtcState.bootCount),
                  resetReasonToString(rtcState.lastResetReason),
                  rtcState.lastResetReason,
                  resetReasonToString(rtcState.prevResetReason),
                  rtcState.prevResetReason,
                  rtcState.healingTriggered ? "true" : "false");
}

void recordWifiCredsPresent() {
    wifi_config_t wifiCfg;
    memset(&wifiCfg, 0, sizeof(wifiCfg));
    bool hasCreds = (esp_wifi_get_config(WIFI_IF_STA, &wifiCfg) == ESP_OK) &&
                    (strnlen(reinterpret_cast<const char*>(wifiCfg.sta.ssid), sizeof(wifiCfg.sta.ssid)) > 0);
    rtcState.hadWifiCredsAtBoot = hasCreds;
    Serial.printf("[DIAG] Stored Wi-Fi credentials at boot: %s\n", hasCreds ? "true" : "false");
}

void onWifiGotIp() {
    rtcState.wifiConnectedOnce = true;
}

void onWifiDisconnected(uint8_t reason) {
    if (rtcState.wifiDisconnectCount < 255) {
        rtcState.wifiDisconnectCount++;
    }
    rtcState.lastDisconnectReason = reason;
}

void onProvStarted() {
    rtcState.provStarted = true;
    rtcState.provCount++;
    rtcState.pendingPublish = true;
}

void onProvCredSuccess() {
    rtcState.provCredSuccess = true;
    rtcState.pendingPublish = true;
}

void onHealingRestart() {
    rtcState.healingTriggered = true;
    if (rtcState.lastDisconnectReason != 0) {
        rtcState.prevDisconnectReason = rtcState.lastDisconnectReason;
    }
}

void onMqttConnected() {
    rtcState.mqttConnectedOnce = true;
}

bool hasPendingPublish() {
    return rtcState.pendingPublish;
}

void markPublished() {
    rtcState.pendingPublish = false;
    // Clear one-shot incident flags once reported so future unrelated boots/reconnects stay accurate
    rtcState.healingTriggered = false;
    rtcState.prevDisconnectReason = 0;
}

String buildJsonPayload() {
    char buf[384];
    snprintf(buf, sizeof(buf),
             "{\"boot\":%lu,\"reset_reason\":\"%s\",\"reset_code\":%u,"
             "\"prev_reset_reason\":\"%s\",\"prev_reset_code\":%u,"
             "\"had_wifi_creds\":%s,\"wifi_connected\":%s,"
             "\"wifi_disconnects\":%u,\"last_disc_reason\":%u,\"prev_disc_reason\":%u,"
             "\"healing_restart\":%s,\"prov_started\":%s,\"prov_success\":%s,"
             "\"prov_count\":%lu,\"mqtt_connected\":%s,\"uptime_ms\":%lu}",
             static_cast<unsigned long>(rtcState.bootCount),
             resetReasonToString(rtcState.lastResetReason),
             rtcState.lastResetReason,
             resetReasonToString(rtcState.prevResetReason),
             rtcState.prevResetReason,
             rtcState.hadWifiCredsAtBoot ? "true" : "false",
             rtcState.wifiConnectedOnce ? "true" : "false",
             rtcState.wifiDisconnectCount,
             rtcState.lastDisconnectReason,
             rtcState.prevDisconnectReason,
             rtcState.healingTriggered ? "true" : "false",
             rtcState.provStarted ? "true" : "false",
             rtcState.provCredSuccess ? "true" : "false",
             static_cast<unsigned long>(rtcState.provCount),
             rtcState.mqttConnectedOnce ? "true" : "false",
             static_cast<unsigned long>(millis()));
    return String(buf);
}

} // namespace DiagManager
