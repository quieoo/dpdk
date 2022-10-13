#ifndef _LRU_H_
#define _LRU_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_BUCKET_MAX 1024
#define HASH_BUCKET_CAPACITY_MAX (256)
#define HASHTABLE_DEBUG
#define TRUE  1
#define FALSE 0

// #define DEBUG(format, ...) printf("[%s] [%d] : format\n", __FUNCTION__, __LINE__, __VA_ARGS__);

#define DEBUG(format, ...)  printf("[debug]%s:%d %s()| "format"\r\n",__FILE__,__LINE__,__FUNCTION__,##__VA_ARGS__)
struct hash_bucket {
    int                capacity;  /* 桶的容量 */
    void               *hkey;     /* hashtable的key */
    void               *hdata;    /* hashtable的data */
    struct hash_bucket *prev;
    struct hash_bucket *next;
    struct hash_bucket *tail;
};

typedef struct hash;

uint32_t string_hash_key(void* str, int key_length);
struct hash* hash_create(int key_length, int value_length);
int hash_lookup(struct hash* g_htable, void *key, void *value);
int hash_add(struct hash *g_htable, void *key, void* data);
int hash_delete(struct hash *g_htable, void *key);
void hash_destroy(struct hash *g_htable);
void hash_iter_print(struct hash *g_htable);



#define  bucket_free(bucket) \
    free(bucket->hkey);      \
    free(bucket->hdata);     \
    free(bucket);            \
    bucket = NULL;



#endif