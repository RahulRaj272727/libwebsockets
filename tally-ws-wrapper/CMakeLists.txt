# CMakeLists.txt for Tally WebSocket Wrapper
cmake_minimum_required(VERSION 3.10)
project(TallyWebSocket CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libwebsockets
# This assumes libwebsockets was built in the parent directory
set(LWS_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build")
set(LWS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(LWS_BUILD_INCLUDE_DIR "${LWS_BUILD_DIR}")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LWS_INCLUDE_DIR}
    ${LWS_BUILD_INCLUDE_DIR}
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

# Link against libwebsockets
if(WIN32)
    target_link_libraries(TallyWebSocket
        ${LWS_BUILD_DIR}/lib/Release/websockets_static.lib
        ws2_32
    )
else()
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
