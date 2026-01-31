/*
 * TallyWebSocketImpl.cpp - libwebsockets implementation of TallyWebSocket
 *
 * This implementation uses libwebsockets Secure Streams API for
 * WebSocket client functionality.
 */

#include "TallyWebSocket.h"
#include <libwebsockets.h>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstring>

namespace Tally {

/**
 * @brief libwebsockets implementation of IWebSocketClient
 */
class LwsWebSocketClient : public IWebSocketClient {
public:
    LwsWebSocketClient();
    ~LwsWebSocketClient() override;

    // IWebSocketClient interface
    bool Connect(const WebSocketConfig& config) override;
    void Disconnect(int code, const std::string& reason) override;
    bool IsConnected() const override;
    ConnectionState GetState() const override;
    bool SendText(const std::string& message) override;
    bool SendBinary(const uint8_t* data, size_t length) override;
    void SetMessageCallback(MessageCallback callback) override;
    void SetStateCallback(StateCallback callback) override;
    int Poll(int timeoutMs) override;

private:
    // libwebsockets context and connection
    struct lws_context* m_context = nullptr;
    struct lws* m_wsi = nullptr;

    // State
    std::atomic<ConnectionState> m_state{ConnectionState::Disconnected};
    WebSocketConfig m_config;

    // Callbacks
    MessageCallback m_messageCallback;
    StateCallback m_stateCallback;
    std::mutex m_callbackMutex;

    // Send queue
    struct QueuedMessage {
        std::vector<uint8_t> data;
        MessageType type;
    };
    std::queue<QueuedMessage> m_sendQueue;
    std::mutex m_queueMutex;

public:
    // libwebsockets callback
    static int LwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                          void* user, void* in, size_t len);

private:

    // Helper methods
    void SetState(ConnectionState newState, const WebSocketError* error = nullptr);
    void ProcessSendQueue();
    void Cleanup();
};

// Protocol definition for libwebsockets
static const struct lws_protocols protocols[] = {
    {
        "tally-ws",                             // protocol name
        LwsWebSocketClient::LwsCallback,        // callback
        sizeof(LwsWebSocketClient*),            // per-session data size
        4096,                                   // rx buffer size
        0, nullptr, 0                           // unused fields
    },
    LWS_PROTOCOL_LIST_TERM
};

/**
 * @brief Constructs a LwsWebSocketClient with no active libwebsockets context or connection.
 *
 * The client starts in the Disconnected state; creation of the libwebsockets context is deferred
 * until Connect() is called.
 */
LwsWebSocketClient::LwsWebSocketClient() {
    // Context will be created on Connect()
}

/**
 * @brief Destroy the WebSocket client and release all associated resources.
 *
 * Ensures any active connection and libwebsockets context are cleaned up before
 * the object is destroyed.
 */
LwsWebSocketClient::~LwsWebSocketClient() {
    Cleanup();
}

/**
 * @brief Release libwebsockets resources and reset client state.
 *
 * Destroys the libwebsockets context if one exists, clears the stored
 * connection handle, and sets the client's connection state to
 * ConnectionState::Disconnected.
 *
 * Note: this does not perform a graceful close handshake for an active
 * connection; any connection-specific cleanup is expected to occur via
 * the libwebsockets callback flow.
 */
void LwsWebSocketClient::Cleanup() {
    if (m_wsi) {
        // Connection cleanup happens in callback
        m_wsi = nullptr;
    }
    if (m_context) {
        lws_context_destroy(m_context);
        m_context = nullptr;
    }
    m_state = ConnectionState::Disconnected;
}

/**
 * @brief Initiates a client connection using the provided WebSocket configuration.
 *
 * Attempts to create a libwebsockets context and start a client connection to the URL
 * specified in the config; on success the connection proceeds asynchronously via the
 * library's callbacks and the client's state is advanced from Disconnected to Connecting.
 *
 * @param config Configuration for the connection (URL, optional TLS/settings, and related options).
 * @return true if the connection initiation succeeded and the client will proceed via callbacks, false if setup failed or the client was not in a disconnected state.
 */
