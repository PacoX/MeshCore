#pragma once

#include <cstdint>

/**
 * @brief WireGuard VPN Configuration Structure
 * 
 * Contains all configuration parameters needed to establish a WireGuard
 * VPN tunnel on ESP32. The key parameters must be base64-encoded.
 * 
 * To generate WireGuard keys:
 *   wg genkey | tee privatekey | wg pubkey > publickey
 * 
 * Then base64 encode them for use in this config.
 */
struct WireGuardConfig {
  // WiFi Configuration (required)
  const char* wifi_ssid;                /**< WiFi network SSID */
  const char* wifi_password;            /**< WiFi network password */
  
  // WireGuard Tunnel Configuration (required)
  const char* local_ip;                 /**< Local VPN IP address (e.g., "10.0.0.2") */
  const char* private_key;              /**< Private key (base64 encoded) */
  const char* peer_public_key;          /**< Peer public key (base64 encoded) */
  const char* endpoint_address;         /**< VPN endpoint hostname or IP */
  uint16_t endpoint_port;               /**< VPN endpoint port (typically 51820) */
  
  // NTP Configuration (optional, has defaults)
  const char* ntp_server;               /**< NTP server hostname */
  uint32_t ntp_timeout_ms;              /**< NTP sync timeout in milliseconds */
  
  /**
   * @brief Default constructor for WireGuardConfig
   */
  WireGuardConfig()
    : wifi_ssid(nullptr),
      wifi_password(nullptr),
      local_ip(nullptr),
      private_key(nullptr),
      peer_public_key(nullptr),
      endpoint_address(nullptr),
      endpoint_port(51820),
      ntp_server("pool.ntp.org"),
      ntp_timeout_ms(15000) {}
  
  /**
   * @brief Validate that all required fields are set
   * @return true if configuration is valid, false otherwise
   */
  bool isValid() const {
    return wifi_ssid != nullptr &&
           wifi_password != nullptr &&
           local_ip != nullptr &&
           private_key != nullptr &&
           peer_public_key != nullptr &&
           endpoint_address != nullptr &&
           endpoint_port > 0;
  }
};
