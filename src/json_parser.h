/**
 * Minimal JSON Parser for OddSockets C SDK
 * Simple key-value extraction from flat JSON objects.
 * Not a full parser — handles the subset needed for API responses.
 */

#ifndef ODDSOCKETS_JSON_PARSER_H
#define ODDSOCKETS_JSON_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct json_object json_object_t;

/**
 * Parse a JSON string into an object
 * @param json_string The JSON string to parse
 * @return Parsed object or NULL on error
 */
json_object_t* json_parse(const char* json_string);

/**
 * Get a string value by key (supports dot notation e.g. "session.id")
 * @param obj The JSON object
 * @param key The key to look up
 * @return String value or NULL if not found
 */
const char* json_get_string(json_object_t* obj, const char* key);

/**
 * Get an integer value by key
 * @param obj The JSON object
 * @param key The key to look up
 * @param default_value Value to return if key not found
 * @return Integer value or default_value
 */
int json_get_int(json_object_t* obj, const char* key, int default_value);

/**
 * Get a boolean value by key
 * @param obj The JSON object
 * @param key The key to look up
 * @param default_value Value to return if key not found
 * @return Boolean value or default_value
 */
bool json_get_bool(json_object_t* obj, const char* key, bool default_value);

/**
 * Free a parsed JSON object
 * @param obj The JSON object to free
 */
void json_free(json_object_t* obj);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_JSON_PARSER_H */
