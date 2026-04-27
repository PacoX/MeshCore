#include "WireGuardManager.h"
#include "ESP32TimeSync.h"
#include <Arduino.h>
#include <cstring>

#ifdef WIREGUARD_ENABLED

// Constructor
WireGuardManager::WireGuardManager()
  : _state(STATE_IDLE),
    _previous_state(STATE_IDLE),
    _state_enter_time(0),
    _config_valid(false),
    _wifi_connected(false),
    _wifi_connect_start(0),
    _time_synced(false),
    _ntp_start(0) {
}

/**
 * @brief Initialize the WireGuard connection sequence
 */
bool WireGuardManager::begin(const WireGuardConfig& config) {
  // Validate configuration
  if (!config.isValid()) {
    _state = STATE_FAILED;
    return false;
  }
  
  // Store config
  _config = config;
  _config_valid = true;
  
  // Start WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(_config.wifi_ssid, _config.wifi_password);
  
  _wifi_connect_start = millis();
  _transitionState(STATE_WIFI_CONNECTING);
  
  return true;
}

/**
 * @brief Shutdown WireGuard connection
 */
void WireGuardManager::end() {
  if (_state == STATE_CONNECTED || _state == STATE_WG_INIT) {
    _wg.end();
  }
  
  WiFi.disconnect(true);  // true = turn off WiFi radio
  _transitionState(STATE_IDLE);
}

/**
 * @brief Main state machine loop
 */
WireGuardManager::State WireGuardManager::loop() {
  // Nothing to do if not initialized or failed
  if (_state == STATE_IDLE || _state == STATE_FAILED) {
    return _state;
  }
  
  // Check for state timeout
  _handleStateTimeout();
  
  // State machine transitions
  switch (_state) {
    case STATE_WIFI_CONNECTING:
      if (_checkWiFiConnected()) {
        _transitionState(STATE_WIFI_CONNECTED);
      }
      break;
    
    case STATE_WIFI_CONNECTED:
      // WiFi is connected, start NTP sync
      ESP32TimeSync::syncTime(_config.ntp_server, _config.ntp_timeout_ms);
      _ntp_start = millis();
      _transitionState(STATE_NTP_SYNCING);
      break;
    
    case STATE_NTP_SYNCING:
      if (_checkTimesynced()) {
        _transitionState(STATE_WG_INIT);
      }
      break;
    
    case STATE_WG_INIT:
      if (_initWireGuard()) {
        _transitionState(STATE_CONNECTED);
      }
      break;
    
    case STATE_CONNECTED:
      // Check if WiFi still connected (watchdog)
      if (!_checkWiFiConnected()) {
        // WiFi dropped, need to reconnect
        _transitionState(STATE_WIFI_CONNECTING);
        WiFi.begin(_config.wifi_ssid, _config.wifi_password);
      }
      break;
    
    default:
      break;
  }
  
  return _state;
}

/**
 * @brief Check if WiFi is connected
 */
bool WireGuardManager::_checkWiFiConnected() {
  wl_status_t status = WiFi.status();
  bool connected = (status == WL_CONNECTED);
  
  _wifi_connected = connected;
  return connected;
}

/**
 * @brief Check if NTP time has been synced
 */
bool WireGuardManager::_checkTimesynced() {
  _time_synced = ESP32TimeSync::isTimeSynced();
  return _time_synced;
}

/**
 * @brief Initialize WireGuard tunnel
 */
bool WireGuardManager::_initWireGuard() {
  IPAddress local_ip;
  if (!local_ip.fromString(_config.local_ip)) {
    return false;
  }

  // Note: begin() can block briefly while setting up the interface.
  return _wg.begin(
    local_ip,
    _config.private_key,
    _config.endpoint_address,
    _config.peer_public_key,
    _config.endpoint_port
  );
}

/**
 * @brief Handle timeout for current state
 * 
 * If state takes too long, transition to FAILED.
 */
void WireGuardManager::_handleStateTimeout() {
  unsigned long elapsed = getStateTime();
  unsigned long timeout = 0;
  
  switch (_state) {
    case STATE_WIFI_CONNECTING:
      timeout = WIFI_TIMEOUT_MS;
      break;
    
    case STATE_NTP_SYNCING:
      timeout = NTP_TIMEOUT_MS;
      break;
    
    case STATE_WG_INIT:
      timeout = WG_TIMEOUT_MS;
      break;
    
    default:
      return;
  }
  
  if (timeout > 0 && elapsed > timeout) {
    _transitionState(STATE_FAILED);
  }
}

/**
 * @brief Transition to a new state
 */
void WireGuardManager::_transitionState(State new_state) {
  if (new_state != _state) {
    _previous_state = _state;
    _state = new_state;
    _state_enter_time = millis();
  }
}

/**
 * @brief Check if VPN is connected
 */
bool WireGuardManager::isConnected() const {
  return _state == STATE_CONNECTED;
}

/**
 * @brief Get human-readable state name
 */
const char* WireGuardManager::getStateName(State state) const {
  if (state == STATE_MAX) {
    state = _state;
  }
  
  switch (state) {
    case STATE_IDLE:             return "IDLE";
    case STATE_WIFI_CONNECTING:  return "WIFI_CONNECTING";
    case STATE_WIFI_CONNECTED:   return "WIFI_CONNECTED";
    case STATE_NTP_SYNCING:      return "NTP_SYNCING";
    case STATE_WG_INIT:          return "WG_INIT";
    case STATE_CONNECTED:        return "CONNECTED";
    case STATE_FAILED:           return "FAILED";
    default:                     return "UNKNOWN";
  }
}

/**
 * @brief Get local VPN IP address
 */
const char* WireGuardManager::getLocalIP() const {
  if (_state == STATE_CONNECTED) {
    return _config.local_ip;
  }
  return nullptr;
}

/**
 * @brief Get human-readable status string
 */
char* WireGuardManager::getStatus(char* buffer, size_t buffer_size) const {
  if (buffer == nullptr || buffer_size < 20) {
    return buffer;
  }
  
  const char* state_name = getStateName();
  const char* ip = (_state == STATE_CONNECTED) ? _config.local_ip : "N/A";
  
  snprintf(buffer, buffer_size,
           "State: %s | WiFi: %s | NTP: %s | IP: %s",
           state_name,
           _wifi_connected ? "yes" : "no",
           _time_synced ? "yes" : "no",
           ip);
  
  return buffer;
}

/**
 * @brief Get time spent in current state
 */
unsigned long WireGuardManager::getStateTime() const {
  return millis() - _state_enter_time;
}

/**
 * @brief Force reconnection from FAILED state
 */
void WireGuardManager::reconnect() {
  if (_state == STATE_FAILED) {
    WiFi.begin(_config.wifi_ssid, _config.wifi_password);
    _wifi_connect_start = millis();
    _transitionState(STATE_WIFI_CONNECTING);
  }
}

#endif
