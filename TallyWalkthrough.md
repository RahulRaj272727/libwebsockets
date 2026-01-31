# Tally libwebsockets Integration Walkthrough

> **Source:** AI Model Context Transfer  
> **Date:** January 30, 2026  
> **Status:** In Progress

---

## Summary

| Phase | Status |
|-------|--------|
| Repository Setup | ✅ Complete |
| Windows Build | ✅ Complete (No TLS) |
| Abstraction Layer | ✅ Complete |
| Demo App | ✅ Complete |
| TLS Build | ⚠️ Pending (SChannel fails, need OpenSSL) |
| Build Output | `websockets_static.lib` (1.5 MB) |

---

## Repository Setup

| Item | Value |
|------|-------|
| Fork | `github.com/RahuIMallya272727/libwebsockets` |
| Local Clone | `d:\ALL_BINS\TallylibwebsocketsPOC` |
| Upstream Remote | Configured |

---

## Build Output Files

```
build/lib/Release/
├── websockets.exp
├── websockets.lib          ← Import library
└── websockets_static.lib   ← Primary static library (1.5 MB)
```

---

## Architecture: TallyWebSocket Abstraction Layer

### Purpose
Implementation wrapper using libwebsockets low-level API.  
Provides clean C++ interface with callbacks for Tally integration.

### Actual File Structure (Verified)

```
tally-ws-wrapper/
├── include/
│   └── TallyWebSocket.h       # Interface definition (180 lines)
├── src/
│   └── TallyWebSocketImpl.cpp # libwebsockets implementation (369 lines)
├── demo/
│   └── demo.cpp               # Echo test demo (116 lines)
└── CMakeLists.txt             # Build configuration
```

---

## C++ Interface Design (Actual Implementation)

### IWebSocketClient (Abstract Interface)

```cpp
namespace Tally {

class IWebSocketClient {
public:
    virtual ~IWebSocketClient() = default;
    
    // Callback types
    using MessageCallback = std::function<void(const uint8_t* data, size_t length, MessageType type)>;
    using StateCallback = std::function<void(ConnectionState newState, const WebSocketError* error)>;
    
    // Connection lifecycle
    virtual bool Connect(const WebSocketConfig& config) = 0;
    virtual void Disconnect(int code = 1000, const std::string& reason = "") = 0;
    virtual bool IsConnected() const = 0;
    virtual ConnectionState GetState() const = 0;
    
    // Messaging
    virtual bool SendText(const std::string& message) = 0;
    virtual bool SendBinary(const uint8_t* data, size_t length) = 0;
    
    // Callbacks
    virtual void SetMessageCallback(MessageCallback callback) = 0;
    virtual void SetStateCallback(StateCallback callback) = 0;
    
    // Event loop
    virtual int Poll(int timeoutMs = 0) = 0;
};

// Factory function
std::unique_ptr<IWebSocketClient> CreateWebSocketClient();
std::string GetWebSocketLibraryVersion();

} // namespace Tally
```

### Connection States

```cpp
enum class ConnectionState {
    Disconnected,   // Not connected
    Connecting,     // Connection in progress
    Connected,      // Connected and ready
    Disconnecting,  // Disconnect in progress
    Error           // Error state
};

enum class MessageType {
    Text,           // UTF-8 text message
    Binary          // Binary data message
};
```

### WebSocketConfig

```cpp
struct WebSocketConfig {
    std::string url;                    // ws:// or wss://
    std::string subprotocol;            // Optional
    int connectTimeoutMs = 30000;
    int pingIntervalMs = 30000;
    bool autoReconnect = false;
    int reconnectDelayMs = 5000;
    int maxReconnectAttempts = 5;
};
```

---

## Implementation Phases

### Phase 1: Repository Setup ✅
- Fork libwebsockets to GitHub (`RahuIMallya272727`)
- Clone repository to `d:\ALL_BINS\TallylibwebsocketsPOC`
- Add upstream remote
- Review repository structure

### Phase 2: Abstraction Layer Design ✅
- Created `TallyWebSocket.h` interface (180 lines)
- Callback-based API with `std::function`
- Thread-safe message queue in `LwsWebSocketClient`

### Phase 3: Windows Build Configuration ✅
- CMake configuration for Tally
- Working build (No TLS - suitable for PoC)

**Current Build Command (No TLS):**
```batch
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
    -DLWS_WITH_SSL=OFF ^
    -DLWS_ROLE_WS=ON ^
    -DLWS_WITHOUT_SERVER=ON ^
    -DLWS_WITH_SECURE_STREAMS=ON ^
    -DLWS_WITHOUT_TESTAPPS=ON ^
    -DLWS_WITHOUT_EXTENSIONS=ON ^
    -DLWS_WITH_SHARED=ON ^
    -DLWS_WITH_STATIC=ON ^
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --parallel
```

