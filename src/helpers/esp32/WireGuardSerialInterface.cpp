#include "WireGuardSerialInterface.h"
#include <Arduino.h>
#include <cstring>

#ifdef WIREGUARD_ENABLED

/**
 * @brief Constructor
 */
WireGuardSerialInterface::WireGuardSerialInterface()
  : _server(WiFiServer(5000)),
    _client(WiFiClient()),
    _tcp_port(5000),
    _is_enabled(false),
    _initialized(false),
    _server_started(false),
    _device_connected(false),
    _send_queue_len(0),
    _recv_queue_len(0),
    _last_write(0) {
  _clearBuffers();
  _resetReceivedFrameHeader();
  
  // Initialize frame structures
  for (int i = 0; i < FRAME_QUEUE_SIZE; i++) {
    _send_queue[i].len = 0;
    _recv_queue[i].len = 0;
  }
}

/**
 * @brief Initializer with WireGuard config
 */
bool WireGuardSerialInterface::begin(const WireGuardConfig& config,
                                      uint16_t tcp_port) {
  if (_initialized) {
    return false;  // Already initialized
  }
  
  _wg_config = config;
  _tcp_port = tcp_port;
  _initialized = true;
  
  // Start WireGuard manager
  if (!_wg_mgr.begin(_wg_config)) {
    return false;
  }
  
  return true;
}

/**
 * @brief Shutdown
 */
void WireGuardSerialInterface::end() {
  _wg_mgr.end();
  _client.stop();
  _server_started = false;
  _device_connected = false;
  _initialized = false;
  _clearBuffers();
}

/**
 * @brief Enable interface
 */
void WireGuardSerialInterface::enable() {
  if (_is_enabled) return;
  _is_enabled = true;
  _clearBuffers();
}

/**
 * @brief Disable interface
 */
void WireGuardSerialInterface::disable() {
  _is_enabled = false;
}

/**
 * @brief Check if client connected
 */
bool WireGuardSerialInterface::isConnected() const {
  // WiFiClient::connected() is non-const in ESP32 Arduino, so we rely on
  // the cached connection state maintained in loop()/checkRecvFrame().
  return _device_connected;
}

/**
 * @brief Check if write is busy (never true in our implementation)
 */
bool WireGuardSerialInterface::isWriteBusy() const {
  return false;
}

/**
 * @brief Main loop - update everything
 */
void WireGuardSerialInterface::loop() {
  if (!_initialized || !_is_enabled) {
    return;
  }
  
  // Update WireGuard state machine
  WireGuardManager::State wg_state = _wg_mgr.loop();
  
  // Start TCP server once VPN tunnel is up
  if (wg_state == WireGuardManager::STATE_CONNECTED && !_server_started) {
    _startTCPServer();
  }
  
  // Handle TCP client connections/data
  if (_device_connected || _client.connected()) {
    _handleTCPClient();
  } else if (wg_state == WireGuardManager::STATE_CONNECTED) {
    // Check for new client only when VPN is connected
    WiFiClient new_client = _server.available();
    if (new_client) {
      _device_connected = true;
      _client = new_client;
      _clearBuffers();
    }
  }
  
  // Process outgoing frames
  _processSendQueue();
  
  // Process incoming frames
  _processRecvQueue();
}

/**
 * @brief Start TCP server on VPN IP
 */
void WireGuardSerialInterface::_startTCPServer() {
  // Recreate server to ensure it binds to correct interface
  // (after WireGuard tunnel is up)
  _server = WiFiServer(_tcp_port);
  _server.begin();
  _server_started = true;
}

/**
 * @brief Handle TCP client connection
 */
void WireGuardSerialInterface::_handleTCPClient() {
  if (!_client.connected()) {
    _device_connected = false;
    _client.stop();
    return;
  }
  
  if (!_device_connected) {
    _device_connected = true;
  }
}

/**
 * @brief Write frame to send queue
 */
size_t WireGuardSerialInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE || !_is_enabled) {
    return 0;
  }
  
  if (!_device_connected || _send_queue_len >= FRAME_QUEUE_SIZE) {
    return 0;
  }
  
  // Add to queue
  _send_queue[_send_queue_len].len = len;
  memcpy(_send_queue[_send_queue_len].buf, src, len);
  _send_queue_len++;
  
  return len;
}

/**
 * @brief Process send queue and transmit
 */
void WireGuardSerialInterface::_processSendQueue() {
  if (_send_queue_len == 0 || !_device_connected) {
    return;
  }
  
  // Send first frame in queue
  Frame& frame = _send_queue[0];
  
  // Frame format: '>' + LSB length + MSB length + data
  uint8_t pkt[3 + frame.len];
  pkt[0] = '>';
  pkt[1] = (frame.len & 0xFF);      // LSB
  pkt[2] = (frame.len >> 8);        // MSB
  memcpy(&pkt[3], frame.buf, frame.len);
  
  if (_client.write(pkt, 3 + frame.len) > 0) {
    _last_write = millis();
    
    // Remove from queue
    _send_queue_len--;
    for (int i = 0; i < _send_queue_len; i++) {
      _send_queue[i] = _send_queue[i + 1];
    }
  }
}

