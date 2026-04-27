#pragma once

#include "WireGuardConfig.h"
#include <WiFi.h>

#ifdef WIREGUARD_ENABLED
  #include <WireGuard-ESP32.h>
#endif

/**
 * @brief WireGuard VPN Manager State Machine
 * 
 * Manages the complete lifecycle of establishing a WireGuard VPN connection:
 * 1. IDLE → WiFi connection attempt
 * 2. WiFi connected → NTP time sync
 * 3. Time synced → WireGuard tunnel initialization
 * 4. Tunnel established → CONNECTED
 * 
 * This class provides a single, high-level abstraction for the VPN tunnel.
 * Applications simply call begin() and then loop() periodically.
 * 
 * Non-blocking design: Uses polling in loop(), no async/await.
 * 
 * Usage:
 *   WireGuardManager wg;
 *   WireGuardConfig cfg = { ... };
 *   wg.begin(cfg);
 *   
 *   while (true) {
 *     wg.loop();
 *     if (wg.isConnected()) {
 *       // VPN ready, can make TCP connections
 *     }
 *   }
 */
class WireGuardManager {
public:
  /**
   * @brief State enumeration for the connection FSM
   */
  enum State {
    STATE_IDLE = 0,              ///< Not initialized
    STATE_WIFI_CONNECTING,       ///< WiFi.begin() called, waiting for connection
    STATE_WIFI_CONNECTED,        ///< WiFi connected, starting NTP sync
    STATE_NTP_SYNCING,           ///< NTP time sync in progress
    STATE_WG_INIT,               ///< NTP done, attempting WireGuard init
    STATE_CONNECTED,             ///< VPN tunnel established!
    STATE_FAILED,                ///< Connection failed (terminal state)
    STATE_MAX
  };
  
  /**
   * @brief Default constructor
   */
  WireGuardManager();
  
  /**
   * @brief Initialize WireGuard connection
   * 
   * Starts the state machine. Must be called once before loop().
   * Does NOT block - returns immediately. Actual connection happens in loop().
   * 
   * @param config WireGuard configuration (must be valid, see isValid())
   * @return true if config accepted and state machine started,
   *         false if config invalid or already initialized
   */
  bool begin(const WireGuardConfig& config);
  
  /**
   * @brief Shutdown WireGuard connection
   * 
   * Cleanly closes the VPN tunnel and reverts to IDLE state.
   * Can be called multiple times safely.
   */
  void end();
  
  /**
   * @brief Periodic update - must be called in main loop
   * 
   * Advances the state machine based on current conditions:
   * - Checks WiFi connection status
   * - Polls NTP time sync progress
   * - Initializes WireGuard when ready
   * - Handles reconnection on failure
   * 
   * Should be called every loop iteration (~10-100ms).
   * Non-blocking and tolerates frequent calls.
   * 
   * @return Current state after update
   */
  State loop();
  
  /**
   * @brief Check if VPN tunnel is fully connected and ready
   * 
   * @return true if status is STATE_CONNECTED, false otherwise
   */
  bool isConnected() const;
  
  /**
   * @brief Get current state of the state machine
   * 
   * Useful for debugging and status display.
   * 
   * @return Current state (see enum State)
   */
  State getState() const { return _state; }
  
  /**
   * @brief Get human-readable state name
   * 
   * @param state State to convert (defaults to current state)
   * @return Static string like "CONNECTED", "WIFI_CONNECTING", etc
   */
  const char* getStateName(State state = STATE_MAX) const;
  
  /**
   * @brief Get the local VPN IP address
   * 
   * Only valid when isConnected() returns true.
   * Example: "10.0.0.2"
   * 
   * @return Local VPN IP address, or nullptr if not connected
   */
  const char* getLocalIP() const;
  
  /**
   * @brief Get human-readable status string for logging/display
   * 
   * Example: "WiFi: Connected | NTP: Synced | VPN: Active | IP: 10.0.0.2"
   * 
   * @param buffer Output buffer (recommend at least 80 bytes)
   * @param buffer_size Size of buffer
   * @return Pointer to buffer
   */
  char* getStatus(char* buffer, size_t buffer_size) const;
  
  /**
   * @brief Get time (in seconds) spent in current state
   * 
   * Useful for detecting stuck states or debugging.
   * 
   * @return Seconds elapsed in current state
   */
  unsigned long getStateTime() const;
  
  /**
   * @brief Force reconnection when state is FAILED
   * 
   * Only works when state is FAILED. Transitions back to WIFI_CONNECTING.
   * Automatically called in some timeout scenarios.
   */
  void reconnect();

private:
  // State machine variables
  State _state;                          ///< Current FSM state
  State _previous_state;                 ///< Previous state (for change detection)
  unsigned long _state_enter_time;       ///< millis() when entered current state
  
  // Configuration
  WireGuardConfig _config;               ///< Stored configuration
  bool _config_valid;                    ///< Cached validation result
  
  // WiFi tracking
  bool _wifi_connected;                  ///< Last known WiFi connection status
  unsigned long _wifi_connect_start;     ///< When WiFi connection started
  
  // NTP tracking
  bool _time_synced;                     ///< Last known NTP sync status
  unsigned long _ntp_start;              ///< When NTP sync started
  
  // WireGuard instance
#ifdef WIREGUARD_ENABLED
  WireGuard _wg;                         ///< Underlying WireGuard object
#endif
  
  // Timeout constants
  static constexpr unsigned long WIFI_TIMEOUT_MS = 30000;    ///< Max time for WiFi connection
  static constexpr unsigned long NTP_TIMEOUT_MS = 15000;     ///< Max time for NTP sync
  static constexpr unsigned long WG_TIMEOUT_MS = 10000;      ///< Max time for WireGuard init
  static constexpr unsigned long STATE_LOOP_INTERVAL_MS = 100; ///< Min time between state checks
  
  // Helper methods (private)
  void _transitionState(State new_state);
  bool _checkWiFiConnected();
  bool _checkTimesynced();
  bool _initWireGuard();
  void _handleStateTimeout();
};
