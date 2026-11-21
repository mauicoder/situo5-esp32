#pragma once

#include <Arduino.h>
#include <esp_system.h>

namespace DiagManager {

/**
 * @brief Initializes diagnostic state in RTC memory and captures esp_reset_reason().
 * Should be called once early in setup().
 */
void init();

/**
 * @brief Checks and records whether stored Wi-Fi STA credentials exist in NVS
 * before provisioning is started.
 */
void recordWifiCredsPresent();

/**
 * @brief Records that Wi-Fi STA obtained an IP address.
 */
void onWifiGotIp();

/**
 * @brief Records a Wi-Fi STA disconnection event and its reason code.
 */
void onWifiDisconnected(uint8_t reason);

/**
 * @brief Records that BLE Wi-Fi provisioning has started.
 */
void onProvStarted();

/**
 * @brief Records that BLE Wi-Fi provisioning credentials were received successfully.
 */
void onProvCredSuccess();

/**
 * @brief Records that handleWifiHealing() is wiping credentials and restarting.
 */
void onHealingRestart();

/**
 * @brief Records that the MQTT client connected.
 */
void onMqttConnected();

/**
 * @brief Returns true if a one-shot diagnostic payload is pending publication.
 */
bool hasPendingPublish();

/**
 * @brief Clears the one-shot pending flag and transient incident markers after publishing.
 */
void markPublished();

/**
 * @brief Builds the JSON diagnostic payload string for MQTT publication.
 */
String buildJsonPayload();

/**
 * @brief Maps an esp_reset_reason_t value to a short human-readable string.
 */
const char* resetReasonToString(uint8_t reason);

} // namespace DiagManager
