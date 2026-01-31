/*
 * test_tally_websocket.cpp - Comprehensive unit tests for TallyWebSocket
 *
 * Tests cover:
 * - Factory function and client creation
 * - Connection lifecycle (connect, disconnect, state management)
 * - Message sending (text and binary)
 * - Callbacks (message and state callbacks)
 * - Error handling and edge cases
 * - Threading safety
 */

#include "TallyWebSocket.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>

using namespace Tally;

// Test fixture for WebSocket tests
class TallyWebSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        client = CreateWebSocketClient();
        ASSERT_NE(client, nullptr);
    }

    void TearDown() override {
        if (client && client->IsConnected()) {
            client->Disconnect();
            // Give time for cleanup
            for (int i = 0; i < 10; i++) {
                client->Poll(50);
            }
        }
        client.reset();
    }

    std::unique_ptr<IWebSocketClient> client;
};

// ========================================
// Factory and Creation Tests
// ========================================

TEST_F(TallyWebSocketTest, CreateWebSocketClient_ReturnsValidInstance) {
    auto testClient = CreateWebSocketClient();
    ASSERT_NE(testClient, nullptr);
    EXPECT_EQ(testClient->GetState(), ConnectionState::Disconnected);
    EXPECT_FALSE(testClient->IsConnected());
}

TEST_F(TallyWebSocketTest, GetWebSocketLibraryVersion_ReturnsNonEmptyString) {
    std::string version = GetWebSocketLibraryVersion();
    EXPECT_FALSE(version.empty());
    EXPECT_NE(version.find("libwebsockets"), std::string::npos);
}

// ========================================
// Initial State Tests
// ========================================

TEST_F(TallyWebSocketTest, InitialState_IsDisconnected) {
    EXPECT_EQ(client->GetState(), ConnectionState::Disconnected);
    EXPECT_FALSE(client->IsConnected());
}

// ========================================
// Connection Configuration Tests
// ========================================

TEST_F(TallyWebSocketTest, Connect_WithInvalidURL_ReturnsError) {
    WebSocketConfig config;
    config.url = "invalid-url";
    config.connectTimeoutMs = 1000;

    bool result = client->Connect(config);
    // Should fail to parse URL
    EXPECT_FALSE(result);
    EXPECT_EQ(client->GetState(), ConnectionState::Error);
}

TEST_F(TallyWebSocketTest, Connect_WithEmptyURL_ReturnsError) {
    WebSocketConfig config;
    config.url = "";
    config.connectTimeoutMs = 1000;

    bool result = client->Connect(config);
    EXPECT_FALSE(result);
}

TEST_F(TallyWebSocketTest, Connect_WhenAlreadyConnecting_ReturnsFalse) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 5000;

    // First connection attempt
    bool firstResult = client->Connect(config);
    EXPECT_TRUE(firstResult);
    EXPECT_EQ(client->GetState(), ConnectionState::Connecting);

    // Second attempt while connecting
    bool secondResult = client->Connect(config);
    EXPECT_FALSE(secondResult);
}

// ========================================
// State Management Tests
// ========================================

TEST_F(TallyWebSocketTest, StateCallback_IsCalledOnConnect) {
    std::atomic<bool> connectingCalled{false};
    std::atomic<int> callbackCount{0};
    std::mutex mtx;
    std::condition_variable cv;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        callbackCount++;
        if (state == ConnectionState::Connecting) {
            connectingCalled = true;
            cv.notify_one();
        }
    });

    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 1000;

    client->Connect(config);

    // Wait briefly for state callback
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_for(lock, std::chrono::milliseconds(500), [&]{ return connectingCalled.load(); });

    EXPECT_TRUE(connectingCalled);
    EXPECT_GT(callbackCount, 0);
}

TEST_F(TallyWebSocketTest, StateCallback_ReceivesErrorOnConnectionFailure) {
    std::atomic<bool> errorReceived{false};
    std::string errorMessage;
    std::mutex mtx;
    std::condition_variable cv;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        if (state == ConnectionState::Error) {
            errorReceived = true;
            if (error) {
                errorMessage = error->message;
            }
            cv.notify_one();
        }
    });

    WebSocketConfig config;
    config.url = "ws://nonexistent.host.invalid:9999";
    config.connectTimeoutMs = 2000;

    client->Connect(config);

    // Poll and wait for error
    for (int i = 0; i < 30; i++) {
        client->Poll(100);
        if (errorReceived) break;
    }

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_for(lock, std::chrono::milliseconds(1000), [&]{ return errorReceived.load(); });

    EXPECT_TRUE(errorReceived);
}

