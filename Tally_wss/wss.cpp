// 1. PCH Header (MUST BE FIRST). This is for Tally. This includes wss.h
#include "connector.h"

// --------------------------------------------------------------------------
// CONFIGURATION
// --------------------------------------------------------------------------
#define LOG_FILE_PATH "tally_ws_debug.txt"
#define DEMO_HOST "127.0.0.1"
#define DEMO_PORT 9002

// --------------------------------------------------------------------------
// PROTOTYPES
// --------------------------------------------------------------------------
void LogDebug(const char* fmt, ...);

// --------------------------------------------------------------------------
// Session State
// Used to track the state of this specific connection.
// --------------------------------------------------------------------------
struct TallySessionData {
    // [TODO: PRODUCTION] Add tracking data here
    // e.g., std::string pendingMessage;
    //       bool isAuthenticated;
    int msgCount;
};

// --------------------------------------------------------------------------
// [TODO: PRODUCTION] Logging
// In production, integrate this with Tally's internal logging system
// (e.g., TLog or similar) instead of writing to a raw file.
// --------------------------------------------------------------------------
void LogDebug(const char* fmt, ...) {
    FILE* f = fopen(LOG_FILE_PATH, "a");
    if (f) {
        // [TODO: PRODUCTION] Add timestamp here
        fprintf(f, "[TallyWS] ");
        
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        
        fprintf(f, "\n");
        fclose(f);
    }
}

// --------------------------------------------------------------------------
// WebSocket Callback
// This is the "Heart" of the WebSocket client.
// --------------------------------------------------------------------------
static int callback_tally_demo(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len)
{
    struct TallySessionData* session = (struct TallySessionData*)user;

    switch (reason) {
    
    // ----------------------------------------------------------------------
    // Connection Established
    // ----------------------------------------------------------------------
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        LogDebug("Connection Established!");
        if (session) {
            session->msgCount = 0;
        }
        // Ask libwebsockets to call us back when the socket is writable
        lws_callback_on_writable(wsi);
        break;

    // ----------------------------------------------------------------------
    // Wrapper is Ready for Data (Write Event)
    // ----------------------------------------------------------------------
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        // [TODO: PRODUCTION] Pick message from a thread-safe Queue
        // e.g. if (!queue.empty()) { msg = queue.pop(); ... }
        
        const char* msg = "Hello from Tally! (Polite Mode)";
        size_t msg_len = strlen(msg);
        
        // [TODO: PRODUCTION] Handle partial writes (buffer management)
        // libwebsockets requires LWS_PRE padding
        unsigned char buf[LWS_PRE + 256]; 
        memset(buf, 0, sizeof(buf));
        memcpy(&buf[LWS_PRE], msg, msg_len);

        int n = lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
        LogDebug("Sent %d bytes: %s", n, msg);
        
        // Disconnect politely after sending one message (for Demo)
        // [TODO: PRODUCTION] Remove this logic, only close when Tally shuts down
        LogDebug("Initiating polite update shutdown...");
        
        // Send a standard CLOSE frame (Code 1000 = Normal Closure)
        lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char*)"Demo Complete", 13);
        
        // Return 0. The library will send the close frame and then call 
        // LWS_CALLBACK_CLIENT_CLOSED when it's done.
        break;
    }

    // ----------------------------------------------------------------------
    // Data Received
    // ----------------------------------------------------------------------
    case LWS_CALLBACK_CLIENT_RECEIVE:
        // [TODO: PRODUCTION] Handle fragmented messages (check lws_is_final_fragment)
        LogDebug("Received: %.*s", (int)len, (char*)in);
        break;

    // ----------------------------------------------------------------------
    // Connection Errors
    // ----------------------------------------------------------------------
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LogDebug("Connection Error: %s", in ? (char*)in : "(Unknown Error)");
        // [TODO: PRODUCTION] Trigger exponential backoff reconnect logic here
        break;

    // ----------------------------------------------------------------------
    // Connection Closed
    // ----------------------------------------------------------------------
    case LWS_CALLBACK_CLIENT_CLOSED:
        LogDebug("Connection Closed.");
        // [TODO: PRODUCTION] Determine if accidental or requested, trigger reconnect if needed
        break;

    default:
        break;
    }
    return 0;
}

// --------------------------------------------------------------------------
// Protocol List
// --------------------------------------------------------------------------
static struct lws_protocols protocols[] = {
    {
        "tally-demo-protocol",  // Protocol Name
        callback_tally_demo,    // Callback handler
        sizeof(struct TallySessionData), // Per-session data size
        1024,                   // Rx buffer size
    },
    { NULL, NULL, 0, 0 }        // Terminator
};

// --------------------------------------------------------------------------
// Entry Point: Call this from Tally
// --------------------------------------------------------------------------
void TestWebSocketLifecycle() {
    LogDebug("=== Tally WebSocket POC Starting ===");

    // [TODO: PRODUCTION] Create context ONCE at app startup, reuse it.
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN; 
    info.protocols = protocols;
    // Suppress signed/unsigned warnings
    info.gid = (gid_t)-1;
    info.uid = (uid_t)-1;
    // [TODO: PRODUCTION] Load SSL Certificates here
    // info.client_ssl_ca_filepath = "./ca.pem";

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        LogDebug("Failed to create lws context");
        return;
    }

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = DEMO_HOST;
    ccinfo.port = DEMO_PORT;
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name;
    // [TODO: PRODUCTION] Enable SSL
    // ccinfo.ssl_connection = LCCSCF_USE_SSL;

    LogDebug("Connecting to %s:%d...", ccinfo.address, ccinfo.port);
    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        LogDebug("Client connection init failed");
        lws_context_destroy(context);
        return;
    }

    // ----------------------------------------------------------------------
    // Event Loop
    // [TODO: PRODUCTION] This loop blocks the thread.
    // 1. Move this to a dedicated std::thread (e.g., TallyWSWorker).
    // 2. Use a 'volatile bool running' flag to exit the loop cleanly.
    // ----------------------------------------------------------------------
    LogDebug("Entering Event Loop...");
    
    int n = 0;
    while (n >= 0) {
        // Run the loop for 100ms
        n = lws_service(context, 100); 
        
        // [DEMO ONLY] Exit after 50 iterations (approx 5 seconds) to avoid freezing Tally
        static int iterations = 0;
        if (iterations++ > 50) {
           LogDebug("Forcing loop exit for Demo.");
           break;
        }
    }

    // Polite cleanup
    lws_context_destroy(context);
    LogDebug("Context destroyed. Test Complete.");
}
