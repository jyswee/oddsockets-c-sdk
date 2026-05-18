/**
 * Message Size Validator for OddSockets C SDK
 * 
 * Validates message sizes according to OddSockets platform limits.
 * Follows the JavaScript SDK pattern for consistent behavior across all SDKs.
 */

#ifndef ODDSOCKETS_MESSAGE_SIZE_VALIDATOR_H
#define ODDSOCKETS_MESSAGE_SIZE_VALIDATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

/* Configuration Constants */
#define ODDSOCKETS_MAX_MESSAGE_SIZE 32768  /* 32KB - industry standard */

/* Error Codes */
typedef enum {
    ODDSOCKETS_MESSAGE_SIZE_SUCCESS = 0,
    ODDSOCKETS_MESSAGE_SIZE_ERROR_INVALID_PARAMETER = -1,
    ODDSOCKETS_MESSAGE_SIZE_ERROR_TOO_LARGE = -2,
    ODDSOCKETS_MESSAGE_SIZE_ERROR_MEMORY_ALLOCATION = -3
} oddsockets_message_size_error_t;

/* Message Size Information */
typedef struct {
    bool valid;
    size_t size;
    size_t max_size;
    const char* error_message;
} oddsockets_message_size_result_t;

/**
 * Validate message size
 * 
 * Validates that a message does not exceed the maximum allowed size.
 * This function follows the same validation logic as the JavaScript SDK.
 * 
 * @param message The message to validate (null-terminated string)
 * @return ODDSOCKETS_MESSAGE_SIZE_SUCCESS if valid, error code if invalid
 */
int oddsockets_validate_message_size(const char* message);

/**
 * Validate message size with length
 * 
 * Validates that a message does not exceed the maximum allowed size.
 * Use this version when you already know the message length.
 * 
 * @param message The message to validate
 * @param message_length The length of the message in bytes
 * @return ODDSOCKETS_MESSAGE_SIZE_SUCCESS if valid, error code if invalid
 */
int oddsockets_validate_message_size_with_length(const char* message, size_t message_length);

/**
 * Check message size without throwing error
 * 
 * Checks message size and returns detailed information about the validation result.
 * This function does not return an error code, but provides all information
 * in the result structure.
 * 
 * @param message The message to check (null-terminated string)
 * @param result Output structure containing validation results
 * @return ODDSOCKETS_MESSAGE_SIZE_SUCCESS on success (regardless of validation result)
 */
int oddsockets_check_message_size(const char* message, oddsockets_message_size_result_t* result);

/**
 * Check message size with length without throwing error
 * 
 * @param message The message to check
 * @param message_length The length of the message in bytes
 * @param result Output structure containing validation results
 * @return ODDSOCKETS_MESSAGE_SIZE_SUCCESS on success (regardless of validation result)
 */
int oddsockets_check_message_size_with_length(const char* message, 
                                             size_t message_length,
                                             oddsockets_message_size_result_t* result);

/**
 * Get the maximum allowed message size
 * 
 * @return Maximum message size in bytes (32KB)
 */
size_t oddsockets_get_max_message_size(void);

/**
 * Calculate message size in bytes
 * 
 * Calculates the actual byte size of a message, accounting for UTF-8 encoding.
 * This matches the calculation logic used in the JavaScript SDK.
 * 
 * @param message The message to calculate size for (null-terminated string)
 * @return Size in bytes, or 0 if message is NULL
 */
size_t oddsockets_calculate_message_size(const char* message);

/**
 * Calculate message size with length
 * 
 * @param message The message to calculate size for
 * @param message_length The length of the message in bytes
 * @return Size in bytes, or 0 if message is NULL
 */
size_t oddsockets_calculate_message_size_with_length(const char* message, size_t message_length);

/**
 * Format byte size for human reading
 * 
 * Formats a byte size into a human-readable string (e.g., "1.5 KB", "32 bytes").
 * 
 * @param bytes Size in bytes
 * @param buffer Output buffer for formatted string
 * @param buffer_size Size of the output buffer
 * @return ODDSOCKETS_MESSAGE_SIZE_SUCCESS on success, error code on failure
 */
int oddsockets_format_byte_size(size_t bytes, char* buffer, size_t buffer_size);

/**
 * Get error message for message size error code
 * 
 * @param error Error code
 * @return Error message string
 */
const char* oddsockets_message_size_error_string(oddsockets_message_size_error_t error);

/**
 * Create a detailed error message for message size validation failure
 * 
 * Creates a detailed error message similar to the JavaScript SDK error messages.
 * 
 * @param actual_size The actual size of the message that failed validation
 * @param buffer Output buffer for the error message
 * @param buffer_size Size of the output buffer
 * @return ODDSOCKETS_MESSAGE_SIZE_SUCCESS on success, error code on failure
 */
int oddsockets_create_message_size_error_message(size_t actual_size, 
                                                char* buffer, 
                                                size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_MESSAGE_SIZE_VALIDATOR_H */
