#pragma once

// Forward declaring the function to be used in Tally
void TestWebSocketLifecycle();

// All the below code is done as was getting multiple linker error when compiling Tally

// 1. Suppress Warnings
#if defined(_MSC_VER)
    #pragma warning(push, 0)
    #pragma warning(disable : 4668) 
#endif

// 2. Fix C99 Macro
#if defined(_MSC_VER) && !defined(__STDC_VERSION__)
    #define __STDC_VERSION__ 0
#endif

// 3. UNDEFINE EVERYTHING

// Windows
#pragma push_macro("ERROR")
#undef ERROR
#pragma push_macro("min")
#undef min
#pragma push_macro("max")
#undef max
#pragma push_macro("GetObject")
#undef GetObject

// Verbs
#pragma push_macro("DELETE")
#undef DELETE
#pragma push_macro("GET")
#undef GET
#pragma push_macro("POST")
#undef POST
#pragma push_macro("PUT")
#undef PUT
#pragma push_macro("HEAD")
#undef HEAD
#pragma push_macro("OPTIONS")
#undef OPTIONS
#pragma push_macro("TRACE")
#undef TRACE
#pragma push_macro("CONNECT")
#undef CONNECT

// Status Codes (The "Macro Hell")
#pragma push_macro("HTTP_STATUS_CONTINUE")
#undef HTTP_STATUS_CONTINUE
#pragma push_macro("HTTP_STATUS_OK")
#undef HTTP_STATUS_OK
#pragma push_macro("HTTP_STATUS_NO_CONTENT")
#undef HTTP_STATUS_NO_CONTENT
#pragma push_macro("HTTP_STATUS_PARTIAL_CONTENT")
#undef HTTP_STATUS_PARTIAL_CONTENT
#pragma push_macro("HTTP_STATUS_MOVED_PERMANENTLY")
#undef HTTP_STATUS_MOVED_PERMANENTLY
#pragma push_macro("HTTP_STATUS_FOUND")
#undef HTTP_STATUS_FOUND
#pragma push_macro("HTTP_STATUS_SEE_OTHER")
#undef HTTP_STATUS_SEE_OTHER
#pragma push_macro("HTTP_STATUS_NOT_MODIFIED")
#undef HTTP_STATUS_NOT_MODIFIED
#pragma push_macro("HTTP_STATUS_BAD_REQUEST")
#undef HTTP_STATUS_BAD_REQUEST
#pragma push_macro("HTTP_STATUS_UNAUTHORIZED")
#undef HTTP_STATUS_UNAUTHORIZED
#pragma push_macro("HTTP_STATUS_PAYMENT_REQUIRED")
#undef HTTP_STATUS_PAYMENT_REQUIRED
#pragma push_macro("HTTP_STATUS_FORBIDDEN")
#undef HTTP_STATUS_FORBIDDEN
#pragma push_macro("HTTP_STATUS_NOT_FOUND")
#undef HTTP_STATUS_NOT_FOUND
#pragma push_macro("HTTP_STATUS_METHOD_NOT_ALLOWED")
#undef HTTP_STATUS_METHOD_NOT_ALLOWED
#pragma push_macro("HTTP_STATUS_NOT_ACCEPTABLE")
#undef HTTP_STATUS_NOT_ACCEPTABLE
#pragma push_macro("HTTP_STATUS_PROXY_AUTH_REQUIRED")
#undef HTTP_STATUS_PROXY_AUTH_REQUIRED
#pragma push_macro("HTTP_STATUS_REQUEST_TIMEOUT")
#undef HTTP_STATUS_REQUEST_TIMEOUT
#pragma push_macro("HTTP_STATUS_CONFLICT")
#undef HTTP_STATUS_CONFLICT
#pragma push_macro("HTTP_STATUS_GONE")
#undef HTTP_STATUS_GONE
#pragma push_macro("HTTP_STATUS_LENGTH_REQUIRED")
#undef HTTP_STATUS_LENGTH_REQUIRED
#pragma push_macro("HTTP_STATUS_PRECONDITION_FAILED")
#undef HTTP_STATUS_PRECONDITION_FAILED
#pragma push_macro("HTTP_STATUS_REQ_ENTITY_TOO_LARGE")
#undef HTTP_STATUS_REQ_ENTITY_TOO_LARGE
#pragma push_macro("HTTP_STATUS_REQ_URI_TOO_LONG")
#undef HTTP_STATUS_REQ_URI_TOO_LONG
#pragma push_macro("HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE")
#undef HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE
#pragma push_macro("HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE")
#undef HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE
#pragma push_macro("HTTP_STATUS_EXPECTATION_FAILED")
#undef HTTP_STATUS_EXPECTATION_FAILED
#pragma push_macro("HTTP_STATUS_INTERNAL_SERVER_ERROR")
#undef HTTP_STATUS_INTERNAL_SERVER_ERROR
#pragma push_macro("HTTP_STATUS_NOT_IMPLEMENTED")
#undef HTTP_STATUS_NOT_IMPLEMENTED
#pragma push_macro("HTTP_STATUS_BAD_GATEWAY")
#undef HTTP_STATUS_BAD_GATEWAY
#pragma push_macro("HTTP_STATUS_SERVICE_UNAVAILABLE")
#undef HTTP_STATUS_SERVICE_UNAVAILABLE
#pragma push_macro("HTTP_STATUS_GATEWAY_TIMEOUT")
#undef HTTP_STATUS_GATEWAY_TIMEOUT
#pragma push_macro("HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED")
#undef HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED


