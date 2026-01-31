#!/bin/bash
# run_tests.sh - Convenience script for running TallyWebSocket tests

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-../build}"
RUN_INTEGRATION="${RUN_INTEGRATION_TESTS:-0}"
ECHO_SERVER_URL="${ECHO_SERVER_URL:-ws://localhost:8080}"

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --integration)
            RUN_INTEGRATION=1
            shift
            ;;
        --echo-server)
            ECHO_SERVER_URL="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --build-dir DIR       Build directory (default: ../build)"
            echo "  --integration         Run integration tests"
            echo "  --echo-server URL     Echo server URL (default: ws://localhost:8080)"
            echo "  --help                Show this help message"
            echo ""
            echo "Environment variables:"
            echo "  BUILD_DIR             Same as --build-dir"
            echo "  RUN_INTEGRATION_TESTS Same as --integration"
            echo "  ECHO_SERVER_URL       Same as --echo-server"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

print_header "TallyWebSocket Test Runner"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    print_error "Build directory not found: $BUILD_DIR"
    print_info "Please build the project first:"
    echo "  mkdir -p $BUILD_DIR"
    echo "  cd $BUILD_DIR"
    echo "  cmake .."
    echo "  cmake --build ."
    exit 1
fi

# Check for test executables
UNIT_TEST_BIN="$BUILD_DIR/TallyWebSocketTests"
INTEGRATION_TEST_BIN="$BUILD_DIR/TallyWebSocketIntegrationTests"

if [ ! -f "$UNIT_TEST_BIN" ]; then
    print_error "Unit test executable not found: $UNIT_TEST_BIN"
    print_info "Build tests with: cmake --build $BUILD_DIR"
    exit 1
fi

# Run unit tests
print_header "Running Unit Tests"
print_info "Executable: $UNIT_TEST_BIN"
echo ""

if "$UNIT_TEST_BIN"; then
    print_success "Unit tests passed!"
    UNIT_RESULT=0
else
    print_error "Unit tests failed!"
    UNIT_RESULT=1
fi

echo ""

# Run integration tests if requested
if [ "$RUN_INTEGRATION" = "1" ]; then
    if [ ! -f "$INTEGRATION_TEST_BIN" ]; then
        print_error "Integration test executable not found: $INTEGRATION_TEST_BIN"
        print_info "Build tests with: cmake --build $BUILD_DIR"
        exit 1
    fi

    print_header "Running Integration Tests"
    print_info "Executable: $INTEGRATION_TEST_BIN"
    print_info "Echo server: $ECHO_SERVER_URL"
    echo ""

    export RUN_INTEGRATION_TESTS=1
    export ECHO_SERVER_URL="$ECHO_SERVER_URL"

    if "$INTEGRATION_TEST_BIN"; then
        print_success "Integration tests passed!"
        INTEGRATION_RESULT=0
    else
        print_error "Integration tests failed!"
        INTEGRATION_RESULT=1
    fi

    echo ""
else
    print_warning "Integration tests skipped (use --integration to enable)"
    print_info "Integration tests require a running echo server"
    INTEGRATION_RESULT=0
fi

# Summary
print_header "Test Summary"
if [ $UNIT_RESULT -eq 0 ]; then
    print_success "Unit Tests: PASSED"
else
    print_error "Unit Tests: FAILED"
fi

if [ "$RUN_INTEGRATION" = "1" ]; then
    if [ $INTEGRATION_RESULT -eq 0 ]; then
        print_success "Integration Tests: PASSED"
    else
        print_error "Integration Tests: FAILED"
    fi
else
    print_info "Integration Tests: SKIPPED"
fi

echo ""

# Exit with error if any tests failed
if [ $UNIT_RESULT -ne 0 ] || [ $INTEGRATION_RESULT -ne 0 ]; then
    print_error "Some tests failed!"
    exit 1
else
    print_success "All tests passed!"
    exit 0
fi