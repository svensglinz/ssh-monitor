#include "hash_map.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
// default hashing function
size_t default_hash(const void *key) {
    return (size_t) key;
}

// default comparator
int default_eq(const void *key1, const void *key2) {
    return key1 == key2;
}

hash_map *hash_map_resize(hash_map *map) {
    // double size
    size_t capacity_n = map->capacity << 1;

    // temporarily increases memory to capacity old + capacity new
    hash_node **buckets_n = calloc(capacity_n, sizeof(hash_node *));

    if (buckets_n == NULL) {
        fprintf(stderr, "Error while reallocating memory");
        exit(1);
    }

    // rehash all keys
    hash_node **bucket_old = map->buckets;
    size_t capacity_old = map->capacity;

    map->buckets = buckets_n;
    map->capacity = capacity_n;

    for (size_t i = 0; i < capacity_old; i++) {
        hash_node *node = bucket_old[i];

        while (node != NULL) {
            hash_node *tmp = node->next;
            hash_map_insert_node(map, node);
            node = tmp;
        }
    }
    map->free(bucket_old);
    map->max_size = floor(map->max_load_factor * (double) capacity_n);
    return map;
}

void hash_map_init(hash_map **map, const size_t elem_size, unsigned long (*hash_fn)(const void *),
                   int (*cmp_fn)(const void *, const void *),
                   const size_t capacity, const float max_load_factor, void* (*_malloc)(size_t size), void (*_free)(void* ptr)) {

    *map = malloc(sizeof(hash_map));

    // initialize all values to 0
    hash_node **buckets_n = calloc(capacity, sizeof(hash_node *));

    if (buckets_n == NULL) {
        fprintf(stderr, "Failed to allocate memory for hash map\n");
        exit(1);
    }

    // initialize fields
    (*map)->buckets = buckets_n;
    (*map)->max_load_factor = max_load_factor;
    (*map)->size = 0;
    (*map)->elem_size = elem_size;
    (*map)->max_size = floor((double) capacity * max_load_factor);
    (*map)->capacity = capacity;
    (*map)->hash_fn = hash_fn == NULL ? default_hash : hash_fn;
    (*map)->cmp_fn = cmp_fn == NULL ? default_eq : cmp_fn;

    // set allocators
    (*map)->malloc = _malloc == NULL? malloc : _malloc;
    (*map)->free = _free == NULL? free : _free;
}

hash_node *allocate_node(const hash_map *map, const void *pair, const size_t hash) {
    hash_node *node = map->malloc(sizeof(hash_node) + map->elem_size);

    if (node == NULL) {
        fprintf(stderr, "Error while allocating memory");
        exit(1);
    }

    node->pair = (void *) (node + 1);
    node->hash = hash;
    node->next = NULL;

    if (node->pair == NULL) {
        fprintf(stderr, "Error while allocating memory");
        exit(1);
    }
    memcpy(node->pair, pair, map->elem_size);
    return node;
}

void *hash_map_find_at(const hash_map *map, const void *pair, const size_t index) {
    const hash_node *node = map->buckets[index];
    if (node == NULL) {
        return NULL;
    }
    while (node != NULL) {
        if (map->cmp_fn(node->pair, pair)) {
            return node->pair;
        }
        node = node->next;
    }
    return NULL;
}

void *hash_map_insert(hash_map *map, const void *pair) {
    const size_t hash = map->hash_fn(pair);
    const size_t index = hash % map->capacity;
    hash_node *node = NULL;

    if (hash_map_find_at(map, pair, index)) {
        return NULL;
    }

    node = allocate_node(map, pair, hash);
    hash_node *head = map->buckets[index];
    map->buckets[index] = node;
    node->next = head;

    map->size++;
    // resize of load factor is exceeded
    if (map->size > map->max_size) {
#ifdef DEBUG
    printf("Resizing from Size: %lu, Capacity: %lu\n", map->size, map->capacity);
#endif
        hash_map_resize(map);
    }
    return node;
}

void hash_map_insert_node(const hash_map *map, hash_node *node) {
    const size_t index = node->hash % map->capacity;
    hash_node *head = map->buckets[index];

    if (head == NULL) {
        map->buckets[index] = node;
        node->next = NULL; // will be terminal
    } else {
        map->buckets[index] = node;
        node->next = head;
    }
}


void *hash_map_find(const hash_map *map, const void *pair) {
    const unsigned long hash = map->hash_fn(pair);
    const size_t index = hash % map->capacity;

    const hash_node *node = map->buckets[index];

    if (node == NULL) {
        return NULL;
    }

    while (node != NULL) {
        if (map->cmp_fn(node->pair, pair)) {
            return node->pair;
        }
        node = node->next;
    }
    return NULL;
}

// sets entries in array to NULL and frees all elements in the linked list
// size of allocated array stays the same
void hash_map_clear(hash_map *map) {
    for (size_t i = 0; i < map->capacity; i++) {
        hash_node *cur = map->buckets[i];

        while (cur != NULL) {
            hash_node *next = cur->next;
            map->free(cur);
            cur = next;
        }
    }
    map->size = 0;
}

void hash_map_free(hash_map *map) {
    hash_map_clear(map);
    map->free(map->buckets);
    map->free(map);
}

void hash_map_remove(hash_map *map, const void *key) {
    const unsigned long hash = map->hash_fn(key);
    const size_t index = hash % map->capacity;

    hash_node *cur = map->buckets[index];
    hash_node *prev = NULL;
    while (cur != NULL) {
        if (map->cmp_fn(key, cur->pair)) {
            if (prev != NULL) {
                prev->next = cur->next;
            } else {
                map->buckets[index] = cur->next;
            }
            map->free(cur);
            map->size--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}
