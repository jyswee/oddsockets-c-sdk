/**
 * Message Validator for OddSockets C SDK
 * Validates message size against the 32KB industry standard limit.
 */

#ifndef ODDSOCKETS_MESSAGE_VALIDATOR_H
#define ODDSOCKETS_MESSAGE_VALIDATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "oddsockets.h"

/* Re-export the validate function from oddsockets.h */
/* oddsockets_validate_message_size() is implemented in oddsockets.c */

#ifdef __cplusplus
}
#endif

#endif /* ODDSOCKETS_MESSAGE_VALIDATOR_H */