bool LwsWebSocketClient::Connect(const WebSocketConfig& config) {
    if (m_state != ConnectionState::Disconnected) {
        return false; // Already connected or connecting
    }

    m_config = config;
    SetState(ConnectionState::Connecting);

    // Create lws context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN; // Client only
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = this;

    m_context = lws_create_context(&info);
    if (!m_context) {
        WebSocketError err{-1, "Failed to create lws context"};
        SetState(ConnectionState::Error, &err);
        return false;
    }

    // Parse URL and create connection
    const char* prot = nullptr;
    const char* address = nullptr;
    int port = 0;
    const char* path = nullptr;
    
    // Use vector for dynamic sizing and to ensure null termination
    std::vector<char> urlBuf(config.url.begin(), config.url.end());
    urlBuf.push_back('\0');

    if (lws_parse_uri(urlBuf.data(), &prot, &address, &port, &path) != 0) {
        WebSocketError err{-1, "Failed to parse URL"};
        SetState(ConnectionState::Error, &err);
        Cleanup();
        return false;
    }

    // Prepare connection info
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = m_context;
    ccinfo.address = address;
    ccinfo.port = port;
    ccinfo.path = path;
    ccinfo.host = address;
    ccinfo.origin = address;
    ccinfo.protocol = protocols[0].name;
    if (!m_config.subprotocol.empty()) {
        ccinfo.protocol = m_config.subprotocol.c_str();
    }
    ccinfo.userdata = this;

    // Check for TLS (wss://)
    if (prot && (strcmp(prot, "wss") == 0 || strcmp(prot, "https") == 0)) {
        ccinfo.ssl_connection = LCCSCF_USE_SSL;
    }

    m_wsi = lws_client_connect_via_info(&ccinfo);
    if (!m_wsi) {
        WebSocketError err{-1, "Failed to create client connection"};
        SetState(ConnectionState::Error, &err);
        Cleanup();
        return false;
    }

    return true;
}

/**
 * @brief Initiates a graceful disconnect sequence for the client connection.
 *
 * If the client is already disconnected this is a no-op. Otherwise the method
 * transitions the internal state to Disconnecting and, when a libwebsockets
 * connection exists, requests a close with the supplied status code and reason
 * and schedules the connection to become writable so the close handshake can
 * proceed. Final resource cleanup is performed asynchronously via the library
 * callbacks.
 *
 * @param code WebSocket close status code to send to the peer.
 * @param reason Human-readable reason to include with the close frame.
 */
void LwsWebSocketClient::Disconnect(int code, const std::string& reason) {
    if (m_state == ConnectionState::Disconnected) {
        return;
    }

    SetState(ConnectionState::Disconnecting);

    if (m_wsi) {
        // Request graceful close
        lws_close_reason(m_wsi, (enum lws_close_status)code,
                        (unsigned char*)reason.c_str(), reason.length());
        lws_callback_on_writable(m_wsi);
    }

    // Cleanup will happen in callback or force it
    // Cleanup(); // Removed immediate cleanup to allow close handshake to complete
}

/**
 * @brief Indicates whether the client currently has an active connection.
 *
 * @return `true` if the client's connection state is Connected, `false` otherwise.
 */
bool LwsWebSocketClient::IsConnected() const {
    return m_state == ConnectionState::Connected;
}

/**
 * @brief Retrieve the client's current connection state.
 *
 * @return ConnectionState The current connection state (e.g., Disconnected, Connecting, Connected, Disconnecting, Error).
 */
ConnectionState LwsWebSocketClient::GetState() const {
    return m_state;
}

/**
 * @brief Queues a text message for transmission over the active WebSocket connection.
 *
 * If the client is connected, the message is copied into the internal send queue and the
 * connection is scheduled to become writable so the queued message can be sent asynchronously.
 *
 * @param message UTF-8 text message to send.
 * @return true if the message was queued for send, false if the client was not connected.
 */
