#pragma once

#include <ctime>
#include <cstdint>

/**
 * @brief Helper class for ESP32 NTP time synchronization
 * 
 * WireGuard requires accurate system time for cryptographic operations.
 * This helper provides methods to synchronize time from NTP servers
 * with timeout and retry logic suitable for embedded systems.
 * 
 * Usage:
 *   ESP32TimeSync::syncTime("pool.ntp.org", 15000);  // Sync with 15s timeout
 *   if (ESP32TimeSync::isTimeSynced()) {
 *     // Time is now valid, can proceed with WireGuard
 *   }
 */
class ESP32TimeSync {
public:
  /**
   * @brief Synchronize system time from NTP server
   * 
   * This function blocks until time is synced or timeout is reached.
   * It calls configTime() internally and polls the system clock.
   * 
   * @param ntp_server NTP server hostname or IP (e.g., "pool.ntp.org")
   * @param timeout_ms Maximum time to wait for sync in milliseconds
   * @return true if time was successfully synced, false on timeout
   */
  static bool syncTime(const char* ntp_server, uint32_t timeout_ms = 15000);
  
  /**
   * @brief Synchronize system time from multiple NTP servers with fallback
   * 
   * Tries each server in sequence. Useful for improving reliability.
   * 
   * @param ntp_servers Array of NTP server hostnames
   * @param num_servers Number of servers in array
   * @param timeout_ms_per_server Timeout for each server attempt
   * @return true if any server succeeded, false if all failed
   */
  static bool syncTimeMulti(const char* const* ntp_servers, 
                           int num_servers,
                           uint32_t timeout_ms_per_server = 15000);
  
  /**
   * @brief Get current Unix timestamp
   * 
   * @return Number of seconds since epoch (1970-01-01 00:00:00 UTC)
   */
  static time_t getUnixTime();
  
  /**
   * @brief Check if system time has been synchronized
   * 
   * Returns true if localtime() can return valid date/time.
   * Note: This is a heuristic (checks if year > 2020).
   * 
   * @return true if time appears to be valid, false if not synced
   */
  static bool isTimeSynced();
  
  /**
   * @brief Get human-readable date/time string
   * 
   * Format: "YYY-MM-DD HH:MM:SS UTC"
   * Useful for logging and debugging.
   * 
   * @param buffer Buffer to store output string
   * @param buffer_size Size of output buffer (should be at least 26 bytes)
   * @return Pointer to buffer (same as input)
   */
  static char* getTimeString(char* buffer, size_t buffer_size);
  
private:
  // Minimum reasonable year (WireGuard won't work with time before ~2020)
  static constexpr int MIN_VALID_YEAR = 2020;
  
  // Tuning parameters
  static constexpr uint32_t POLL_INTERVAL_MS = 100;  // Check time every 100ms
};
