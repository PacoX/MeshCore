#include "ESP32TimeSync.h"
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

/**
 * @brief Synchronize system time from NTP server
 * 
 * Blocks until time is synced or timeout is reached.
 * This implementation:
 * - Calls configTime() to start NTP sync
 * - Polls localtime() until it returns valid date/time
 * - Respects timeout to prevent infinite blocking
 * 
 * @param ntp_server NTP server hostname
 * @param timeout_ms Maximum time to wait
 * @return true if synced, false on timeout
 */
bool ESP32TimeSync::syncTime(const char* ntp_server, uint32_t timeout_ms) {
  if (ntp_server == nullptr) {
    return false;
  }
  
  // Start NTP time sync
  // Parameters: timezone offset (UTC), daylight offset, server names
  configTime(0, 0, ntp_server);
  
  // Wait for time to be set
  unsigned long start_ms = millis();
  
  while (!isTimeSynced()) {
    // Check timeout
    unsigned long elapsed = millis() - start_ms;
    if (elapsed > timeout_ms) {
      return false;
    }
    
    // Small delay to avoid busy-wait
    delay(POLL_INTERVAL_MS);
  }
  
  return true;
}

/**
 * @brief Synchronize system time from multiple NTP servers with fallback
 * 
 * Tries each server in sequence until one succeeds.
 * Useful for resilience if primary server is unavailable.
 * 
 * @param ntp_servers Array of server hostnames
 * @param num_servers Number of servers in array
 * @param timeout_ms_per_server Timeout for each attempt
 * @return true if any server succeeded
 */
bool ESP32TimeSync::syncTimeMulti(const char* const* ntp_servers,
                                  int num_servers,
                                  uint32_t timeout_ms_per_server) {
  if (ntp_servers == nullptr || num_servers <= 0) {
    return false;
  }
  
  for (int i = 0; i < num_servers; i++) {
    if (ntp_servers[i] != nullptr) {
      if (syncTime(ntp_servers[i], timeout_ms_per_server)) {
        return true;
      }
    }
  }
  
  return false;
}

/**
 * @brief Get current Unix timestamp
 * 
 * @return Seconds since epoch (1970-01-01 00:00:00 UTC)
 */
time_t ESP32TimeSync::getUnixTime() {
  return time(nullptr);
}

/**
 * @brief Check if system time has been synchronized
 * 
 * Uses a heuristic: if localtime returns a year >= MIN_VALID_YEAR,
 * we assume time is valid. This is needed because:
 * - Before NTP sync, ESP32 might have year 1970 or 1900
 * - After NTP sync, year should be current year
 * 
 * @return true if time appears valid, false otherwise
 */
bool ESP32TimeSync::isTimeSynced() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  if (timeinfo == nullptr) {
    return false;
  }
  
  // tm_year is years since 1900
  int current_year = timeinfo->tm_year + 1900;
  
  return current_year >= MIN_VALID_YEAR;
}

/**
 * @brief Get human-readable date/time string
 * 
 * Format: "YYYY-MM-DD HH:MM:SS UTC"
 * Example: "2026-04-27 14:30:45 UTC"
 * 
 * @param buffer Output buffer (minimum 26 bytes recommended)
 * @param buffer_size Size of buffer
 * @return Pointer to buffer
 */
char* ESP32TimeSync::getTimeString(char* buffer, size_t buffer_size) {
  if (buffer == nullptr || buffer_size < 26) {
    return buffer;
  }
  
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  if (timeinfo == nullptr) {
    snprintf(buffer, buffer_size, "Time not set");
    return buffer;
  }
  
  snprintf(buffer, buffer_size, "%04d-%02d-%02d %02d:%02d:%02d UTC",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday,
           timeinfo->tm_hour,
           timeinfo->tm_min,
           timeinfo->tm_sec);
  
  return buffer;
}