bool LwsWebSocketClient::SendText(const std::string& message) {
    if (!IsConnected()) {
        return false;
    }

    QueuedMessage msg;
    msg.type = MessageType::Text;
    msg.data.resize(LWS_PRE + message.size());
    memcpy(msg.data.data() + LWS_PRE, message.data(), message.size());

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_sendQueue.push(std::move(msg));
    }

    if (m_wsi) {
        lws_callback_on_writable(m_wsi);
    }

    return true;
}

/**
 * @brief Queues a binary message for transmission over the active WebSocket connection.
 *
 * If the client is connected, the provided byte buffer is copied into an internal send queue
 * and the connection is scheduled for writing so the message will be sent on the next writable callback.
 *
 * @param data Pointer to the binary payload to send.
 * @param length Number of bytes at `data` to send.
 * @return true if the message was accepted and queued for sending, false if the client is not connected.
 */
bool LwsWebSocketClient::SendBinary(const uint8_t* data, size_t length) {
    if (!IsConnected()) {
        return false;
    }

    QueuedMessage msg;
    msg.type = MessageType::Binary;
    msg.data.resize(LWS_PRE + length);
    memcpy(msg.data.data() + LWS_PRE, data, length);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_sendQueue.push(std::move(msg));
    }

    if (m_wsi) {
        lws_callback_on_writable(m_wsi);
    }

    return true;
}

/**
 * @brief Registers the callback to be invoked when an incoming WebSocket message arrives.
 *
 * The provided callback replaces any previously registered callback and is stored in a
 * thread-safe manner.
 *
 * @param callback Callable invoked for each received message with parameters (data, length, type).
 */
void LwsWebSocketClient::SetMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_messageCallback = std::move(callback);
}

/**
 * @brief Registers a callback to be invoked when the connection state changes.
 *
 * The provided callback replaces any previously registered state callback. Supplying
 * an empty/null callback clears the existing callback. The callback is stored
 * with internal synchronization and will be called from the client's event handling
 * path when the connection state changes.
 *
 * @param callback Function to invoke on state changes; receives the new ConnectionState.
 */
void LwsWebSocketClient::SetStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

/**
 * @brief Runs the libwebsockets service loop for the client context.
 *
 * Blocks up to the specified timeout while processing libwebsockets events for the client's context.
 *
 * @param timeoutMs Maximum time to block in milliseconds.
 * @return int `-1` if the client context is not initialized; otherwise the integer result returned by the libwebsockets service call.
 */
int LwsWebSocketClient::Poll(int timeoutMs) {
    if (!m_context) {
        return -1;
    }
    return lws_service(m_context, timeoutMs);
}

/**
 * @brief Update the client's connection state and notify the registered state callback.
 *
 * Sets the internal connection state to @p newState and, if a state callback has been
 * registered, invokes it with @p newState and @p error.
 *
 * @param newState The new connection state to apply.
 * @param error Optional pointer to a WebSocketError providing additional error context; may be `nullptr`.
 */
void LwsWebSocketClient::SetState(ConnectionState newState, const WebSocketError* error) {
    m_state = newState;

    StateCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        callback = m_stateCallback;
    }

    if (callback) {
        callback(newState, error);
    }
}

/**
 * @brief Sends the next message from the send queue and schedules further writes if needed.
 *
 * Dequeues a single queued message in a thread-safe manner, writes its payload to the
 * current libwebsockets connection using the appropriate text or binary frame type,
 * and, if additional queued messages remain, requests another writable callback so
 * remaining messages will be sent. Uses the LWS_PRE offset when writing.
 *
 * If the write fails, the function returns and any error handling is performed via
 * the libwebsockets callbacks.
 */