**Production Build Command (With OpenSSL TLS):**
```batch
set TALLY_XLIBS=D:\work\code\tally\sa_infra\xlibs
set OPENSSL_SRC=%TALLY_XLIBS%\openssl\src
set SSL_LIB=%TALLY_XLIBS%\openssl\lib\bin\release64\libssl.lib
set CRYPTO_LIB=%TALLY_XLIBS%\openssl\lib\bin\release64\libcrypto.lib

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DLWS_WITH_STATIC=ON -DLWS_WITH_SHARED=OFF ^
    -DLWS_WITH_SSL=ON -DLWS_WITH_SCHANNEL=OFF ^
    -DLWS_WITH_ZLIB=OFF ^
    -DLWS_WITHOUT_EXTENSIONS=ON ^
    -DLWS_WITH_SECURE_STREAMS=ON ^
    -DLWS_WITHOUT_TESTAPPS=ON ^
    -DLWS_WITH_MINIMAL_EXAMPLES=OFF ^
    -DLWS_WITHOUT_SERVER=OFF ^
    -DOPENSSL_ROOT_DIR="%OPENSSL_SRC%" ^
    -DOPENSSL_USE_STATIC_LIBS=TRUE ^
    -DOPENSSL_SSL_LIBRARY="%SSL_LIB%" ^
    -DOPENSSL_CRYPTO_LIBRARY="%CRYPTO_LIB%" ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

cmake --build build --config Release --parallel
```

### Phase 4: Implementation ✅
- `TallyWebSocketImpl.cpp` (369 lines) complete
- Uses libwebsockets low-level API (`lws_context`, `lws_client_connect_via_info`)
- Thread-safe send queue with `std::mutex`
- Proper `LWS_PRE` buffer handling

### Phase 5: Demo Application ✅
- `demo/demo.cpp` (116 lines) complete
- Connects to echo server
- Tests send/receive functionality

### Phase 6: Build tally-ws-wrapper ⏳ Pending
- Build the wrapper library
- Link against `websockets_static.lib`
- Test demo executable

---

## TLS Status

### SChannel (Windows Native)
❌ **FAILS** - MSVC 19.35 syntax error in `LWS_WITH_SCHANNEL` option

### OpenSSL
✅ **Recommended** - Use OpenSSL via `LWS_WITH_SSL=ON` with Tally's OpenSSL 1.1.1

---

## Current Build Analysis

**From `build/CMakeCache.txt`:**

| Option | Current Value | Recommended |
|--------|---------------|-------------|
| `LWS_WITH_SSL` | OFF | ON (for production) |
| `LWS_WITHOUT_SERVER` | ON | OFF (if Tally sync needed) |
| `LWS_WITH_SHARED` | ON | OFF (use static only) |
| `LWS_WITH_STATIC` | ON | ✅ Correct |
| `LWS_WITH_SECURE_STREAMS` | ON | ✅ Correct |
| `LWS_WITHOUT_EXTENSIONS` | ON | ✅ Correct |

---

## Key CMake Options for Production TLS

```cmake
-DLWS_WITH_SSL=ON                    # Enable TLS
-DLWS_WITH_SECURE_STREAMS=ON         # Modern high-level API  
-DLWS_WITHOUT_EXTENSIONS=ON          # Disable permessage-deflate (simpler)
-DLWS_WITH_SCHANNEL=OFF              # Avoid - has bugs
```

---

## Recommended Approach: Use Secure Streams API

The Secure Streams (SS) API in libwebsockets is modern, higher-level interface:

- **Connection policy in JSON** (endpoints, TLS, retry logic)
- **User code handles only payloads** (`tx/rx/state` callbacks)
- **Automatic retry/backoff**
- **Cleaner than low-level `lws_context` API**

---

## Implementation Details (Verified from Source)

### LwsWebSocketClient Key Features

1. **Thread-safe send queue:**
   ```cpp
   std::queue<QueuedMessage> m_sendQueue;
   std::mutex m_queueMutex;
   ```

2. **Proper LWS_PRE buffer handling:**
   ```cpp
   msg.data.resize(LWS_PRE + message.size());
   memcpy(msg.data.data() + LWS_PRE, message.data(), message.size());
   ```

3. **URL parsing with TLS detection:**
   ```cpp
   if (strcmp(prot, "wss") == 0 || strcmp(prot, "https") == 0) {
       ccinfo.ssl_connection = LCCSCF_USE_SSL;
   }
   ```

4. **Callback handling:**
   - `LWS_CALLBACK_CLIENT_ESTABLISHED` → Connected
   - `LWS_CALLBACK_CLIENT_RECEIVE` → Message received
   - `LWS_CALLBACK_CLIENT_WRITEABLE` → Process send queue
   - `LWS_CALLBACK_CLIENT_CONNECTION_ERROR` → Error state
   - `LWS_CALLBACK_CLIENT_CLOSED` → Disconnected

---

## Next Steps

1. ⏳ **Build tally-ws-wrapper** - Run CMake in `tally-ws-wrapper/` directory
2. ⏳ **Test demo app** - Run `TallyWebSocketDemo.exe` against echo server
3. ⏳ **Enable TLS** - Rebuild with OpenSSL for `wss://` support
4. ⏳ **Integration** - Integrate wrapper into Tally codebase

---

## Files to Verify/Create

```
✅ websockets_static.lib      (build/lib/Release/)
✅ TallyWebSocket.h           (tally-ws-wrapper/include/)
✅ TallyWebSocketImpl.cpp     (tally-ws-wrapper/src/)
✅ demo.cpp                   (tally-ws-wrapper/demo/)
✅ CMakeLists.txt             (tally-ws-wrapper/)
⏳ TallyWebSocket.lib         (tally-ws-wrapper/build/ - not built yet)
⏳ TallyWebSocketDemo.exe     (tally-ws-wrapper/build/ - not built yet)
```
