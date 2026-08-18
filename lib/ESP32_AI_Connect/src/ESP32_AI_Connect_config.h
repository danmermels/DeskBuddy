// ESP32_AI_Connect/ESP32_AI_Connect_config.h

#ifndef ESP32_AI_CONNECT_CONFIG_H
#define ESP32_AI_CONNECT_CONFIG_H

// --- Platform Selection ---
// Only OpenAI-compatible (Groq) is used by this project
#define USE_AI_API_OPENAI
// #define USE_AI_API_GEMINI
// #define USE_AI_API_DEEPSEEK
// #define USE_AI_API_CLAUDE
// #define USE_AI_API_GROK

// --- Tool Calls Support --- (not used by this project)
// #define ENABLE_TOOL_CALLS

// --- Streaming Chat Support --- (not used by this project)
// #define ENABLE_STREAM_CHAT

// --- Debug Options ---
// #define ENABLE_DEBUG_OUTPUT

// --- Advanced Configuration ---
#define AI_API_REQ_JSON_DOC_SIZE 5120
#define AI_API_RESP_JSON_DOC_SIZE 16384
#define AI_API_HTTP_TIMEOUT_MS 30000

// --- Connection Resilience ---
// Retries transient network/TLS failures (connection refused, 5xx) with
// exponential backoff. Needed because the TLS handshake can intermittently hit
// out-of-memory (-32512) when the faceplate sprites fragment the heap.
#define ENABLE_AUTO_RETRY
#define AUTO_RETRY_MAX_ATTEMPTS 2
#define AUTO_RETRY_INITIAL_DELAY_MS 1500
#define AUTO_RETRY_MAX_DELAY_MS 8000
#define AUTO_RETRY_STALE_CONNECTION_THRESHOLD_MS 30000

#endif // ESP32_AI_CONNECT_CONFIG_H
