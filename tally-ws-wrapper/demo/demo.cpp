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
    // Default to localhost for safety, use CLI arg for specifics
    config.url = "ws://localhost:7681"; 
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
