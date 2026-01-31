# Tally WebSocket Integration: POC vs Production

## Overview
This document details the Proof of Concept (POC) for integrating `libwebsockets` (a C library) into the Tally C++ Codebase, and outlines the steps required to turn this into a production-grade feature.

## 1. The POC: What we achieved
We have successfully linked the static version of `libwebsockets` into `tally.exe`.

### Key Challenges Solved
1.  **Macro Collisions**: Tally and Windows headers define macros (e.g., `ERROR`, `min`, `max`, `DELETE`, `GET`) that clash with `libwebsockets` enums.
    *   *Solution*: Created `wss.h` (wrapper) which uses `#undef` and `#pragma push_macro` to isolate the library.
2.  **Precompiled Headers (PCH)**: `connector.h` must be the first include.
    *   *Solution*: Structured [wss.cpp](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.cpp) to respect PCH order while maintaining macro safety.
3.  **Build System (Maya)**: Configured Maya to link [websockets_static.lib](file:///d:/ALL_BINS/TallylibwebsocketsPOC/build_release/lib/Release/websockets_static.lib) and `openssl`/`zlib` dependencies.

### POC Implementation ([wss.cpp](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.cpp))
*   **Single File**: All logic is currently in `src/serverconnector/wss/wss.cpp` for easy verification.
*   **Blocking Loop**: The test function [TestWebSocketLifecycle()](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.cpp#153-222) runs a `while` loop that blocks the calling thread for ~5 seconds.
*   **File Logging**: writes debug info to `C:\tally_ws_debug.txt`.

---

## 2. Production Roadmap: What needs to be done

### Architecture
*   [ ] **Dedicated Thread**: The `lws_service()` loop MUST run in its own `std::thread` (e.g., `NetworkThread`). It cannot start on the UI thread or Main Tally thread, or it will freeze the application.
*   [ ] **Thread-Safe Queue**: Tally Logic threads should not call `lws_write` directly.
    *   *Design*: specific [Send(msg)](file:///d:/ALL_BINS/TallylibwebsocketsPOC/tally-ws-wrapper/src/TallyWebSocketImpl.cpp#200-221) function should push to a `ConcurrentQueue`.
    *   *Design*: The `LWS_CALLBACK_CLIENT_WRITEABLE` event should pop from this queue and send.
*   [ ] **Encapsulation**: Move the global functions in [wss.cpp](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.cpp) into a proper C++ class (e.g., `class TallyWSClient`).

### Reliability
*   [ ] **Reconnection Logic**: Implement exponential backoff (retry after 1s, 2s, 4s...) if the connection drops (`LWS_CALLBACK_CLIENT_CONNECTION_ERROR`).
*   [ ] **Heartbeats**: Configure `libwebsockets` PING/PONG intervals to detect dead connections.
*   [ ] **SSL Verification**: Currently, the POC ignores SSL certs or uses defaults. Production must specify the CA Bundle path (`info.client_ssl_ca_filepath`) to prevent Man-in-the-Middle attacks.

### Code Quality
*   [ ] **Logging**: Replace `fprintf` with Tally's internal Logging macros (`LogInfo`, `LogError`).
*   [ ] **Memory Management**: Ensure `lws_context_destroy` is called exactly once on application shutdown to prevent memory leaks.

## 3. How to Use (Current)
Call the function [TestWebSocketLifecycle()](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.cpp#153-222) from anywhere in the codebase.
```cpp
#include "serverconnector/wss/wss.h"

void TallyStartup() {
    // ...
    TestWebSocketLifecycle(); // Will block for 5 seconds for testing
}
```

## 4. Troubleshooting
*   **Syntax Errors (C2059)**: Usually caused by a new Windows macro colliding with libwebsockets. Add the colliding word to the `#undef` list in `wss.h`.
*   **Linker Errors**: Ensure [websockets_static.lib](file:///d:/ALL_BINS/TallylibwebsocketsPOC/build_release/lib/Release/websockets_static.lib), `libssl.lib`, `libcrypto.lib`, `crypt32.lib` are all listed in `.maya` file.

## 5. Artifacts
*   **Production Module Code**: Committed to `d:\ALL_BINS\TallylibwebsocketsPOC`
*   **Commit Hash**: `2bb2f3c249e62cfe230b86d5639a02382761e0c0`
*   **Files**: [Tally_wss/wss.hpp](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.hpp), [Tally_wss/wss.cpp](file:///d:/ALL_BINS/TallylibwebsocketsPOC/Tally_wss/wss.cpp)
