# Building libwebsockets for Tally

This document provides complete instructions for building libwebsockets as a static library for integration with Tally on Windows.

---

## Table of Contents

1. [Build Strategy](#build-strategy)
2. [Prerequisites](#prerequisites)
3. [Build Order](#build-order)
4. [CMake Options Reference](#cmake-options-reference)
5. [Recommended Configuration](#recommended-configuration)
6. [Build Commands](#build-commands)
7. [Common Mistakes](#common-mistakes)
8. [Verification](#verification)
9. [Troubleshooting](#troubleshooting)

---

## Build Strategy

### Locked Decisions

| Decision | Value | Rationale |
|----------|-------|-----------|
| OS | Windows | Tally target platform |
| Compiler | MSVC (Visual Studio 2022) | Tally standard toolchain |
| Linking | **Static** | Single binary deployment, no DLL dependencies |
| TLS | **OpenSSL** | Secure WebSocket (`wss://`) support required |
| Runtime | `/MT` (Release) / `/MTd` (Debug) | Static CRT, matches Tally |
| Build System | CMake | Source build, full control |

### Dependency Control

- **You own all binaries** - no package managers, no prebuilt DLLs
- **Build → Install → Consume** - never link against random build outputs
- **Same MSVC version and runtime** across all dependencies

---

## Prerequisites

### Required Software

| Software | Version | Purpose |
|----------|---------|---------|
| Visual Studio | 2022 (v17.x) | MSVC compiler |
| CMake | 3.10+ | Build system |
| Git | Any | Source control |

### Required Libraries

| Library | Location | Notes |
|---------|----------|-------|
| OpenSSL | Tally xlibs | Must be built with same MSVC version and `/MT`/`/MTd` runtime |

### OpenSSL Files Required

Before building libwebsockets, verify these files exist:

```
%TALLY_XLIBS%\openssl\lib\bin\debug64\libssl.lib
%TALLY_XLIBS%\openssl\lib\bin\debug64\libcrypto.lib
%TALLY_XLIBS%\openssl\lib\bin\release64\libssl.lib
%TALLY_XLIBS%\openssl\lib\bin\release64\libcrypto.lib
```

> ⚠️ **Critical:** If OpenSSL is built with wrong MSVC version or wrong runtime (`/MD` vs `/MT`), you will get linker errors or runtime crashes.

---

## Build Order

**Order matters.** libwebsockets depends on OpenSSL, not the reverse.

```
1. OpenSSL    ──────►  libssl.lib, libcrypto.lib
                              │
                              ▼
2. libwebsockets  ──────►  websockets_static.lib
                              │
                              ▼
3. Your Application  ──────►  Tally.exe
```

---

## CMake Options Reference

This section documents **all** libwebsockets CMake options for future reference.

### Core Network Features

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_NETWORK` | ON | ON | Network support - required |
| `LWS_ROLE_H1` | ON | ON | HTTP/1.1 support - required for WebSocket upgrade |
| `LWS_ROLE_WS` | ON | ON | WebSocket protocol support - required |
| `LWS_ROLE_MQTT` | OFF | OFF | MQTT client support |
| `LWS_ROLE_DBUS` | OFF | OFF | DBUS support (Linux) |
| `LWS_ROLE_RAW_PROXY` | OFF | OFF | Raw packet proxy |
| `LWS_ROLE_RAW_FILE` | ON | ON | Raw file support |
| `LWS_WITH_HTTP2` | ON | **OFF** | HTTP/2 support |
| `LWS_WITH_LWSWS` | OFF | OFF | Libwebsockets webserver |
| `LWS_WITH_CGI` | OFF | OFF | CGI spawn support |
| `LWS_IPV6` | ON | ON | IPv6 support |
| `LWS_UNIX_SOCK` | ON | ON | Unix domain socket (Windows: named pipes) |

**Why `LWS_WITH_HTTP2=OFF`:**
- Adds extra threads and protocol paths
- Makes TLS configuration more fragile
- Not needed for WebSocket client use case
- Reduces binary size and complexity

### Client/Server Control

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITHOUT_CLIENT` | OFF | **OFF** | Keep client support (we need this) |
| `LWS_WITHOUT_SERVER` | OFF | **ON** | Disable server support |

**Why `LWS_WITHOUT_SERVER=ON`:**
- Reduces attack surface
- Removes server-specific code paths
- Smaller binary
- Change to OFF if Tally needs to act as WebSocket server

### TLS Options

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_SSL` | ON | **ON** | Enable TLS - required for `wss://` |
| `LWS_WITH_MBEDTLS` | OFF | OFF | Use mbedTLS instead of OpenSSL |
| `LWS_WITH_SCHANNEL` | OFF | **OFF** | Windows SChannel TLS |
| `LWS_WITH_BORINGSSL` | OFF | OFF | Use BoringSSL |
| `LWS_WITH_GNUTLS` | OFF | OFF | Use GnuTLS |
| `LWS_WITH_CYASSL` | OFF | OFF | Use CyaSSL |
| `LWS_WITH_WOLFSSL` | OFF | OFF | Use wolfSSL |
| `LWS_SSL_CLIENT_USE_OS_CA_CERTS` | ON | ON | Use OS CA certificate store |
| `LWS_WITH_TLS_SESSIONS` | ON | ON | TLS session resumption |
| `LWS_WITH_TLS_JIT_TRUST` | OFF | OFF | Dynamic CA trust computation |
| `LWS_TLS_LOG_PLAINTEXT_RX` | OFF | OFF | Debug: log decrypted RX |
| `LWS_TLS_LOG_PLAINTEXT_TX` | OFF | OFF | Debug: log plaintext TX |

**Why `LWS_WITH_SCHANNEL=OFF`:**
- SChannel has compilation errors on MSVC 19.35+
- OpenSSL is more portable and better tested
- Tally already has OpenSSL dependency

### Extensions & Compression

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITHOUT_EXTENSIONS` | ON | **ON** | Disable WebSocket extensions |
| `LWS_WITH_ZLIB` | OFF | **OFF** | zlib compression |
| `LWS_WITH_BUNDLED_ZLIB` | ON (Win) | OFF | Use bundled zlib |
| `LWS_WITH_MINIZ` | OFF | OFF | Use miniz instead of zlib |
| `LWS_WITH_GZINFLATE` | ON | OFF | Internal gzip inflator |
| `LWS_WITH_HTTP_STREAM_COMPRESSION` | OFF | OFF | HTTP stream compression |
| `LWS_WITH_HTTP_BROTLI` | OFF | OFF | Brotli compression |

**Why Extensions and Compression OFF:**
- Per-message deflate increases memory fragmentation
- Adds complex state machines
- Rarely needed for internal protocols
- Simplifies debugging
- If compression is needed later, enable both `LWS_WITH_ZLIB=ON` and `LWS_WITHOUT_EXTENSIONS=OFF`

### Secure Streams API

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_SECURE_STREAMS` | ON | **OFF** | High-level Secure Streams API |
| `LWS_WITH_SECURE_STREAMS_CPP` | OFF | OFF | C++ Secure Streams classes |
| `LWS_WITH_SECURE_STREAMS_PROXY_API` | OFF | OFF | Cross-process Secure Streams |
| `LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY` | OFF | OFF | Hardcoded policy only |
| `LWS_WITH_SECURE_STREAMS_AUTH_SIGV4` | OFF | OFF | AWS Sigv4 auth |
| `LWS_WITH_SS_DIRECT_PROTOCOL_STR` | OFF | OFF | Direct metadata access |
| `LWS_ONLY_SSPC` | OFF | OFF | SSPC client only |

**Why `LWS_WITH_SECURE_STREAMS=OFF`:**
- Current Tally wrapper uses low-level `lws_context` API
- Secure Streams adds JSON policy parsing overhead
- Can be enabled later if migrating to Secure Streams API

### System Features

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_SYS_STATE` | ON | OFF | System state machine |
| `LWS_WITH_SYS_SMD` | ON | OFF | System message distribution |
| `LWS_WITH_SYS_ASYNC_DNS` | OFF | OFF | Async DNS resolver |
| `LWS_WITH_SYS_NTPCLIENT` | OFF | OFF | Built-in NTP client |
| `LWS_WITH_SYS_DHCP_CLIENT` | OFF | OFF | Built-in DHCP client |
| `LWS_WITH_SYS_FAULT_INJECTION` | OFF | OFF | Fault injection testing |
| `LWS_WITH_SYS_METRICS` | OFF | OFF | Metrics API |

### Build Output Control

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `BUILD_SHARED_LIBS` | - | **OFF** | CMake shared libs default |
| `LWS_WITH_STATIC` | ON | **ON** | Build static library |
| `LWS_WITH_SHARED` | ON | **OFF** | Build shared library (DLL) |
| `LWS_LINK_TESTAPPS_DYNAMIC` | OFF | OFF | Link test apps to DLL |
| `LWS_STATIC_PIC` | OFF | OFF | Position-independent static lib |

**Why Static-Only:**
- Single binary deployment
- No DLL versioning issues
- Symbols are yours to debug
- Simpler distribution

### Test & Example Control

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITHOUT_TESTAPPS` | OFF | **ON** | Skip test applications |
| `LWS_WITHOUT_TEST_SERVER` | OFF | ON | Skip test server |
| `LWS_WITHOUT_TEST_SERVER_EXTPOLL` | OFF | ON | Skip extpoll test server |
| `LWS_WITHOUT_TEST_PING` | OFF | ON | Skip ping test |
| `LWS_WITHOUT_TEST_CLIENT` | OFF | ON | Skip test client |
| `LWS_WITH_MINIMAL_EXAMPLES` | ON | **OFF** | Skip minimal examples |

**Why Test Apps OFF:**
- Pulls extra code paths into build
- Can silently enable features
- Bloats static binary
- Slows link time
- You're not shipping examples

### Logging & Debug

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_NO_LOGS` | OFF | OFF | Disable all logging |
| `LWS_LOGS_TIMESTAMP` | ON | ON | Timestamp in logs |
| `LWS_LOG_TAG_LIFECYCLE` | ON | OFF (Release) | Log object lifecycle |
| `LWS_WITH_ASAN` | OFF | OFF | Address sanitizer |
| `LWS_WITH_GCOV` | OFF | OFF | Code coverage |
| `LWS_WITH_FANALYZER` | OFF | OFF | GCC static analyzer |

### Event Loop Options

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_LIBEV` | OFF | OFF | libev integration |
| `LWS_WITH_LIBUV` | OFF | OFF | libuv integration |
| `LWS_WITH_LIBEVENT` | OFF | OFF | libevent integration |
| `LWS_WITH_GLIB` | OFF | OFF | GLib event loop |
| `LWS_WITH_SDEVENT` | OFF | OFF | systemd event loop |
| `LWS_WITH_ULOOP` | OFF | OFF | OpenWrt uloop |
| `LWS_WITH_EVLIB_PLUGINS` | OFF (Win) | OFF | Event lib plugins |

**Tally uses default poll-based event loop** - no external event library dependencies.

### Parsers & Utilities

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_LEJP` | ON | ON | Lightweight JSON Parser |
| `LWS_WITH_CBOR` | OFF | OFF | CBOR parser |
| `LWS_WITH_LHP` | ON | OFF | Lightweight HTML5 parser |
| `LWS_WITH_SQLITE3` | OFF | OFF | SQLite3 support |
| `LWS_WITH_STRUCT_JSON` | OFF | OFF | Struct to JSON serialization |
| `LWS_WITH_STRUCT_SQLITE3` | OFF | OFF | Struct to SQLite3 |
| `LWS_WITH_JSONRPC` | ON | OFF | JSON RPC support |
| `LWS_WITH_LWSAC` | ON | ON | Chunk allocation API |
| `LWS_WITH_DIR` | ON | ON | Directory scanning |
| `LWS_WITH_FILE_OPS` | ON | ON | File operations VFS |

### Graphics & Display (Not Used)

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_UPNG` | ON | OFF | PNG decoder |
| `LWS_WITH_JPEG` | ON | OFF | JPEG decoder |
| `LWS_WITH_DLO` | ON | OFF | Display List Objects |
| `LWS_WITH_DRIVERS` | OFF | OFF | GPIO/I2C drivers |

### Authentication

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_HTTP_BASIC_AUTH` | ON | ON | HTTP Basic Auth |
| `LWS_WITH_HTTP_DIGEST_AUTH` | ON | OFF | HTTP Digest Auth (deprecated crypto) |
| `LWS_WITH_ACME` | OFF | OFF | Let's Encrypt ACME |
| `LWS_WITH_JOSE` | OFF | OFF | JSON Web Encryption |
| `LWS_WITH_COSE` | OFF | OFF | CBOR encryption |
| `LWS_WITH_GENCRYPTO` | OFF | OFF | Generic crypto APIs |

### Networking Extras

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITH_HTTP_PROXY` | OFF | OFF | Active HTTP proxying |
| `LWS_WITH_SOCKS5` | OFF | OFF | SOCKS5 proxy support |
| `LWS_WITH_PEER_LIMITS` | OFF | OFF | Per-peer resource limits |
| `LWS_WITH_ACCESS_LOG` | OFF | OFF | Apache-style access logs |
| `LWS_WITH_RANGES` | OFF | OFF | HTTP ranges (RFC7233) |
| `LWS_WITH_ZIP_FOPS` | OFF | OFF | Serve pre-zipped files |
| `LWS_CLIENT_HTTP_PROXYING` | ON | ON | HTTP proxy for clients |
| `LWS_WITH_UDP` | ON | ON | UDP support |
| `LWS_WITH_CONMON` | ON | OFF | Connection monitoring |
| `LWS_WITH_WOL` | ON | OFF | Wake-on-LAN |

### Miscellaneous

| Option | Default | Tally Value | Description |
|--------|---------|-------------|-------------|
| `LWS_WITHOUT_DAEMONIZE` | ON | ON | No daemon API (Windows) |
| `LWS_WITHOUT_BUILTIN_GETIFADDRS` | OFF | OFF | Use libc getifaddrs |
| `LWS_WITHOUT_BUILTIN_SHA1` | OFF | OFF | Use OpenSSL SHA1 |
| `LWS_FALLBACK_GETHOSTBYNAME` | OFF | OFF | Fallback DNS resolution |
| `LWS_WITHOUT_EVENTFD` | OFF | OFF | Use pipe instead of eventfd |
| `LWS_WITH_EXTERNAL_POLL` | OFF | OFF | External poll integration |
| `LWS_WITH_CUSTOM_HEADERS` | ON | ON | Custom HTTP headers |
| `LWS_WITH_HTTP_UNCOMMON_HEADERS` | ON | ON | Less common headers |
| `LWS_REPRODUCIBLE` | ON | ON | Reproducible builds |
| `LWS_WITH_EXPORT_LWSTARGETS` | ON | ON | Export CMake targets |
| `LWS_SUPPRESS_DEPRECATED_API_WARNINGS` | ON | ON | Suppress OpenSSL 3 warnings |
| `LWS_WITH_SELFTESTS` | OFF | OFF | Run selftests at startup |
| `LWS_WITH_THREADPOOL` | OFF | OFF | Worker thread pool |
| `LWS_WITH_SPAWN` | OFF | OFF | Subprocess spawning |
| `LWS_WITH_FSMOUNT` | OFF | OFF | Filesystem mounting |
| `LWS_WITH_FTS` | OFF | OFF | Full-text search |
| `LWS_WITH_DISKCACHE` | OFF | OFF | Disk cache |
| `LWS_WITH_LWS_DSH` | OFF | OFF | Disordered shared heap |
| `LWS_HTTP_HEADERS_ALL` | OFF | OFF | All HTTP headers |
| `LWS_WITH_SUL_DEBUGGING` | OFF | OFF | Sul zombie checking |
| `LWS_WITH_PLUGINS` | OFF | OFF | Protocol plugins |
| `LWS_WITH_PLUGINS_API` | OFF | OFF | Plugin API |
| `LWS_WITH_PLUGINS_BUILTIN` | OFF | OFF | Built-in plugins |
| `LWS_WITH_CACHE_NSCOOKIEJAR` | ON | OFF | Cookie jar cache |

---

## Recommended Configuration

### Environment Variables

```batch
set TALLY_XLIBS=D:\work\code\tally\sa_infra\xlibs
set LWS_SRC=%TALLY_XLIBS%\libwebsockets\src
set OPENSSL_SRC=%TALLY_XLIBS%\openssl\src
```

### Minimal Flags (Explicitly Set)

These are the flags we explicitly set. All others use defaults from the reference above.

```cmake
# Build type
BUILD_SHARED_LIBS=OFF
LWS_WITH_STATIC=ON
LWS_WITH_SHARED=OFF

# TLS
LWS_WITH_SSL=ON
LWS_WITH_SCHANNEL=OFF

# Features OFF for stability
LWS_WITH_HTTP2=OFF
LWS_WITH_ZLIB=OFF
LWS_WITHOUT_EXTENSIONS=ON
LWS_WITH_SECURE_STREAMS=OFF
LWS_WITHOUT_SERVER=ON

# Test apps OFF
LWS_WITHOUT_TESTAPPS=ON
LWS_WITH_MINIMAL_EXAMPLES=OFF

# OpenSSL paths
OPENSSL_ROOT_DIR=<path>
OPENSSL_USE_STATIC_LIBS=TRUE
OPENSSL_SSL_LIBRARY=<path>/libssl.lib
OPENSSL_CRYPTO_LIBRARY=<path>/libcrypto.lib

# MSVC runtime
CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded[Debug]
```

---

## Build Commands

### Debug 64-bit

```batch
set TALLY_XLIBS=D:\work\code\tally\sa_infra\xlibs
set OPENSSL_SRC=%TALLY_XLIBS%\openssl\src
set SSL_LIB=%TALLY_XLIBS%\openssl\lib\bin\debug64\libssl.lib
set CRYPTO_LIB=%TALLY_XLIBS%\openssl\lib\bin\debug64\libcrypto.lib

cmake -S . -B build\Debug -G "Visual Studio 17 2022" -A x64 ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DLWS_WITH_STATIC=ON ^
    -DLWS_WITH_SHARED=OFF ^
    -DLWS_WITH_SSL=ON ^
    -DLWS_WITH_SCHANNEL=OFF ^
    -DLWS_WITH_HTTP2=OFF ^
    -DLWS_WITH_ZLIB=OFF ^
    -DLWS_WITHOUT_EXTENSIONS=ON ^
    -DLWS_WITH_SECURE_STREAMS=OFF ^
    -DLWS_WITH_SYS_STATE=OFF ^
    -DLWS_WITHOUT_TESTAPPS=ON ^
    -DLWS_WITH_MINIMAL_EXAMPLES=OFF ^
    -DLWS_WITHOUT_SERVER=ON ^
    -DOPENSSL_ROOT_DIR="%OPENSSL_SRC%" ^
    -DOPENSSL_USE_STATIC_LIBS=TRUE ^
    -DOPENSSL_SSL_LIBRARY="%SSL_LIB%" ^
    -DOPENSSL_CRYPTO_LIBRARY="%CRYPTO_LIB%" ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug ^
    -DCMAKE_C_FLAGS_DEBUG="/MTd /Z7"

cmake --build build\Debug --config Debug --parallel
```

### Release 64-bit

```batch
set TALLY_XLIBS=D:\work\code\tally\sa_infra\xlibs
set OPENSSL_SRC=%TALLY_XLIBS%\openssl\src
set SSL_LIB=%TALLY_XLIBS%\openssl\lib\bin\release64\libssl.lib
set CRYPTO_LIB=%TALLY_XLIBS%\openssl\lib\bin\release64\libcrypto.lib

cmake -S . -B build\Release -G "Visual Studio 17 2022" -A x64 ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DLWS_WITH_STATIC=ON ^
    -DLWS_WITH_SHARED=OFF ^
    -DLWS_WITH_SSL=ON ^
    -DLWS_WITH_SCHANNEL=OFF ^
    -DLWS_WITH_HTTP2=OFF ^
    -DLWS_WITH_ZLIB=OFF ^
    -DLWS_WITHOUT_EXTENSIONS=ON ^
    -DLWS_WITH_SECURE_STREAMS=OFF ^
    -DLWS_WITH_SYS_STATE=OFF ^
    -DLWS_WITHOUT_TESTAPPS=ON ^
    -DLWS_WITH_MINIMAL_EXAMPLES=OFF ^
    -DLWS_WITHOUT_SERVER=ON ^
    -DOPENSSL_ROOT_DIR="%OPENSSL_SRC%" ^
    -DOPENSSL_USE_STATIC_LIBS=TRUE ^
    -DOPENSSL_SSL_LIBRARY="%SSL_LIB%" ^
    -DOPENSSL_CRYPTO_LIBRARY="%CRYPTO_LIB%" ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

cmake --build build\Release --config Release --parallel
```

---

## Common Mistakes

### 1. Wrong Runtime Library

| Symptom | Cause | Fix |
|---------|-------|-----|
| `LNK2038: mismatch detected for 'RuntimeLibrary'` | OpenSSL built with `/MD`, lws with `/MT` | Rebuild OpenSSL with `/MT` or `/MTd` |
| Random crashes at runtime | Mixed CRT versions | Ensure all libs use same runtime |

**Prevention:** Always verify `CMAKE_MSVC_RUNTIME_LIBRARY` matches across all dependencies.

### 2. Wrong OpenSSL Architecture

| Symptom | Cause | Fix |
|---------|-------|-----|
| `LNK2019: unresolved external symbol` | x86 OpenSSL with x64 build | Use matching architecture |
| CMake can't find OpenSSL | Wrong `OPENSSL_ROOT_DIR` | Verify path exists |

### 3. Extensions Without Zlib

| Symptom | Cause | Fix |
|---------|-------|-----|
| Linker errors for `deflate`, `inflate` | `LWS_WITHOUT_EXTENSIONS=OFF` but `LWS_WITH_ZLIB=OFF` | Either enable zlib or keep extensions off |

**Rule:** `LWS_WITHOUT_EXTENSIONS` and `LWS_WITH_ZLIB` must be consistent.

### 4. Missing Server with Server-Side Code

| Symptom | Cause | Fix |
|---------|-------|-----|
| Undefined symbol `lws_create_context` server functions | `LWS_WITHOUT_SERVER=ON` but code uses server APIs | Set `LWS_WITHOUT_SERVER=OFF` |

### 5. Secure Streams Dependencies

| Symptom | Cause | Fix |
|---------|-------|-----|
| Missing `ss_*` symbols | `LWS_WITH_SECURE_STREAMS=OFF` but using SS API | Enable Secure Streams or use low-level API |
| Missing `lws_system_*` | `LWS_WITH_SYS_STATE=OFF` but SS enabled | Enable `LWS_WITH_SYS_STATE` with SS |

**Rule:** Secure Streams requires `LWS_WITH_SYS_STATE=ON`.

### 6. HTTP/2 Without Need

| Symptom | Cause | Fix |
|---------|-------|-----|
| Larger binary than expected | `LWS_WITH_HTTP2=ON` (default) | Set `LWS_WITH_HTTP2=OFF` |
| Extra complexity | HTTP/2 code paths | Disable unless specifically needed |

### 7. SChannel Compilation Errors

| Symptom | Cause | Fix |
|---------|-------|-----|
| Syntax errors in SChannel code | MSVC 19.35+ incompatibility | Use `LWS_WITH_SCHANNEL=OFF`, use OpenSSL |

### 8. Test Apps Bloating Binary

| Symptom | Cause | Fix |
|---------|-------|-----|
| Build takes too long | Building all test apps | `LWS_WITHOUT_TESTAPPS=ON` |
| Unexpected symbols in lib | Examples pulling features | `LWS_WITH_MINIMAL_EXAMPLES=OFF` |

---

## Verification

### Expected Output Files

```
build\Release\lib\
└── websockets_static.lib   (~1-1.5 MB)

build\Debug\lib\
└── websockets_static.lib   (~3-5 MB)
```

### Verify Static Linking

```batch
dumpbin /headers build\Release\lib\websockets_static.lib | findstr "machine"
```

Expected: `machine (x64)`

### Verify No DLL Dependencies

The static library should not require any libwebsockets DLLs. Your final application will link:

```
your_app.exe
├── your code
├── websockets_static.lib (linked)
├── libssl.lib (linked)
├── libcrypto.lib (linked)
└── MSVC CRT (static, linked)
```

---

## Troubleshooting

### CMake Cannot Find OpenSSL

```
Could NOT find OpenSSL, try to set the path to OpenSSL root folder
```

**Fix:** Set all three paths:
```batch
-DOPENSSL_ROOT_DIR="%OPENSSL_SRC%"
-DOPENSSL_SSL_LIBRARY="%SSL_LIB%"
-DOPENSSL_CRYPTO_LIBRARY="%CRYPTO_LIB%"
```

### Linker Error: Unresolved External SSL Functions

```
error LNK2019: unresolved external symbol SSL_CTX_new
```

**Fix:** Ensure `LWS_WITH_SSL=ON` and OpenSSL library paths are correct.

### Runtime Error: CRT Mismatch

**Fix:** Rebuild everything with consistent `/MT` or `/MTd`:
1. OpenSSL
2. libwebsockets  
3. Your application

### Build Succeeds But TLS Fails at Runtime

**Possible causes:**
1. OpenSSL DLLs in PATH conflicting with static libs
2. Missing CA certificates
3. Wrong OpenSSL version

**Fix:** Remove OpenSSL DLLs from PATH, verify `LWS_SSL_CLIENT_USE_OS_CA_CERTS=ON`.

---

## Quick Reference Card

```batch
# Minimal build command (adjust paths)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DLWS_WITH_STATIC=ON -DLWS_WITH_SHARED=OFF ^
    -DLWS_WITH_SSL=ON -DLWS_WITH_SCHANNEL=OFF ^
    -DLWS_WITH_HTTP2=OFF -DLWS_WITH_ZLIB=OFF ^
    -DLWS_WITHOUT_EXTENSIONS=ON ^
    -DLWS_WITH_SECURE_STREAMS=OFF ^
    -DLWS_WITHOUT_SERVER=ON ^
    -DLWS_WITHOUT_TESTAPPS=ON ^
    -DLWS_WITH_MINIMAL_EXAMPLES=OFF ^
    -DOPENSSL_ROOT_DIR="<openssl_path>" ^
    -DOPENSSL_USE_STATIC_LIBS=TRUE ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

cmake --build build --config Release --parallel
```

**Output:** `build\lib\Release\websockets_static.lib`
