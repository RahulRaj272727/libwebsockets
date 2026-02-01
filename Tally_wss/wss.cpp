// 1. PCH Header (MUST BE FIRST)
#include "connector.h"

// --------------------------------------------------------------------------
// CONFIGURATION
// --------------------------------------------------------------------------
#define LOG_FILE_PATH "tally_ws_debug.txt"
#define DEMO_HOST "127.0.0.1"
#define DEMO_PORT 9002

// --------------------------------------------------------------------------
// GLOBAL FLAGS
// --------------------------------------------------------------------------
static bool g_demo_complete = false;

// --------------------------------------------------------------------------
// PROTOTYPES
// --------------------------------------------------------------------------
void LogDebug(const char* fmt, ...);

// --------------------------------------------------------------------------
// Session State
// --------------------------------------------------------------------------
struct TallySessionData {
    bool msgSent; // <--- FIX: Track if we sent the message
};

// --------------------------------------------------------------------------
// Logging
// --------------------------------------------------------------------------
void LogDebug(const char* fmt, ...) {
    FILE* f = fopen(LOG_FILE_PATH, "a");
    if (f) {
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
// --------------------------------------------------------------------------
static int callback_tally_demo(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len)
{
    struct TallySessionData* session = (struct TallySessionData*)user;

    switch (reason) {
    
    // --- Connection Established ---
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        LogDebug("Connection Established!");
        if (session) {
            session->msgSent = false; // Reset flag
        }
        lws_callback_on_writable(wsi);
        break;

    // --- Ready to Write ---
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        // FIX: Only send ONCE
        if (session && session->msgSent) {
            break; 
        }

        const char* msg = "Hello from Tally! (Polite Mode)";
        size_t msg_len = strlen(msg);
        
        unsigned char buf[LWS_PRE + 256]; 
        memset(buf, 0, sizeof(buf));
        memcpy(&buf[LWS_PRE], msg, msg_len);

        int n = lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
        LogDebug("Sent %d bytes: %s", n, msg);
        
        if (session) session->msgSent = true; // Mark as sent
        break;
    }

    // --- Data Received ---
    case LWS_CALLBACK_CLIENT_RECEIVE:
        LogDebug("Received: %.*s", (int)len, (char*)in);
        
        LogDebug("Echo received. Closing session...");
        lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char*)"Echo Done", 9);
        return -1; // Force Close

    // --- Errors ---
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LogDebug("Connection Error: %s", in ? (char*)in : "(Unknown)");
        g_demo_complete = true;
        break;

    // --- Closed ---
    case LWS_CALLBACK_CLIENT_CLOSED:
        LogDebug("Connection Closed.");
        g_demo_complete = true;
        break;

    default:
        break;
    }
    return 0;
}

// --------------------------------------------------------------------------
// Protocol Definition
// --------------------------------------------------------------------------
static struct lws_protocols protocols[] = {
    { "tally-demo-protocol", callback_tally_demo, sizeof(struct TallySessionData), 1024 },
    { NULL, NULL, 0, 0 }
};

// --------------------------------------------------------------------------
// Entry Point
// --------------------------------------------------------------------------
void TestWebSocketLifecycle() {
    LogDebug("=== Tally WebSocket POC Starting ===");

    g_demo_complete = false;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN; 
    info.protocols = protocols;
    info.gid = (gid_t)-1; 
    info.uid = (uid_t)-1;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        LogDebug("Failed to create context");
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

    LogDebug("Connecting to %s:%d...", ccinfo.address, ccinfo.port);
    if (!lws_client_connect_via_info(&ccinfo)) {
        LogDebug("Client init failed");
        lws_context_destroy(context);
        return;
    }

    LogDebug("Entering Event Loop...");
    
    int n = 0;
    int safety_counter = 0;
    
    while (n >= 0 && !g_demo_complete) {
        n = lws_service(context, 100); 
        
        if (safety_counter++ > 100) { 
            LogDebug("Safety Timeout Reached. Forcing Exit.");
            break;
        }
    }

    lws_context_destroy(context);
    LogDebug("Context destroyed. Test Complete.");
}
