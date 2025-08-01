#include "kvstore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Simple linked list node that holds a key-value pair along with its hash.
 */
typedef struct KVNode {
    char*          key;
    unsigned long  hash; /* Cached hash value */
    void*          value;
    struct KVNode* next;
} KVNode;

/*
 * Actual internal representation of the key-value store.
 */
struct KeyValueStore {
    KVNode**             buckets;
    size_t               capacity;
    kv_destroy_item_func destructor;
    kv_create_item_func  constructor;
};

/* Optimized hash function: djb2 by Dan Bernstein */
static unsigned long
optimized_hash(const char* str) {
    unsigned long hash = 5381;
    int           c;
    while ((c = (unsigned char) *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/* Helper to create a copy of a string */
static char*
kv_strdup(const char* src) {
    if (!src)
        return NULL;
    size_t len = strlen(src) + 1;
    char*  dst = (char*) malloc(len);
    if (dst) {
        memcpy(dst, src, len);
    }
    return dst;
}

/*
 * Create a new KeyValueStore with the specified capacity and optional destructor/constructor.
 */
KeyValueStore*
kvstore_create(
    kv_destroy_item_func destructor,
    kv_create_item_func  constructor) {
    KeyValueStore* store = (KeyValueStore*) malloc(sizeof(KeyValueStore));
    if (!store) {
        return NULL;
    }
    store->buckets = (KVNode**) calloc(KVSTORE_CAPACITY, sizeof(KVNode*));
    if (!store->buckets) {
        free(store);
        return NULL;
    }
    store->capacity    = KVSTORE_CAPACITY;
    store->destructor  = destructor;
    store->constructor = constructor;
    return store;
}

/*
 * Free all nodes in the chain.
 * If a destructor is provided, call it for each value.
 */
static void
free_bucket_chain(KVNode* node, kv_destroy_item_func destructor) {
    while (node) {
        KVNode* next = node->next;
        if (destructor) {
            destructor(node->value);
        } else {
            free(node->value);
        }
        free(node->key);
        free(node);
        node = next;
    }
}

/*
 * Destroy the entire store.
 */
void
kvstore_destroy(KeyValueStore* store) {
    if (!store)
        return;
    for (size_t i = 0; i < store->capacity; i++) {
        if (store->buckets[i]) {
            free_bucket_chain(store->buckets[i], store->destructor);
        }
    }
    free(store->buckets);
    free(store);
}

/*
 * Insert or replace a key-value pair in the store.
 */
int
kvstore_set(KeyValueStore* store, const char* key, void* value) {
    if (!store || !key)
        return -1;

    unsigned long hash  = optimized_hash(key);
    size_t        index = hash % store->capacity;

    KVNode* cur  = store->buckets[index];
    KVNode* prev = NULL;

    /* Search for existing key */
    while (cur) {
        if (cur->hash == hash && strcmp(cur->key, key) == 0) {
            /* Key already exists: replace value */
            if (store->destructor) {
                store->destructor(cur->value);
            }
            cur->value = value;
            return 0;
        }
        prev = cur;
        cur  = cur->next;
    }

    /* Not found, create a new node */
    KVNode* new_node = (KVNode*) malloc(sizeof(KVNode));
    if (!new_node) {
        return -1;
    }
    new_node->key = kv_strdup(key);
    if (!new_node->key) {
        free(new_node);
        return -1;
    }
    new_node->hash  = hash;
    new_node->value = value;
    new_node->next  = NULL;

    /* Append to the chain */
    if (prev) {
        prev->next = new_node;
    } else {
        store->buckets[index] = new_node;
    }

    return 0;
}

/*
 * Get the value for a given key.
 */
void*
kvstore_get(KeyValueStore* store, const char* key) {
    if (!store || !key)
        return NULL;

    unsigned long hash  = optimized_hash(key);
    size_t        index = hash % store->capacity;

    KVNode* cur = store->buckets[index];
    while (cur) {
        if (cur->hash == hash && strcmp(cur->key, key) == 0) {
            return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

/*
 * Find or create an item by key.
 * If the key is not found, calls the constructor (if available) to create it,
 * then sets it in the store.
 */
void*
kvstore_find_or_create(KeyValueStore* store, const char* key) {
    if (!store || !key)
        return NULL;

    /* Try to get existing value */
    void* existing = kvstore_get(store, key);
    if (existing) {
        return existing;
    }

    /* If not found, see if we have a constructor */
    if (!store->constructor) {
        /* No constructor, cannot create */
        return NULL;
    }

    /* Create a new value and store it */
    void* new_value = store->constructor();
    if (new_value) {
        if (kvstore_set(store, key, new_value) != 0) {
            /* Setting failed, clean up if needed */
            if (store->destructor) {
                store->destructor(new_value);
            }
            return NULL;
        }
    }
    return new_value;
}
