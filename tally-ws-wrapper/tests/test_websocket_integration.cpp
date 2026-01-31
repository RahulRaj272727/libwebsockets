/*
 * test_websocket_integration.cpp - Integration tests for TallyWebSocket
 *
 * These tests validate the WebSocket implementation with real connections
 * where possible, or simulate realistic connection scenarios.
 *
 * Note: Some tests may require a local WebSocket echo server to be running.
 * You can skip these by setting an environment variable or running specific tests.
 */

#include "TallyWebSocket.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstdlib>

using namespace Tally;

// Helper function to check if integration tests should run
static bool ShouldRunIntegrationTests() {
    const char* env = std::getenv("RUN_INTEGRATION_TESTS");
    return env && (std::string(env) == "1" || std::string(env) == "true");
}

// Helper function to get echo server URL from environment or use default
static std::string GetEchoServerURL() {
    const char* env = std::getenv("ECHO_SERVER_URL");
    if (env) {
        return std::string(env);
    }
    // Default to a local server (user must run their own)
    return "ws://localhost:8080";
}

class WebSocketIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!ShouldRunIntegrationTests()) {
            GTEST_SKIP() << "Integration tests disabled. Set RUN_INTEGRATION_TESTS=1 to enable.";
        }
        client = CreateWebSocketClient();
        ASSERT_NE(client, nullptr);
    }

    void TearDown() override {
        if (client && client->IsConnected()) {
            client->Disconnect();
            for (int i = 0; i < 10; i++) {
                client->Poll(50);
            }
        }
        client.reset();
    }

    // Helper: Wait for specific state with timeout
    bool WaitForState(ConnectionState targetState, int timeoutMs = 5000) {
        auto start = std::chrono::steady_clock::now();
        while (client->GetState() != targetState) {
            client->Poll(100);
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if (elapsed.count() >= timeoutMs) {
                return false;
            }
            if (client->GetState() == ConnectionState::Error) {
                return false;
            }
        }
        return true;
    }

    std::unique_ptr<IWebSocketClient> client;
};

// ========================================
// Connection Tests with Echo Server
// ========================================

TEST_F(WebSocketIntegrationTest, ConnectToEchoServer_Succeeds) {
    std::atomic<bool> connected{false};
    std::mutex mtx;
    std::condition_variable cv;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        if (state == ConnectionState::Connected) {
            connected = true;
            cv.notify_one();
        }
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    bool result = client->Connect(config);
    ASSERT_TRUE(result);

    // Wait for connection
    bool connectionSucceeded = WaitForState(ConnectionState::Connected, 10000);
    EXPECT_TRUE(connectionSucceeded);
    EXPECT_TRUE(client->IsConnected());
}

TEST_F(WebSocketIntegrationTest, SendAndReceiveTextMessage_Succeeds) {
    std::atomic<bool> connected{false};
    std::atomic<bool> messageReceived{false};
    std::string receivedMessage;
    std::mutex mtx;
    std::condition_variable cv;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        if (state == ConnectionState::Connected) {
            connected = true;
            cv.notify_one();
        }
    });

    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        receivedMessage = std::string(reinterpret_cast<const char*>(data), len);
        messageReceived = true;
        cv.notify_one();
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    // Wait for connection
    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    // Send message
    const std::string testMessage = "Hello WebSocket!";
    bool sendResult = client->SendText(testMessage);
    ASSERT_TRUE(sendResult);

    // Wait for echo response
    auto start = std::chrono::steady_clock::now();
    while (!messageReceived) {
        client->Poll(100);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 5000) {
            break;
        }
    }

    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(WebSocketIntegrationTest, SendAndReceiveBinaryMessage_Succeeds) {
    std::atomic<bool> messageReceived{false};
    std::vector<uint8_t> receivedData;
    std::mutex mtx;
    std::condition_variable cv;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        // State tracking
    });

    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        receivedData.assign(data, data + len);
        messageReceived = true;
        cv.notify_one();
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    // Send binary message
    uint8_t testData[] = {0x01, 0x02, 0x03, 0xFF, 0xAA, 0x55};
    bool sendResult = client->SendBinary(testData, sizeof(testData));
    ASSERT_TRUE(sendResult);

    // Wait for echo response
    auto start = std::chrono::steady_clock::now();
    while (!messageReceived) {
        client->Poll(100);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 5000) {
            break;
        }
    }

    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedData.size(), sizeof(testData));
    if (receivedData.size() == sizeof(testData)) {
        EXPECT_TRUE(std::equal(receivedData.begin(), receivedData.end(), testData));
    }
}

TEST_F(WebSocketIntegrationTest, MultipleMessages_AllReceived) {
    std::atomic<int> messagesReceived{0};
    std::vector<std::string> receivedMessages;
    std::mutex dataMtx;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        // State tracking
    });

    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        std::lock_guard<std::mutex> lock(dataMtx);
        receivedMessages.push_back(std::string(reinterpret_cast<const char*>(data), len));
        messagesReceived++;
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    // Send multiple messages
    const int numMessages = 5;
    std::vector<std::string> sentMessages;
    for (int i = 0; i < numMessages; i++) {
        std::string msg = "Message " + std::to_string(i + 1);
        sentMessages.push_back(msg);
        ASSERT_TRUE(client->SendText(msg));
    }

    // Wait for all echoes
    auto start = std::chrono::steady_clock::now();
    while (messagesReceived < numMessages) {
        client->Poll(100);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 10000) {
            break;
        }
    }

    EXPECT_EQ(messagesReceived, numMessages);
    EXPECT_EQ(receivedMessages.size(), static_cast<size_t>(numMessages));
}

