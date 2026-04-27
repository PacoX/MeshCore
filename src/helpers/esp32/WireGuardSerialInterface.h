#pragma once

#include "../BaseSerialInterface.h"
#include "WireGuardManager.h"
#include "WireGuardConfig.h"
#include <WiFi.h>

/**
 * @brief Serial Interface over WireGuard VPN Tunnel
 * 
 * Provides a BaseSerialInterface implementation that:
 * 1. Manages WiFi + WireGuard tunnel via WireGuardManager
 * 2. Creates a TCP server listening on the VPN IP (e.g., 10.0.0.2:5000)
 * 3. Handles packet framing (same format as SerialWifiInterface)
 * 
 * This allows MeshCore applications to use TCP communication through
 * a secure, encrypted VPN tunnel without any changes to their code.
 * 
 * Usage:
 *   WireGuardSerialInterface serial_interface;
 *   WireGuardConfig cfg = { ... };
 *   serial_interface.begin(cfg, 5000);  // 5000 = TCP port
 *   
 *   while (true) {
 *     serial_interface.loop();
 *     if (serial_interface.isConnected()) {
 *       // Can send/receive packets now
 *     }
 *   }
 */
class WireGuardSerialInterface : public BaseSerialInterface {
public:
  /**
   * @brief Default constructor
   */
  WireGuardSerialInterface();
  
  /**
   * @brief Initialize with WireGuard configuration
   * 
   * Starts:
   * - WiFi connection
   * - NTP time sync
   * - WireGuard tunnel
   * - TCP server (once tunnel is up)
   * 
   * @param config WireGuard configuration
   * @param tcp_port Port for TCP server (typically 5000)
   * @return true if initialization started successfully
   */
  bool begin(const WireGuardConfig& config, uint16_t tcp_port = 5000);
  
  /**
   * @brief Periodic update - call from main loop
   * 
   * Handles:
   * - WireGuard state machine
   * - TCP server accept/read/write
   * - Frame queuing
   * 
   * Must be called every loop iteration.
   */
  void loop();
  
  /**
   * @brief Shutdown the interface
   * 
   * Closes TCP connection, stops WireGuard, disconnects WiFi.
   */
  void end();
  
  // ------ BaseSerialInterface implementation ------
  
  /**
   * @brief Enable the interface
   * 
   * Clears buffers and enables packet processing.
   */
  void enable() override;
  
  /**
   * @brief Disable the interface
   * 
   * Stops packet processing.
   */
  void disable() override;
  
  /**
   * @brief Check if interface is enabled
   */
  bool isEnabled() const override { return _is_enabled; }
  
  /**
   * @brief Check if a client is connected
   */
  bool isConnected() const override;
  
  /**
   * @brief Check if transmit is still in progress
   */
  bool isWriteBusy() const override;
  
  /**
   * @brief Write frame to send queue
   * 
   * @param src Packet data
   * @param len Packet length
   * @return Number of bytes written (len if successful, 0 on failure)
   */
  size_t writeFrame(const uint8_t src[], size_t len) override;
  
  /**
   * @brief Check for incoming frame
   * 
   * @param dest Buffer to receive frame (must be MAX_FRAME_SIZE)
   * @return Number of bytes received (0 if no frame available)
   */
  size_t checkRecvFrame(uint8_t dest[]) override;
  
  // ------ Status methods ------
  
  /**
   * @brief Get WireGuard connection status
   * 
   * @return true if VPN tunnel is connected
   */
  bool isVPNConnected() const;
  
  /**
   * @brief Get human-readable status string
   * 
   * @param buffer Output buffer
   * @param buffer_size Buffer size
   * @return Pointer to buffer
   */
  char* getStatus(char* buffer, size_t buffer_size);

private:
  // Configuration
  WireGuardConfig _wg_config;            ///< Stored WireGuard config
  uint16_t _tcp_port;                    ///< TCP port to listen on
  bool _is_enabled;                      ///< Interface enabled flag
  bool _initialized;                     ///< begin() has been called
  
  // WireGuard management
  WireGuardManager _wg_mgr;              ///< Manages VPN tunnel
  
  // TCP server and client
  WiFiServer _server;                    ///< TCP server
  WiFiClient _client;                    ///< Current connected client
  bool _server_started;                  ///< TCP server started flag
  bool _device_connected;                ///< Tracks client connection status
  
  // Frame queuing (same pattern as SerialWifiInterface)
  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };
  
  static constexpr int FRAME_QUEUE_SIZE = 4;
  Frame _send_queue[FRAME_QUEUE_SIZE];   ///< Outgoing frames
  int _send_queue_len;                   ///< Number of frames in send queue
  Frame _recv_queue[FRAME_QUEUE_SIZE];   ///< Incoming frames
  int _recv_queue_len;                   ///< Number of frames in receive queue
  
  // Frame header parsing
  struct FrameHeader {
    uint8_t type;
    uint16_t length;
  };
  FrameHeader _received_frame_header;    ///< Current frame header being parsed
  
  // Timing
  unsigned long _last_write;             ///< Time of last write
  
  // Helper methods
  void _clearBuffers();
  void _startTCPServer();
  void _handleTCPClient();
  void _processSendQueue();
  void _processRecvQueue();
  bool _hasReceivedFrameHeader() const;
  void _resetReceivedFrameHeader();
};