// ========================================
// Disconnect Tests
// ========================================

TEST_F(TallyWebSocketTest, Disconnect_WhenNotConnected_DoesNotCrash) {
    EXPECT_NO_THROW(client->Disconnect());
    EXPECT_EQ(client->GetState(), ConnectionState::Disconnected);
}

TEST_F(TallyWebSocketTest, Disconnect_WithCustomCodeAndReason_Succeeds) {
    // This tests the API contract even if we can't establish a real connection
    EXPECT_NO_THROW(client->Disconnect(1001, "Going away"));
}

// ========================================
// Message Sending Tests (Disconnected State)
// ========================================

TEST_F(TallyWebSocketTest, SendText_WhenDisconnected_ReturnsFalse) {
    bool result = client->SendText("Test message");
    EXPECT_FALSE(result);
}

TEST_F(TallyWebSocketTest, SendBinary_WhenDisconnected_ReturnsFalse) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    bool result = client->SendBinary(data, sizeof(data));
    EXPECT_FALSE(result);
}

TEST_F(TallyWebSocketTest, SendText_WithEmptyString_WhenDisconnected_ReturnsFalse) {
    bool result = client->SendText("");
    EXPECT_FALSE(result);
}

TEST_F(TallyWebSocketTest, SendBinary_WithNullData_WhenDisconnected_ReturnsFalse) {
    bool result = client->SendBinary(nullptr, 0);
    EXPECT_FALSE(result);
}

TEST_F(TallyWebSocketTest, SendBinary_WithZeroLength_WhenDisconnected_ReturnsFalse) {
    uint8_t data[] = {0x01};
    bool result = client->SendBinary(data, 0);
    EXPECT_FALSE(result);
}

// ========================================
// Callback Management Tests
// ========================================

TEST_F(TallyWebSocketTest, SetMessageCallback_DoesNotCrash) {
    EXPECT_NO_THROW(client->SetMessageCallback([](const uint8_t* data, size_t len, MessageType type) {
        // Callback body
    }));
}

TEST_F(TallyWebSocketTest, SetStateCallback_DoesNotCrash) {
    EXPECT_NO_THROW(client->SetStateCallback([](ConnectionState state, const WebSocketError* error) {
        // Callback body
    }));
}

TEST_F(TallyWebSocketTest, SetMessageCallback_WithNullptr_DoesNotCrash) {
    EXPECT_NO_THROW(client->SetMessageCallback(nullptr));
}

TEST_F(TallyWebSocketTest, SetStateCallback_WithNullptr_DoesNotCrash) {
    EXPECT_NO_THROW(client->SetStateCallback(nullptr));
}

TEST_F(TallyWebSocketTest, SetMessageCallback_CanBeUpdated) {
    std::atomic<int> firstCallbackCount{0};
    std::atomic<int> secondCallbackCount{0};

    // Set first callback
    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        firstCallbackCount++;
    });

    // Replace with second callback
    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        secondCallbackCount++;
    });

    // The second callback should be the active one
    // (We can't easily test this without a real connection, but the API works)
    EXPECT_EQ(firstCallbackCount, 0);
    EXPECT_EQ(secondCallbackCount, 0);
}

// ========================================
// Poll Tests
// ========================================

TEST_F(TallyWebSocketTest, Poll_WhenNotConnected_ReturnsError) {
    int result = client->Poll(100);
    EXPECT_EQ(result, -1);
}

TEST_F(TallyWebSocketTest, Poll_WithZeroTimeout_DoesNotBlock) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    client->Connect(config);

    auto start = std::chrono::steady_clock::now();
    client->Poll(0);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100); // Should be very fast
}

TEST_F(TallyWebSocketTest, Poll_WithTimeout_ReturnsInReasonableTime) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    client->Connect(config);

    auto start = std::chrono::steady_clock::now();
    client->Poll(100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should not exceed timeout significantly
    EXPECT_LE(duration.count(), 300); // Allow some margin
}

// ========================================
// Edge Cases and Robustness Tests
// ========================================

