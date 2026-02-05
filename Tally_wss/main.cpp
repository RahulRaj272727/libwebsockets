// Simple test harness for Tally WebSocket POC
// Include C headers first before wss.hpp (which has extern "C")
#include <cstdio>
#include "wss.hpp"

int main() {
    printf("=== Tally WebSocket Cloud Test ===\n");
    printf("Connecting to apigw.tdl.poc.tgodev.com (WSS)...\n\n");
    
    TestWebSocketLifecycle();
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
