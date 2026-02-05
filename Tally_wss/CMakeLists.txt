cmake_minimum_required(VERSION 3.10)
project(TallyWSS_Test CXX)

# ============================================================================
# CONFIGURATION - Adjust these paths for your environment
# ============================================================================

# Path to locally built libwebsockets (contains include/ and lib/ subdirs)
set(LWS_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build")

# Path to OpenSSL installation (required for TLS/WSS support)
# Download from: https://slproweb.com/products/Win32OpenSSL.html
# Or use: D:/ALL_BINS/openssl-1.1.1d
set(OPENSSL_DIR "D:/ALL_BINS/openssl-1.1.1d")

# Add executable
add_executable(tally_wss_test main.cpp wss.cpp)

# Include directories
target_include_directories(tally_wss_test PRIVATE
    "${LWS_BUILD_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${OPENSSL_DIR}/include"
)

# Link libraries
target_link_libraries(tally_wss_test PRIVATE
    "${LWS_BUILD_DIR}/lib/Release/websockets_static.lib"
    "${OPENSSL_DIR}/libssl.lib"
    "${OPENSSL_DIR}/libcrypto.lib"
    ws2_32
    crypt32
    iphlpapi
)

# Use /MT runtime to match the library
set_property(TARGET tally_wss_test PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
