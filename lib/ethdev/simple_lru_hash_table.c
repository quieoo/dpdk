#include "simple_lru_hash_table.h"
#include <rte_jhash.h>

// static struct hash *g_htable = NULL;



struct hash {
    uint32_t            (*hash_key)(void *, int);
    int                 (*hash_cmp)(const void *, const void*, int len);
    struct hash_bucket* bucket[HASH_BUCKET_MAX];
    int key_length;
};

uint32_t string_hash_key(void* str, int key_length) {
    uint32_t result=rte_jhash(str, key_length, 0);
    
    if(key_length==4){
        uint32_t* ptr=str;
        DEBUG("hash key: %x, index : %d", *ptr, result%HASH_BUCKET_MAX);
    }else if(key_length==6){
        uint8_t* ptr=str;
        DEBUG("hash key: %x-%x-%x-%x-%x-%x, index: %d",
        ptr[0],
        ptr[1],
        ptr[2],
        ptr[3],
        ptr[4],
        ptr[5],
        result%HASH_BUCKET_MAX
        );
    }
    return result%HASH_BUCKET_MAX;
}


int string_hash_cmp(const void* src, const void* dst, int len) {
    if (!src || !dst) {
        //DEBUG("src addr: %p, dst addr: %p", src, dst);
        return -1;
    }
    return strncmp((char *)src, (char *)dst, len);
}
struct hash* hash_create(int key_length) {
    struct hash *g_htable = (struct hash *)malloc(sizeof(struct hash));
    if (!g_htable) {
        DEBUG("memory alloc failed.");
        return NULL;
    }

    memset(g_htable, 0, sizeof(struct hash));

    g_htable->hash_key = string_hash_key;
    g_htable->hash_cmp = string_hash_cmp;
    g_htable->key_length=key_length;

    return g_htable;
}

static void bucket_delete(struct hash_bucket** ptr) {
    struct hash_bucket *bucket = *ptr;
    struct hash_bucket *tmp;

    while(bucket) {
        tmp = bucket;
        bucket = bucket->next;
        bucket_free(tmp);
    }
}

void hash_destroy(struct hash *g_htable) {
    if (g_htable) {
        for(int i=0; i<HASH_BUCKET_MAX; i++) {
            if (g_htable->bucket[i]) {
                bucket_delete(&g_htable->bucket[i]);
            }
        }

        free(g_htable);
        g_htable = NULL;
    }
    return;
}

#define  lru_bucket_move(bucket,  head)   \
    bucket->next = head;                  \
    bucket->prev = NULL;                  \
    bucket->capacity = head->capacity;    \
                                          \
    head->prev = bucket;                  \
    head->tail = NULL;

int hash_lookup(struct hash* g_htable, void *key, void *value) {
    if (!key) {
        DEBUG("input para is NULL\n");
        return -1;
    }

    uint32_t index = g_htable->hash_key(key, g_htable->key_length);
    struct hash_bucket* head = g_htable->bucket[index];
    struct hash_bucket* bucket = head;

    while(bucket) {
        if (0 == g_htable->hash_cmp(key, bucket->hkey, strlen((char*)key))) {
            if (head != bucket && bucket != head->tail) {
                bucket->prev->next = bucket->next;
                bucket->next->prev = bucket->prev;
                bucket->tail = head->tail;

                lru_bucket_move(bucket, head);
            } else if (bucket == head->tail && head->capacity>1) {
                bucket->prev->next = NULL;
                bucket->tail = bucket->prev;

                lru_bucket_move(bucket, head);
            }
            g_htable->bucket[index] = bucket;
            memcpy(value, bucket->hdata, sizeof(bucket->hdata));
            return 0;
        }
        bucket = bucket->next;
    }
    return -1;
}

int hash_add(struct hash *g_htable, void *key, void* data) {
    if (!key || !data) {
        DEBUG("input para is NULL\n");
        return FALSE;
    }

    uint32_t index = g_htable->hash_key(key,g_htable->key_length);
    struct hash_bucket* head = g_htable->bucket[index];
    if (!head) {
        head = (struct hash_bucket*)malloc(sizeof(struct hash_bucket));
        if (!head) {
            DEBUG("no memory for more hash_bucket\n");
            return FALSE;
        }

        memset(head, 0, sizeof(*head));
        head->capacity++;

        head->hkey  = strdup((char *)key);
        head->hdata = strdup((char *)data);
        head->tail  = head;
        g_htable->bucket[index] = head;
        return TRUE;
    }

    int capacity = head->capacity;
    struct hash_bucket *new_bucket =
        (struct hash_bucket *)malloc(sizeof(struct hash_bucket));

    if (!new_bucket) {
        DEBUG("no memory for more hash_bucket\n");
        return FALSE;
    }

    if (capacity >= HASH_BUCKET_CAPACITY_MAX) {
        struct hash_bucket *tail = head->tail;
        head->tail = tail->prev;

        tail->prev->next = NULL;
        bucket_free(tail);
    }

    head->prev = new_bucket;
    new_bucket->next = head;
    new_bucket->capacity = capacity + 1;
    new_bucket->tail = head->tail;
    head->tail = NULL;

    new_bucket->hkey  = strdup((char *)key);
    new_bucket->hdata = strdup((char *)data);

    g_htable->bucket[index] = new_bucket;

    return TRUE;
}

int hash_delete(struct hash *g_htable, void *key) {
    if (!key) {
        DEBUG("input para is NULL\n");
        return FALSE;
    }

    uint32_t index = g_htable->hash_key(key,g_htable->key_length);
    struct hash_bucket* head = g_htable->bucket[index];
    struct hash_bucket* bkt = head;

    while(bkt) {
        if (0 == g_htable->hash_cmp(key, bkt->hkey, strlen((char*)key))) {
            if (head != bkt && bkt != head->tail) {
                bkt->prev->next = bkt->next;
                bkt->next->prev = bkt->prev;

            } else if (bkt == head->tail && head->capacity>1) {
                bkt->prev->next = NULL;
                bkt->tail = bkt->prev;

            } else {
                if (bkt->next) {
                    bkt->next->tail = bkt->tail;
                    bkt->next->capacity = bkt->capacity;
                    bkt->next->prev = NULL;
                    g_htable->bucket[index] = bkt->next;
                } else {
                    g_htable->bucket[index] = NULL;
                }
            }

            bucket_free(bkt);
            if (g_htable->bucket[index]) {
                g_htable->bucket[index]->capacity--;
            }

            return TRUE;
        }
        bkt = bkt->next;
    }
    return FALSE;
}

static void bucket_print(struct hash_bucket** ptr) {
    struct hash_bucket *bkt = *ptr;
    struct hash_bucket *tmp;

    while(bkt) {
        printf("key=[%s],data=[%s]\n", (char*)bkt->hkey, (char*)bkt->hdata);
        bkt = bkt->next;
    }
}

void hash_iter_print(struct hash *g_htable) {
    if (g_htable) {
        for(int i=0; i<HASH_BUCKET_MAX; i++) {
            if (g_htable->bucket[i]) {
                bucket_print(&g_htable->bucket[i]);
            }
        }
    }
}