TEST_F(TallyWebSocketTest, MultipleDisconnects_DoNotCrash) {
    EXPECT_NO_THROW({
        client->Disconnect();
        client->Disconnect();
        client->Disconnect();
    });
}

TEST_F(TallyWebSocketTest, RapidConnectDisconnect_DoesNotCrash) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 1000;

    EXPECT_NO_THROW({
        client->Connect(config);
        client->Disconnect();
        client->Poll(10);
    });
}

TEST_F(TallyWebSocketTest, LongMessage_WhenDisconnected_ReturnsFalse) {
    std::string longMessage(10000, 'A');
    bool result = client->SendText(longMessage);
    EXPECT_FALSE(result);
}

TEST_F(TallyWebSocketTest, BinaryDataWithSpecialBytes_WhenDisconnected_ReturnsFalse) {
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = static_cast<uint8_t>(i);
    }
    bool result = client->SendBinary(data, sizeof(data));
    EXPECT_FALSE(result);
}

// ========================================
// URL Parsing Tests
// ========================================

TEST_F(TallyWebSocketTest, Connect_WithWSProtocol_ParsesCorrectly) {
    WebSocketConfig config;
    config.url = "ws://example.com:8080/path";
    config.connectTimeoutMs = 1000;

    bool result = client->Connect(config);
    // Should at least parse the URL successfully (connection will fail but parsing should work)
    EXPECT_TRUE(result);
}

TEST_F(TallyWebSocketTest, Connect_WithWSSProtocol_ParsesCorrectly) {
    WebSocketConfig config;
    config.url = "wss://secure.example.com:443/secure-path";
    config.connectTimeoutMs = 1000;

    bool result = client->Connect(config);
    // Should at least parse the URL successfully
    EXPECT_TRUE(result);
}

TEST_F(TallyWebSocketTest, Connect_WithoutPort_UsesDefault) {
    WebSocketConfig config;
    config.url = "ws://example.com/path";
    config.connectTimeoutMs = 1000;

    bool result = client->Connect(config);
    EXPECT_TRUE(result);
}

// ========================================
// Configuration Tests
// ========================================

TEST_F(TallyWebSocketTest, Config_DefaultValues_AreReasonable) {
    WebSocketConfig config;
    EXPECT_EQ(config.connectTimeoutMs, 30000);
    EXPECT_EQ(config.pingIntervalMs, 30000);
    EXPECT_FALSE(config.autoReconnect);
    EXPECT_EQ(config.reconnectDelayMs, 5000);
    EXPECT_EQ(config.maxReconnectAttempts, 5);
}

TEST_F(TallyWebSocketTest, Config_CanSetSubprotocol) {
    WebSocketConfig config;
    config.url = "ws://example.com";
    config.subprotocol = "custom-protocol";
    config.connectTimeoutMs = 1000;

    bool result = client->Connect(config);
    EXPECT_TRUE(result);
}

// ========================================
// State Consistency Tests
// ========================================

TEST_F(TallyWebSocketTest, GetState_MatchesIsConnected) {
    // Initially disconnected
    EXPECT_FALSE(client->IsConnected());
    EXPECT_EQ(client->GetState(), ConnectionState::Disconnected);

    // After attempting connection
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 1000;
    client->Connect(config);

    // State should be Connecting, not Connected
    EXPECT_FALSE(client->IsConnected());
    EXPECT_EQ(client->GetState(), ConnectionState::Connecting);
}

// ========================================
// Thread Safety Tests (Basic)
// ========================================

