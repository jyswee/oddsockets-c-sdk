/**
 * Minimal JSON Parser Implementation
 * Handles flat objects and simple nested dot-notation lookups.
 * No external dependencies — pure C string parsing.
 */

#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_KEYS 64
#define MAX_KEY_LEN 128
#define MAX_VALUE_LEN 1024

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
} json_entry_t;

struct json_object {
    json_entry_t entries[MAX_KEYS];
    int count;
    char* raw; /* Original string for memory management */
};

/* Skip whitespace */
static const char* skip_ws(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Extract quoted string, returns pointer past closing quote */
static const char* extract_string(const char* p, char* out, size_t out_size) {
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++; /* Skip escape */
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

/* Parse flat/nested JSON into entries with dot-notation keys */
static void parse_object(const char* json, const char* prefix, json_object_t* obj) {
    const char* p = skip_ws(json);
    if (*p != '{') return;
    p++;

    while (*p && *p != '}' && obj->count < MAX_KEYS) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        /* Key */
        char key[MAX_KEY_LEN];
        p = extract_string(p, key, sizeof(key));
        if (!p) return;

        p = skip_ws(p);
        if (*p != ':') return;
        p++;
        p = skip_ws(p);

        /* Build full key with prefix */
        char full_key[MAX_KEY_LEN];
        if (prefix[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key);
        } else {
            strncpy(full_key, key, sizeof(full_key) - 1);
            full_key[sizeof(full_key) - 1] = '\0';
        }

        if (*p == '{') {
            /* Nested object — recurse */
            const char* start = p;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
            /* Parse nested with prefix */
            size_t nested_len = (size_t)(p - start);
            char* nested = malloc(nested_len + 1);
            if (nested) {
                memcpy(nested, start, nested_len);
                nested[nested_len] = '\0';
                parse_object(nested, full_key, obj);
                free(nested);
            }
        } else if (*p == '"') {
            /* String value */
            json_entry_t* e = &obj->entries[obj->count];
            strncpy(e->key, full_key, MAX_KEY_LEN - 1);
            p = extract_string(p, e->value, MAX_VALUE_LEN);
            if (p) obj->count++;
        } else {
            /* Number, bool, null */
            json_entry_t* e = &obj->entries[obj->count];
            strncpy(e->key, full_key, MAX_KEY_LEN - 1);
            size_t i = 0;
            while (*p && *p != ',' && *p != '}' && !isspace((unsigned char)*p) && i < MAX_VALUE_LEN - 1) {
                e->value[i++] = *p++;
            }
            e->value[i] = '\0';
            obj->count++;
        }
    }
}

json_object_t* json_parse(const char* json_string) {
    if (!json_string) return NULL;

    json_object_t* obj = calloc(1, sizeof(json_object_t));
    if (!obj) return NULL;

    obj->raw = strdup(json_string);
    parse_object(json_string, "", obj);
    return obj;
}

const char* json_get_string(json_object_t* obj, const char* key) {
    if (!obj || !key) return NULL;
    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            return obj->entries[i].value;
        }
    }
    return NULL;
}

int json_get_int(json_object_t* obj, const char* key, int default_value) {
    const char* val = json_get_string(obj, key);
    return val ? atoi(val) : default_value;
}

bool json_get_bool(json_object_t* obj, const char* key, bool default_value) {
    const char* val = json_get_string(obj, key);
    if (!val) return default_value;
    return (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
}

void json_free(json_object_t* obj) {
    if (!obj) return;
    free(obj->raw);
    free(obj);
}
