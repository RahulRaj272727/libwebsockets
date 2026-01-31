# TallyWebSocket Test Suite

This directory contains comprehensive tests for the TallyWebSocket library.

## Test Files

### test_tally_websocket.cpp
Comprehensive unit tests covering:
- Factory functions and client creation
- Initial state verification
- Connection configuration and validation
- State management and callbacks
- Disconnect behavior
- Message sending (text and binary) with disconnected state
- Callback management and thread safety
- Poll functionality
- Edge cases and robustness
- URL parsing
- Configuration validation
- Boundary conditions
- Negative test cases
- Regression tests

**Coverage:** 60+ test cases covering all major API surface areas and edge cases.

### test_websocket_integration.cpp
Integration tests for real connection scenarios:
- Actual WebSocket server connections
- Send and receive text messages
- Send and receive binary messages
- Multiple message handling
- Large message transmission
- Graceful disconnect
- Error handling with unreachable servers
- Rapid message stress testing
- UTF-8 and special character support

**Note:** Integration tests are disabled by default. Enable with `RUN_INTEGRATION_TESTS=1` environment variable.

## Building the Tests

### Using CMake (Recommended)

```bash
cd tally-ws-wrapper
mkdir build
cd build

# Configure with tests enabled (default)
cmake ..

# Build
cmake --build .

# Run all tests
ctest --output-on-failure

# Or use the convenience target
cmake --build . --target check
```

### Building Specific Test Executables

```bash
# Build only unit tests
cmake --build . --target TallyWebSocketTests

# Build only integration tests
cmake --build . --target TallyWebSocketIntegrationTests
```

### Disabling Tests

To build without tests:

```bash
cmake .. -DBUILD_TESTS=OFF
```

## Running the Tests

### Unit Tests
```bash
# Run from build directory
./TallyWebSocketTests

# Run with gtest filters
./TallyWebSocketTests --gtest_filter="TallyWebSocketTest.CreateWebSocketClient*"
```

### Integration Tests

Integration tests require a WebSocket echo server to be running. By default, integration tests are **skipped** unless explicitly enabled.

#### Option 1: Use a Local Echo Server

1. Start a WebSocket echo server on `ws://localhost:8080`
2. Enable integration tests:
```bash
export RUN_INTEGRATION_TESTS=1
./TallyWebSocketIntegrationTests
```

#### Option 2: Use a Custom Server URL

```bash
export RUN_INTEGRATION_TESTS=1
export ECHO_SERVER_URL="ws://your-server:port"
./TallyWebSocketIntegrationTests
```

#### Option 3: Use Docker Echo Server

```bash
# Run a simple echo server in Docker
docker run -d -p 8080:8080 jmalloc/echo-server

# Run integration tests
export RUN_INTEGRATION_TESTS=1
./TallyWebSocketIntegrationTests
```

## Test Coverage

### API Coverage
- ✅ `CreateWebSocketClient()` - Factory function
- ✅ `GetWebSocketLibraryVersion()` - Version retrieval
- ✅ `IWebSocketClient::Connect()` - Connection initiation
- ✅ `IWebSocketClient::Disconnect()` - Disconnection
- ✅ `IWebSocketClient::IsConnected()` - Connection state check
- ✅ `IWebSocketClient::GetState()` - State retrieval
- ✅ `IWebSocketClient::SendText()` - Text message sending
- ✅ `IWebSocketClient::SendBinary()` - Binary message sending
- ✅ `IWebSocketClient::SetMessageCallback()` - Message callback registration
- ✅ `IWebSocketClient::SetStateCallback()` - State callback registration
- ✅ `IWebSocketClient::Poll()` - Event processing

### Scenario Coverage
- ✅ Normal connection lifecycle
- ✅ Connection failures and error handling
- ✅ Invalid URL handling
- ✅ Multiple connect attempts
- ✅ Graceful disconnect
- ✅ Sending when disconnected
- ✅ Callback management
- ✅ Thread safety (basic)
- ✅ Edge cases (empty messages, null data, etc.)
- ✅ Boundary conditions (timeouts, large messages)
- ✅ UTF-8 and special characters
- ✅ State consistency
- ✅ Resource cleanup

### Additional Test Strength
The test suite includes:
- **Regression tests** to prevent reintroduction of bugs
- **Boundary tests** for extreme values
- **Negative tests** for invalid inputs
- **Stress tests** for rapid operations
- **Concurrency tests** for thread safety

## Test Results

All unit tests should pass without requiring external dependencies or network access.

Integration tests will skip if `RUN_INTEGRATION_TESTS` is not set or if the echo server is not available.

## Debugging Failed Tests

### Verbose Output
```bash
./TallyWebSocketTests --gtest_also_run_disabled_tests --gtest_filter="*" --gtest_repeat=1
```

### List All Tests
```bash
./TallyWebSocketTests --gtest_list_tests
```

### Run Specific Test
```bash
./TallyWebSocketTests --gtest_filter="TallyWebSocketTest.Connect_WithInvalidURL_ReturnsError"
```

## Continuous Integration

The test suite is designed to work in CI environments:

```bash
# CI-friendly command
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure --timeout 300

# Integration tests can be skipped in CI or run with mock servers
```

## Dependencies

- **Google Test (gtest)** v1.14.0 or later
  - Automatically fetched by CMake if not found
  - Can be installed system-wide: `apt-get install libgtest-dev` (Debian/Ubuntu)

## Contributing

When adding new functionality to TallyWebSocket:
1. Add corresponding unit tests in `test_tally_websocket.cpp`
2. Add integration tests in `test_websocket_integration.cpp` if applicable
3. Ensure all existing tests still pass
4. Run tests with various scenarios and edge cases

## Notes

- Unit tests are designed to run without network access
- Integration tests are optional and require a running echo server
- Tests follow the AAA pattern (Arrange, Act, Assert)
- Each test is independent and can run in any order
- Tests use RAII for proper resource cleanup