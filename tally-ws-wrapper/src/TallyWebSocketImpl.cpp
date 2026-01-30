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

LwsWebSocketClient::LwsWebSocketClient() {
    // Context will be created on Connect()
}

LwsWebSocketClient::~LwsWebSocketClient() {
    Cleanup();
}

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

bool LwsWebSocketClient::IsConnected() const {
    return m_state == ConnectionState::Connected;
}

ConnectionState LwsWebSocketClient::GetState() const {
    return m_state;
}

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

void LwsWebSocketClient::SetMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_messageCallback = std::move(callback);
}

void LwsWebSocketClient::SetStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

int LwsWebSocketClient::Poll(int timeoutMs) {
    if (!m_context) {
        return -1;
    }
    return lws_service(m_context, timeoutMs);
}

void LwsWebSocketClient::SetState(ConnectionState newState, const WebSocketError* error) {
    m_state = newState;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_stateCallback) {
        m_stateCallback(newState, error);
    }
}

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

// Static callback for libwebsockets
int LwsWebSocketClient::LwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                                    void* user, void* in, size_t len)
{
    LwsWebSocketClient* self = nullptr;

    // Get our instance pointer
    if (user) {
        self = static_cast<LwsWebSocketClient*>(user);
    } else {
        struct lws_context* ctx = lws_get_context(wsi);
        if (ctx) {
            self = static_cast<LwsWebSocketClient*>(lws_context_user(ctx));
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

            std::lock_guard<std::mutex> lock(self->m_callbackMutex);
            if (self->m_messageCallback) {
                self->m_messageCallback(static_cast<const uint8_t*>(in), len, type);
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

// Factory function
std::unique_ptr<IWebSocketClient> CreateWebSocketClient() {
    return std::make_unique<LwsWebSocketClient>();
}

// Version function
std::string GetWebSocketLibraryVersion() {
    return std::string("libwebsockets ") + lws_get_library_version();
}

} // namespace Tally