// 4. Include Library (libwebsockets.h has its own C linkage guards)
#include <libwebsockets.h>

// 5. Restore Macros (Only listing headers for brevity, compiler doesn't care about order here as long as pop matches push stack)
#pragma pop_macro("HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED")
#pragma pop_macro("HTTP_STATUS_GATEWAY_TIMEOUT")
#pragma pop_macro("HTTP_STATUS_SERVICE_UNAVAILABLE")
#pragma pop_macro("HTTP_STATUS_BAD_GATEWAY")
#pragma pop_macro("HTTP_STATUS_NOT_IMPLEMENTED")
#pragma pop_macro("HTTP_STATUS_INTERNAL_SERVER_ERROR")
#pragma pop_macro("HTTP_STATUS_EXPECTATION_FAILED")
#pragma pop_macro("HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE")
#pragma pop_macro("HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE")
#pragma pop_macro("HTTP_STATUS_REQ_URI_TOO_LONG")
#pragma pop_macro("HTTP_STATUS_REQ_ENTITY_TOO_LARGE")
#pragma pop_macro("HTTP_STATUS_PRECONDITION_FAILED")
#pragma pop_macro("HTTP_STATUS_LENGTH_REQUIRED")
#pragma pop_macro("HTTP_STATUS_GONE")
#pragma pop_macro("HTTP_STATUS_CONFLICT")
#pragma pop_macro("HTTP_STATUS_REQUEST_TIMEOUT")
#pragma pop_macro("HTTP_STATUS_PROXY_AUTH_REQUIRED")
#pragma pop_macro("HTTP_STATUS_NOT_ACCEPTABLE")
#pragma pop_macro("HTTP_STATUS_METHOD_NOT_ALLOWED")
#pragma pop_macro("HTTP_STATUS_NOT_FOUND")
#pragma pop_macro("HTTP_STATUS_FORBIDDEN")
#pragma pop_macro("HTTP_STATUS_PAYMENT_REQUIRED")
#pragma pop_macro("HTTP_STATUS_UNAUTHORIZED")
#pragma pop_macro("HTTP_STATUS_BAD_REQUEST")
#pragma pop_macro("HTTP_STATUS_NOT_MODIFIED")
#pragma pop_macro("HTTP_STATUS_SEE_OTHER")
#pragma pop_macro("HTTP_STATUS_FOUND")
#pragma pop_macro("HTTP_STATUS_MOVED_PERMANENTLY")
#pragma pop_macro("HTTP_STATUS_PARTIAL_CONTENT")
#pragma pop_macro("HTTP_STATUS_NO_CONTENT")
#pragma pop_macro("HTTP_STATUS_OK")
#pragma pop_macro("HTTP_STATUS_CONTINUE")

#pragma pop_macro("CONNECT")
#pragma pop_macro("TRACE")
#pragma pop_macro("OPTIONS")
#pragma pop_macro("HEAD")
#pragma pop_macro("PUT")
#pragma pop_macro("POST")
#pragma pop_macro("GET")
#pragma pop_macro("DELETE")

#pragma pop_macro("GetObject")
#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("ERROR")

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