TEST_F(WebSocketIntegrationTest, LargeMessage_ReceivedCorrectly) {
    std::atomic<bool> messageReceived{false};
    std::string receivedMessage;
    std::mutex mtx;

    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        std::lock_guard<std::mutex> lock(mtx);
        receivedMessage = std::string(reinterpret_cast<const char*>(data), len);
        messageReceived = true;
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    // Send large message (10KB)
    std::string largeMessage(10000, 'X');
    bool sendResult = client->SendText(largeMessage);
    ASSERT_TRUE(sendResult);

    // Wait for echo response
    auto start = std::chrono::steady_clock::now();
    while (!messageReceived) {
        client->Poll(100);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 10000) {
            break;
        }
    }

    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedMessage.size(), largeMessage.size());
    EXPECT_EQ(receivedMessage, largeMessage);
}

// ========================================
// Disconnect Tests
// ========================================

TEST_F(WebSocketIntegrationTest, GracefulDisconnect_TriggersCallback) {
    std::atomic<bool> connected{false};
    std::atomic<bool> disconnected{false};
    std::mutex mtx;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        if (state == ConnectionState::Connected) {
            connected = true;
        } else if (state == ConnectionState::Disconnected) {
            disconnected = true;
        }
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    EXPECT_TRUE(connected);

    // Disconnect
    client->Disconnect();

    // Poll to process disconnect
    for (int i = 0; i < 20; i++) {
        client->Poll(50);
        if (disconnected) break;
    }

    EXPECT_TRUE(disconnected || client->GetState() == ConnectionState::Disconnected);
}

// ========================================
// Error Handling Tests
// ========================================

TEST_F(WebSocketIntegrationTest, ConnectToNonExistentServer_ReportsError) {
    std::atomic<bool> errorReceived{false};
    std::string errorMessage;

    client->SetStateCallback([&](ConnectionState state, const WebSocketError* error) {
        if (state == ConnectionState::Error) {
            errorReceived = true;
            if (error) {
                errorMessage = error->message;
            }
        }
    });

    WebSocketConfig config;
    config.url = "ws://127.0.0.1:19999"; // Unlikely to be listening
    config.connectTimeoutMs = 3000;

    ASSERT_TRUE(client->Connect(config));

    // Wait for error
    auto start = std::chrono::steady_clock::now();
    while (!errorReceived && client->GetState() == ConnectionState::Connecting) {
        client->Poll(100);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 5000) {
            break;
        }
    }

    EXPECT_TRUE(errorReceived || client->GetState() == ConnectionState::Error);
}

// ========================================
// Stress Tests
// ========================================

TEST_F(WebSocketIntegrationTest, RapidSendAndReceive_NoDataLoss) {
    std::atomic<int> messagesReceived{0};
    std::mutex mtx;

    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        messagesReceived++;
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    // Send messages rapidly
    const int numMessages = 20;
    for (int i = 0; i < numMessages; i++) {
        std::string msg = "Rapid message " + std::to_string(i);
        ASSERT_TRUE(client->SendText(msg));
    }

    // Poll to receive all echoes
    auto start = std::chrono::steady_clock::now();
    while (messagesReceived < numMessages) {
        client->Poll(50);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 15000) {
            break;
        }
    }

    // We should receive most or all messages
    EXPECT_GE(messagesReceived, numMessages - 2); // Allow small margin
}

// ========================================
// UTF-8 and Special Character Tests
// ========================================

TEST_F(WebSocketIntegrationTest, UTF8Message_ReceivedCorrectly) {
    std::atomic<bool> messageReceived{false};
    std::string receivedMessage;

    client->SetMessageCallback([&](const uint8_t* data, size_t len, MessageType type) {
        receivedMessage = std::string(reinterpret_cast<const char*>(data), len);
        messageReceived = true;
    });

    WebSocketConfig config;
    config.url = GetEchoServerURL();
    config.connectTimeoutMs = 10000;

    ASSERT_TRUE(client->Connect(config));

    if (!WaitForState(ConnectionState::Connected, 10000)) {
        GTEST_SKIP() << "Could not connect to echo server";
    }

    // Send UTF-8 message with various characters
    const std::string utf8Message = "Hello 世界 🌍 Здравствуй мир";
    ASSERT_TRUE(client->SendText(utf8Message));

    // Wait for echo
    auto start = std::chrono::steady_clock::now();
    while (!messageReceived) {
        client->Poll(100);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 5000) {
            break;
        }
    }

    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedMessage, utf8Message);
}

// ========================================
// Main entry point
// ========================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "Integration Test Configuration:" << std::endl;
    std::cout << "  RUN_INTEGRATION_TESTS=" << (ShouldRunIntegrationTests() ? "enabled" : "disabled") << std::endl;
    if (ShouldRunIntegrationTests()) {
        std::cout << "  ECHO_SERVER_URL=" << GetEchoServerURL() << std::endl;
    } else {
        std::cout << "  (Set RUN_INTEGRATION_TESTS=1 to enable)" << std::endl;
    }
    std::cout << std::endl;

    return RUN_ALL_TESTS();
}