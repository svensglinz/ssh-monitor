//
// Created by sven on 10/2/24.
//

#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdio.h>

typedef struct hash_node {
    struct hash_node* next;
    void* pair;
    size_t hash;
} hash_node;

typedef struct hash_map {
    size_t size;
    size_t max_size;
    double max_load_factor;
    size_t capacity;
    hash_node **buckets;
    size_t elem_size;
    size_t (*hash_fn)(const void *); // optional custom hash function
    int (*cmp_fn)(const void *, const void *); // optional custom comparator
    void* (*malloc)(size_t);
    void (*free)(void *);
} hash_map;

void hash_map_init(hash_map **map, size_t elem_size, const unsigned long (*hash_fn)(const void *),
                   const int (*cmp_fn)(const void *, const void *), size_t capacity, float max_load_factor, void* (*)(size_t), void (*)(void*));

void hash_map_free(hash_map *map);

void hash_map_clear(hash_map *map);

void *hash_map_insert(hash_map *map, const void* pair);

void hash_map_insert_node(const hash_map* map, hash_node* node);

void *hash_map_find(const hash_map *map, const void *pair);

void hash_map_remove(hash_map *map, const void *pair);

#endif