/**
 * @brief Process receive queue
 */
void WireGuardSerialInterface::_processRecvQueue() {
  if (_recv_queue_len == 0 || !_device_connected) {
    return;
  }
  
  // Data already in queue, nothing to do
  // (caller will get it via checkRecvFrame)
}

/**
 * @brief Check for received frame
 */
size_t WireGuardSerialInterface::checkRecvFrame(uint8_t dest[]) {
  if (!_is_enabled || !_device_connected) {
    return 0;
  }
  
  if (!_client.connected()) {
    if (_device_connected) {
      _device_connected = false;
      _resetReceivedFrameHeader();
    }
    return 0;
  }
  
  // Check for new client connection (should only happen once)
  WiFiClient new_client = _server.available();
  if (new_client) {
    _device_connected = true;
    _client = new_client;
    _clearBuffers();
  }
  
  // If not connected, nothing to do
  if (!_device_connected) {
    return 0;
  }
  
  // Check if we need to read frame header
  if (!_hasReceivedFrameHeader()) {
    // Frame header is 3 bytes: (1 byte type) + (2 bytes length)
    int frame_header_size = 3;
    if (_client.available() >= frame_header_size) {
      // Read frame header: '>' + LSB + MSB
      _client.readBytes((char*)&_received_frame_header.type, 1);
      uint8_t length_bytes[2];
      _client.readBytes((char*)length_bytes, 2);
      _received_frame_header.length = length_bytes[0] | (length_bytes[1] << 8);

      // We only accept app-to-device frames.
      if (_received_frame_header.type != '<') {
        int skip = _received_frame_header.length;
        while (skip > 0 && _client.available() > 0) {
          uint8_t trash[1];
          int read_n = _client.read(trash, 1);
          if (read_n <= 0) {
            break;
          }
          skip -= read_n;
        }
        _resetReceivedFrameHeader();
      }
    }
    return 0;  // Still reading header
  }
  
  // We have frame header, check if frame data is available
  int frame_length = _received_frame_header.length;
  int available = _client.available();
  
  if (frame_length > available) {
    // Not all data available yet
    return 0;
  }
  
  // Check for invalid frame size
  if (frame_length > MAX_FRAME_SIZE) {
    // Skip this invalid frame
    while (frame_length > 0 && _client.available() > 0) {
      uint8_t skip[1];
      int skipped = _client.read(skip, 1);
      if (skipped <= 0) {
        break;
      }
      frame_length -= skipped;
    }
    _resetReceivedFrameHeader();
    return 0;
  }
  
  // Frame is complete and valid - read it
  if (_recv_queue_len >= FRAME_QUEUE_SIZE) {
    return 0;  // Queue full
  }
  
  Frame& frame = _recv_queue[_recv_queue_len];
  int read_len = _client.read(frame.buf, frame_length);
  if (read_len <= 0) {
    _resetReceivedFrameHeader();
    return 0;
  }
  frame.len = (uint8_t)read_len;
  _recv_queue_len++;
  
  // Reset header for next frame
  _resetReceivedFrameHeader();
  
  // Return first frame from queue
  if (_recv_queue_len > 0) {
    Frame& return_frame = _recv_queue[0];
    size_t len = return_frame.len;
    memcpy(dest, return_frame.buf, len);
    
    // Remove from queue
    _recv_queue_len--;
    for (int i = 0; i < _recv_queue_len; i++) {
      _recv_queue[i] = _recv_queue[i + 1];
    }
    
    return len;
  }
  
  return 0;
}

/**
 * @brief Check if VPN connected
 */
bool WireGuardSerialInterface::isVPNConnected() const {
  return _wg_mgr.isConnected();
}

/**
 * @brief Get status string
 */
char* WireGuardSerialInterface::getStatus(char* buffer, size_t buffer_size) {
  if (buffer == nullptr || buffer_size < 20) {
    return buffer;
  }
  
  return _wg_mgr.getStatus(buffer, buffer_size);
}

/**
 * @brief Clear send/receive queues
 */
void WireGuardSerialInterface::_clearBuffers() {
  _send_queue_len = 0;
  _recv_queue_len = 0;
}

/**
 * @brief Check if frame header is valid
 */
bool WireGuardSerialInterface::_hasReceivedFrameHeader() const {
  return _received_frame_header.type != 0 && _received_frame_header.length != 0;
}

/**
 * @brief Reset frame header state
 */
void WireGuardSerialInterface::_resetReceivedFrameHeader() {
  _received_frame_header.type = 0;
  _received_frame_header.length = 0;
}

#endif
