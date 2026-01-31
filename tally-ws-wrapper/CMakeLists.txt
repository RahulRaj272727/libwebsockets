# CMakeLists.txt for Tally WebSocket Wrapper
cmake_minimum_required(VERSION 3.10)
project(TallyWebSocket CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Ensure we use static runtime to match libwebsockets
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Tally Dependencies Paths
set(TALLY_XLIBS "D:/work/code.tally/sa_infra/xlibs")
set(OPENSSL_ROOT_DIR "${TALLY_XLIBS}/openssl/install/release64")
set(ZLIB_ROOT "${TALLY_XLIBS}/zlib/bin/release64")

# Libwebsockets Paths (from our manual build)
set(LWS_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build_release")
set(LWS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LWS_INCLUDE_DIR}
    ${LWS_BUILD_DIR}/include
    "${OPENSSL_ROOT_DIR}/include"
)

# Library sources
set(TALLY_WS_SOURCES
    src/TallyWebSocketImpl.cpp
)

set(TALLY_WS_HEADERS
    include/TallyWebSocket.h
)

# Create static library
add_library(TallyWebSocket STATIC ${TALLY_WS_SOURCES} ${TALLY_WS_HEADERS})

# Link against libwebsockets and dependencies
if(WIN32)
    target_link_libraries(TallyWebSocket
        "${LWS_BUILD_DIR}/lib/Release/websockets_static.lib"
        "${OPENSSL_ROOT_DIR}/lib/libssl.lib"
        "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib"
        "${ZLIB_ROOT}/zlib.lib"
        ws2_32
        crypt32
        iphlpapi
    )
else()
    # Placeholder for non-Windows (not active)
    target_link_libraries(TallyWebSocket
        ${LWS_BUILD_DIR}/lib/libwebsockets.a
    )
endif()

# Demo executable
add_executable(TallyWebSocketDemo demo/demo.cpp)
target_link_libraries(TallyWebSocketDemo TallyWebSocket)

# Install targets
install(TARGETS TallyWebSocket
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${TALLY_WS_HEADERS} DESTINATION include/Tally)
