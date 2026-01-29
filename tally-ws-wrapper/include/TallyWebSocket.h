/*
 * TallyWebSocket.h - WebSocket abstraction layer for Tally
 *
 * This header provides a thin C++ wrapper over libwebsockets,
 * allowing Tally to use WebSocket functionality without tight
 * coupling to the underlying library.
 *
 * The library can be replaced by implementing this interface
 * with a different backend if needed.
 */

#ifndef TALLY_WEBSOCKET_H
#define TALLY_WEBSOCKET_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace Tally {

/**
 * @brief Connection states for WebSocket
 */
enum class ConnectionState {
    Disconnected,   ///< Not connected
    Connecting,     ///< Connection in progress
    Connected,      ///< Connected and ready
    Disconnecting,  ///< Disconnect in progress
    Error           ///< Error state
};

/**
 * @brief Message types for WebSocket
 */
enum class MessageType {
    Text,           ///< UTF-8 text message
    Binary          ///< Binary data message
};

/**
 * @brief Configuration for WebSocket connection
 */
struct WebSocketConfig {
    std::string url;                    ///< WebSocket URL (ws:// or wss://)
    std::string subprotocol;            ///< Optional subprotocol
    int connectTimeoutMs = 30000;       ///< Connection timeout in milliseconds
    int pingIntervalMs = 30000;         ///< Ping interval (0 to disable)
    bool autoReconnect = false;         ///< Auto-reconnect on disconnect
    int reconnectDelayMs = 5000;        ///< Delay between reconnect attempts
    int maxReconnectAttempts = 5;       ///< Max reconnect attempts (0 = unlimited)
};

/**
 * @brief Error information
 */
struct WebSocketError {
    int code;                           ///< Error code
    std::string message;                ///< Human-readable error message
};

/**
 * @brief Abstract interface for WebSocket client
 *
 * This interface defines the contract for WebSocket operations.
 * Implementations can use libwebsockets, IXWebSocket, or any
 * other WebSocket library.
 */
class IWebSocketClient {
public:
    virtual ~IWebSocketClient() = default;

    /**
     * @brief Message callback type
     * @param data Message data
     * @param length Message length
     * @param type Message type (text/binary)
     */
    using MessageCallback = std::function<void(const uint8_t* data, size_t length, MessageType type)>;

    /**
     * @brief State change callback type
     * @param newState The new connection state
     * @param error Error details (only valid when state is Error)
     */
    using StateCallback = std::function<void(ConnectionState newState, const WebSocketError* error)>;

    // --- Connection Lifecycle ---

    /**
     * @brief Connect to a WebSocket server
     * @param config Connection configuration
     * @return true if connection attempt started, false on immediate failure
     */
    virtual bool Connect(const WebSocketConfig& config) = 0;

    /**
     * @brief Disconnect from the server
     * @param code Optional close code (default: 1000 = normal closure)
     * @param reason Optional close reason
     */
    virtual void Disconnect(int code = 1000, const std::string& reason = "") = 0;

    /**
     * @brief Check if currently connected
     * @return true if connected
     */
    virtual bool IsConnected() const = 0;

    /**
     * @brief Get current connection state
     * @return Current state
     */
    virtual ConnectionState GetState() const = 0;

    // --- Messaging ---

    /**
     * @brief Send a text message
     * @param message UTF-8 text message
     * @return true if queued successfully
     */
    virtual bool SendText(const std::string& message) = 0;

    /**
     * @brief Send a binary message
     * @param data Binary data
     * @param length Data length
     * @return true if queued successfully
     */
    virtual bool SendBinary(const uint8_t* data, size_t length) = 0;

    // --- Callbacks ---

    /**
     * @brief Set message received callback
     * @param callback Function to call when message is received
     */
    virtual void SetMessageCallback(MessageCallback callback) = 0;

    /**
     * @brief Set state change callback
     * @param callback Function to call when state changes
     */
    virtual void SetStateCallback(StateCallback callback) = 0;

    // --- Event Loop ---

    /**
     * @brief Process pending events (non-blocking)
     *
     * Call this periodically from your main loop if not using
     * a dedicated thread. For threaded operation, the implementation
     * manages this internally.
     *
     * @param timeoutMs Maximum time to wait for events
     * @return Number of events processed, or -1 on error
     */
    virtual int Poll(int timeoutMs = 0) = 0;
};

/**
 * @brief Factory function to create a WebSocket client
 *
 * Creates an instance of the default WebSocket client implementation.
 *
 * @return Unique pointer to a WebSocket client
 */
std::unique_ptr<IWebSocketClient> CreateWebSocketClient();

/**
 * @brief Get library version string
 * @return Version string (e.g., "libwebsockets 4.5.99")
 */
std::string GetWebSocketLibraryVersion();

} // namespace Tally

#endif // TALLY_WEBSOCKET_H