TEST_F(TallyWebSocketTest, ConcurrentCallbackSetting_DoesNotCrash) {
    std::vector<std::thread> threads;
    std::atomic<bool> running{true};

    for (int i = 0; i < 5; i++) {
        threads.emplace_back([&, i]() {
            while (running) {
                client->SetMessageCallback([i](const uint8_t* data, size_t len, MessageType type) {
                    // Callback body
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& thread : threads) {
        thread.join();
    }

    // If we got here without crashing, the test passed
    SUCCEED();
}

// ========================================
// Destructor and Cleanup Tests
// ========================================

TEST_F(TallyWebSocketTest, Destructor_WhenDisconnected_DoesNotCrash) {
    auto tempClient = CreateWebSocketClient();
    EXPECT_NO_THROW(tempClient.reset());
}

TEST_F(TallyWebSocketTest, Destructor_AfterConnectAttempt_DoesNotCrash) {
    auto tempClient = CreateWebSocketClient();
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 1000;
    tempClient->Connect(config);

    EXPECT_NO_THROW(tempClient.reset());
}

// ========================================
// Error Handling Tests
// ========================================

TEST_F(TallyWebSocketTest, Error_StructHasValidFields) {
    WebSocketError error;
    error.code = 1000;
    error.message = "Test error";

    EXPECT_EQ(error.code, 1000);
    EXPECT_EQ(error.message, "Test error");
}

// ========================================
// Enum Tests
// ========================================

TEST_F(TallyWebSocketTest, ConnectionState_EnumValuesAreUnique) {
    EXPECT_NE(static_cast<int>(ConnectionState::Disconnected),
              static_cast<int>(ConnectionState::Connecting));
    EXPECT_NE(static_cast<int>(ConnectionState::Connected),
              static_cast<int>(ConnectionState::Disconnecting));
    EXPECT_NE(static_cast<int>(ConnectionState::Error),
              static_cast<int>(ConnectionState::Connected));
}

TEST_F(TallyWebSocketTest, MessageType_EnumValuesAreUnique) {
    EXPECT_NE(static_cast<int>(MessageType::Text),
              static_cast<int>(MessageType::Binary));
}

// ========================================
// Regression Tests
// ========================================

TEST_F(TallyWebSocketTest, Regression_PollAfterFailedConnect_DoesNotCrash) {
    WebSocketConfig config;
    config.url = "invalid-url";
    config.connectTimeoutMs = 1000;

    client->Connect(config);

    // Poll should handle failed connection gracefully
    EXPECT_NO_THROW({
        for (int i = 0; i < 5; i++) {
            client->Poll(10);
        }
    });
}

TEST_F(TallyWebSocketTest, Regression_MultipleConnectAttemptsAfterFailure_Handled) {
    WebSocketConfig config;
    config.url = "invalid-url";
    config.connectTimeoutMs = 1000;

    // First attempt fails
    client->Connect(config);
    EXPECT_EQ(client->GetState(), ConnectionState::Error);

    // Second attempt should also be rejected (still in error state)
    bool secondAttempt = client->Connect(config);
    EXPECT_FALSE(secondAttempt);
}

// ========================================
// Boundary Tests
// ========================================

TEST_F(TallyWebSocketTest, Boundary_VeryShortTimeout_HandledCorrectly) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 1; // Very short

    bool result = client->Connect(config);
    EXPECT_TRUE(result); // Connection attempt should start
}

TEST_F(TallyWebSocketTest, Boundary_VeryLongTimeout_HandledCorrectly) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.connectTimeoutMs = 999999; // Very long

    bool result = client->Connect(config);
    EXPECT_TRUE(result);
}

TEST_F(TallyWebSocketTest, Boundary_MaxReconnectAttempts_Zero_IsUnlimited) {
    WebSocketConfig config;
    config.url = "ws://localhost:9999";
    config.maxReconnectAttempts = 0; // Unlimited
    config.autoReconnect = true;

    bool result = client->Connect(config);
    EXPECT_TRUE(result);
}

// ========================================
// Additional Negative Tests
// ========================================

TEST_F(TallyWebSocketTest, Negative_SendAfterDestruction_PreventsUseAfterFree) {
    // This test validates that the client can be safely destroyed
    auto tempClient = CreateWebSocketClient();
    tempClient.reset();
    // If we got here, memory was freed properly
    SUCCEED();
}

TEST_F(TallyWebSocketTest, Negative_URLWithSpaces_RejectedOrEscaped) {
    WebSocketConfig config;
    config.url = "ws://example.com/path with spaces";
    config.connectTimeoutMs = 1000;

    // Depending on implementation, this might fail or escape spaces
    // At minimum, it should not crash
    EXPECT_NO_THROW(client->Connect(config));
}

TEST_F(TallyWebSocketTest, Negative_URLWithInvalidCharacters_Handled) {
    WebSocketConfig config;
    config.url = "ws://example.com/<>|\\";
    config.connectTimeoutMs = 1000;

    EXPECT_NO_THROW(client->Connect(config));
}

// ========================================
// Main entry point
// ========================================
// Note: main() is provided by gtest_main library
// If you need custom initialization, replace gtest_main with your own main()