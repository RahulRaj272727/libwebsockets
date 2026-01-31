/*
 * demo.cpp - Simple demo of TallyWebSocket wrapper
 *
 * This demo connects to a WebSocket echo server and
 * demonstrates basic send/receive functionality.
 */

#include "TallyWebSocket.h"
#include <iostream>
#include <chrono>
#include <thread>

/**
 * @brief Demo program that exercises the Tally WebSocket client by connecting to an echo server, sending messages, and reporting received echoes.
 *
 * The program creates a WebSocket client, registers state and message callbacks that log events and count incoming messages,
 * connects to a configurable echo server URL (default: ws://echo.websocket.org), sends three text messages, waits for echoes,
 * then disconnects and exits.
 *
 * @param argc Argument count. If greater than 1, argv[1] is used as the WebSocket URL instead of the default.
 * @param argv Argument values. argv[1] may override the default echo server URL.
 * @return int Exit status: `0` if at least three echo messages were received, `1` otherwise or on connection/setup failure.
 */
int main(int argc, char* argv[]) {
    std::cout << "=== Tally WebSocket Demo ===" << std::endl;
    std::cout << "Library: " << Tally::GetWebSocketLibraryVersion() << std::endl;
    std::cout << std::endl;

    // Create client
    auto client = Tally::CreateWebSocketClient();

    // Track received messages
    std::atomic<int> messagesReceived{0};

    // Set up callbacks
    client->SetStateCallback([](Tally::ConnectionState state, const Tally::WebSocketError* error) {
        switch (state) {
        case Tally::ConnectionState::Connecting:
            std::cout << "[STATE] Connecting..." << std::endl;
            break;
        case Tally::ConnectionState::Connected:
            std::cout << "[STATE] Connected!" << std::endl;
            break;
        case Tally::ConnectionState::Disconnected:
            std::cout << "[STATE] Disconnected" << std::endl;
            break;
        case Tally::ConnectionState::Error:
            std::cout << "[STATE] Error: " << (error ? error->message : "Unknown") << std::endl;
            break;
        default:
            break;
        }
    });

    client->SetMessageCallback([&messagesReceived](const uint8_t* data, size_t len, Tally::MessageType type) {
        std::string msg(reinterpret_cast<const char*>(data), len);
        std::cout << "[RECV] " << (type == Tally::MessageType::Text ? "Text" : "Binary")
                  << " (" << len << " bytes): " << msg << std::endl;
        messagesReceived.fetch_add(1);
    });

    // Configure connection
    Tally::WebSocketConfig config;
    // Using ws:// since we built without TLS
    // For testing, you can use local servers or public echo services
    config.url = "ws://echo.websocket.org";  // Note: This may require wss://
    config.connectTimeoutMs = 10000;

    // Check for URL argument
    if (argc > 1) {
        config.url = argv[1];
        std::cout << "Using URL from argument: " << config.url << std::endl;
    }

    std::cout << "Connecting to: " << config.url << std::endl;

    // Connect
    if (!client->Connect(config)) {
        std::cerr << "Failed to start connection" << std::endl;
        return 1;
    }

    // Poll for connection (up to 5 seconds)
    std::cout << "Waiting for connection..." << std::endl;
    for (int i = 0; i < 50 && !client->IsConnected(); i++) {
        client->Poll(100);
        if (client->GetState() == Tally::ConnectionState::Error) {
            std::cerr << "Connection failed" << std::endl;
            return 1;
        }
    }

    if (!client->IsConnected()) {
        std::cerr << "Connection timeout" << std::endl;
        return 1;
    }

    // Send some messages
    std::cout << std::endl << "Sending test messages..." << std::endl;

    client->SendText("Hello from Tally WebSocket!");
    client->SendText("Message 2: Testing 123");
    client->SendText("Message 3: WebSocket is working!");

    // Poll to send and receive (up to 5 seconds)
    std::cout << "Waiting for echo responses..." << std::endl;
    for (int i = 0; i < 50 && messagesReceived < 3; i++) {
        client->Poll(100);
    }

    // Results
    std::cout << std::endl;
    std::cout << "=== Results ===" << std::endl;
    std::cout << "Messages received: " << messagesReceived << "/3" << std::endl;

    // Disconnect
    client->Disconnect();

    // Final poll to process disconnect
    for (int i = 0; i < 10; i++) {
        client->Poll(50);
    }

    std::cout << "Demo complete." << std::endl;
    return (messagesReceived >= 3) ? 0 : 1;
}