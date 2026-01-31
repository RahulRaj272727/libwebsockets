# Quick Start Guide - TallyWebSocket Tests

This guide will get you running tests in 5 minutes.

## Prerequisites

- CMake 3.10+
- C++17 compatible compiler
- Google Test (auto-downloaded if not found)
- libwebsockets (already built in parent project)

## Step 1: Build the Project

```bash
cd tally-ws-wrapper
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Step 2: Run Unit Tests

Unit tests don't require any server or network access:

```bash
# From build directory
./TallyWebSocketTests

# Or using CTest
ctest --output-on-failure -R UnitTests
```

Expected output:
```
[==========] Running 56 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 56 tests from TallyWebSocketTest
...
[----------] 56 tests from TallyWebSocketTest (XX ms total)
[==========] 56 tests from 1 test suite ran. (XX ms total)
[  PASSED  ] 56 tests.
```

## Step 3: Run Integration Tests (Optional)

Integration tests require a WebSocket echo server.

### Option A: Use the Provided Echo Server

```bash
# Terminal 1: Start echo server
cd tally-ws-wrapper/tests
python3 echo_server.py

# Terminal 2: Run integration tests
cd tally-ws-wrapper/build
export RUN_INTEGRATION_TESTS=1
./TallyWebSocketIntegrationTests
```

### Option B: Use Docker

```bash
# Terminal 1: Run echo server in Docker
docker run -d -p 8080:8080 --name echo-server jmalloc/echo-server

# Terminal 2: Run integration tests
cd tally-ws-wrapper/build
export RUN_INTEGRATION_TESTS=1
./TallyWebSocketIntegrationTests

# Cleanup
docker stop echo-server && docker rm echo-server
```

### Option C: Skip Integration Tests

Integration tests are skipped by default:

```bash
# This will skip integration tests automatically
./TallyWebSocketIntegrationTests

# Output: "Integration tests disabled. Set RUN_INTEGRATION_TESTS=1 to enable."
```

## Step 4: Run All Tests

```bash
# Using the convenience script
cd tally-ws-wrapper/tests
./run_tests.sh --integration  # with integration tests
./run_tests.sh                 # unit tests only

# Or using CMake
cd tally-ws-wrapper/build
cmake --build . --target check
```

## Interpreting Results

### Success
```
[  PASSED  ] All tests passed
```

### Failure
```
[  FAILED  ] TestName
Expected: <value>
Actual: <value>
```

Check the output for details on what failed.

## Common Issues

### Issue: "cmake: command not found"

**Solution:** Install CMake:
- Ubuntu/Debian: `sudo apt-get install cmake`
- macOS: `brew install cmake`
- Windows: Download from https://cmake.org/

### Issue: "gtest not found"

**Solution:** CMake will automatically download it. If that fails:
- Ubuntu/Debian: `sudo apt-get install libgtest-dev`
- macOS: `brew install googletest`

### Issue: Integration tests all skip

**Solution:** This is normal! Set `RUN_INTEGRATION_TESTS=1` to enable them.

### Issue: "Failed to connect to echo server"

**Solution:**
1. Verify echo server is running: `curl http://localhost:8080` (should respond)
2. Check firewall settings
3. Try different port: `export ECHO_SERVER_URL=ws://localhost:9999`

### Issue: Windows build fails with path errors

**Solution:** Update paths in CMakeLists.txt to match your environment:
- `TALLY_XLIBS`
- `OPENSSL_ROOT_DIR`
- `ZLIB_ROOT`
- `LWS_BUILD_DIR`

## Continuous Integration

For CI pipelines:

```yaml
# Example GitHub Actions
- name: Build and Test
  run: |
    cd tally-ws-wrapper
    mkdir build && cd build
    cmake ..
    cmake --build .
    ctest --output-on-failure
```

## Next Steps

- Read [README.md](README.md) for detailed test documentation
- Check [TEST_COVERAGE.md](TEST_COVERAGE.md) for coverage details
- Review test source code to understand test patterns

## Test Statistics

- **Unit Tests:** 56 tests covering all APIs
- **Integration Tests:** 9 tests for real connections
- **Total Lines:** 1072+ lines of test code
- **Execution Time:** <5 seconds (unit tests)

## Help

If you encounter issues:
1. Check this guide
2. Read [README.md](README.md)
3. Review test output carefully
4. Check that libwebsockets is built correctly

## Summary

```bash
# Quick command reference
cd tally-ws-wrapper
mkdir build && cd build
cmake .. && cmake --build .
./TallyWebSocketTests                    # Unit tests
RUN_INTEGRATION_TESTS=1 ./TallyWebSocketIntegrationTests  # Integration
```

That's it! You're now running comprehensive tests on TallyWebSocket. 🚀