#ifndef KVSTORE_H
#define KVSTORE_H

#include <stddef.h> /* For size_t */

#ifndef KVSTORE_CAPACITY
#define KVSTORE_CAPACITY 501
#endif
/*
 * Type for a user-provided destructor function for the store's values.
 * If set, it will be called whenever a value is overwritten or removed.
 */
typedef void (*kv_destroy_item_func)(void *value);

/*
 * Type for a user-provided constructor function for creating new items.
 * If set, it will be called to create a new value whenever a key is not found.
 */
typedef void *(*kv_create_item_func)(void);

/*
 * Opaque structure that holds the Key-Value store internals.
 */
typedef struct KeyValueStore KeyValueStore;

/*
 * Create a new key-value store.
 *
 * Parameters:
 *  - destructor: (optional) function for cleaning up values.
 *  - constructor: (optional) function for creating new values.
 *
 * Returns:
 *  - Pointer to a newly allocated KeyValueStore, or NULL if allocation fails.
 */
KeyValueStore *kvstore_create(kv_destroy_item_func destructor, kv_create_item_func constructor);

/*
 * Destroy an existing key-value store.
 * Frees all memory, and if a destructor was provided, calls it on every stored value.
 * If no destructor - just free value
 */
void kvstore_destroy(KeyValueStore *store);

/*
 * Insert or update a key-value pair.
 * If the key already exists, its old value is destroyed (if destructor is set)
 * and replaced by the new value.
 *
 * Parameters:
 *  - store: The key-value store.
 *  - key:   The string key (copied internally).
 *  - value: The value to store (pointer).
 *
 * Returns 0 on success, non-zero on error (e.g., memory allocation failure).
 */
int kvstore_set(KeyValueStore *store, const char *key, void *value);

/*
 * Retrieve the value associated with a given key.
 *
 * Returns the value pointer if the key is found, or NULL otherwise.
 */
void *kvstore_get(KeyValueStore *store, const char *key);

/*
 * Find or create an item by key.
 * If the key is found, returns its associated value.
 * If the key is not found, it creates a new value by calling the user-provided
 * constructor (if set), stores it, and returns it.
 *
 * If no constructor is set and the key is not found, returns NULL.
 */
void *kvstore_find_or_create(KeyValueStore *store, const char *key);

#endif /* KVSTORE_H */