void LwsWebSocketClient::ProcessSendQueue() {
    QueuedMessage msg;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_sendQueue.empty() || !m_wsi) {
            return;
        }
        msg = std::move(m_sendQueue.front());
        m_sendQueue.pop();
    }

    enum lws_write_protocol wp = (msg.type == MessageType::Text)
        ? LWS_WRITE_TEXT
        : LWS_WRITE_BINARY;

    size_t dataLen = msg.data.size() - LWS_PRE;
    int written = lws_write(m_wsi, msg.data.data() + LWS_PRE, dataLen, wp);

    if (written < 0) {
        // Error - will be handled in callback
        return;
    }

    bool hasMore;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        hasMore = !m_sendQueue.empty();
    }
    
    if (hasMore && m_wsi) {
        lws_callback_on_writable(m_wsi);
    }
}

/**
 * @brief libwebsockets static callback that dispatches client connection events to the LwsWebSocketClient instance.
 *
 * Routes libwebsockets client events to the corresponding instance behavior:
 * - On established: stores the connection wsi and updates state to Connected.
 * - On receive: determines message type (text or binary) and invokes the configured message callback with the payload.
 * - On writable: processes the send queue to write pending messages.
 * - On connection error: constructs a WebSocketError from the provided error text, updates state to Error, and clears the connection handle.
 * - On closed: clears the connection handle and updates state to Disconnected.
 *
 * @param wsi The libwebsockets session handle for this event (may be null for some context-level events).
 * @param reason The libwebsockets callback reason indicating the event type.
 * @param user Per-session user data; may point to the LwsWebSocketClient instance.
 * @param in Pointer to event-specific input data (e.g., received payload or error text).
 * @param len Length of the data pointed to by `in`.
 * @return int Always returns 0.
 */
int LwsWebSocketClient::LwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                                    void* user, void* in, size_t len)
{
    LwsWebSocketClient* self = nullptr;

    // Get our instance pointer from context user data (safer than using 'user' param directly)
    struct lws_context* ctx = lws_get_context(wsi);
    if (ctx) {
        void* user_data = lws_context_user(ctx);
        if (user_data) {
            self = static_cast<LwsWebSocketClient*>(user_data);
        }
    }

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        if (self) {
            self->m_wsi = wsi;
            self->SetState(ConnectionState::Connected);
        }
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        if (self && self->m_messageCallback) {
            MessageType type = lws_frame_is_binary(wsi)
                ? MessageType::Binary
                : MessageType::Text;

            MessageCallback callback;
            {
                std::lock_guard<std::mutex> lock(self->m_callbackMutex);
                callback = self->m_messageCallback;
            }

            if (callback) {
                callback(static_cast<const uint8_t*>(in), len, type);
            }
        }
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (self) {
            self->ProcessSendQueue();
        }
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        if (self) {
            WebSocketError err{-1, in ? std::string(static_cast<const char*>(in), len) : "Connection error"};
            self->SetState(ConnectionState::Error, &err);
            self->m_wsi = nullptr;
        }
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        if (self) {
            self->m_wsi = nullptr;
            self->SetState(ConnectionState::Disconnected);
        }
        break;

    default:
        break;
    }

    return 0;
}

/**
 * @brief Creates a new IWebSocketClient backed by the libwebsockets Secure Streams implementation.
 *
 * Constructs and returns a heap-allocated LwsWebSocketClient instance wrapped in a unique_ptr.
 *
 * @return std::unique_ptr<IWebSocketClient> A unique pointer owning a new IWebSocketClient implementation.
 */
std::unique_ptr<IWebSocketClient> CreateWebSocketClient() {
    return std::make_unique<LwsWebSocketClient>();
}

/**
 * @brief Returns the libwebsockets library version prefixed with "libwebsockets ".
 *
 * @return std::string A string like "libwebsockets X.Y.Z" containing the library name and its version.
 */
std::string GetWebSocketLibraryVersion() {
    return std::string("libwebsockets ") + lws_get_library_version();
}

} // namespace Tally