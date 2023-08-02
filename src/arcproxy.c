/*
   Copyright 20203 Joseph W. Seigh
   
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "arcproxy.h"

#include <assert.h>
#include <stdio.h>


//#define REF 1
/*
 *  0-15    ephimeral count
 * 16-31    link count
 *
*/
typedef uint32_t refcount_t;

static_assert(sizeof(arcref_t) == sizeof(refcount_t), "arcref_t not same size as refcount_t");

// reference count for adding to arcref_t
#define LREF 1 << 16

#define EPHEMERAL_MASK 0xffff0000

// ref node index mask
#define NDX_MASK 0x0000ffff

typedef struct arcnode_t
{
    refcount_t  count;

    void        *obj;           // object to be deallocated
    void (*dealloc)(void *);    // object deallocation function

    struct arcnode_t *next;     // pool pointer, queue  pointer

    uint32_t    ndx;            // index in node array


} arcnode_t;


typedef struct arcproxy_t
{
    arcref_t    tail;

    uint32_t    size;
    arcnode_t   *nodes;

    arcnode_t   *pool;

} arcproxy_t;


/**
 * Extract ephemeral reference count from ref
 * 
 * @param ref reference
 * @returns reference count w/ ephemeral count set
*/
static inline refcount_t ref2count(arcref_t ref)
{
    return ref & EPHEMERAL_MASK;
}

/**
 * index or arcref_t to node address
*/
static inline arcnode_t *ndx2node(arcproxy_t *proxy, arcref_t ndx)
{
    ndx &= NDX_MASK;
    return &proxy->nodes[ndx];
}

static arcnode_t *pop(arcproxy_t *proxy);
static void push(arcproxy_t *proxy, arcnode_t *node);


arcproxy_t *arcproxy_create(uint32_t size)
{

    if (size < 1 || size >= INT16_MAX)
        return NULL;

    arcproxy_t *proxy = malloc(sizeof(arcproxy_t));
    memset(proxy, 0, sizeof(arcproxy_t));
    //proxy->tail = 0;
    proxy->size = size;

    size_t sz = sizeof(arcnode_t) * size;
    arcnode_t *nodes = malloc(sz);
    memset(nodes, 0, sz);
    proxy->nodes = nodes;
    proxy->pool = NULL;
    for (int ndx = 1; ndx < size; ndx++)
    {
        nodes[ndx].ndx = ndx;
        nodes[ndx].next = proxy->pool;
        proxy->pool = &nodes[ndx];
    }

    nodes[0].count = 1;     // initial link count
    proxy->tail = 0;        // --> nodes[0]

     

    return proxy;
}

void arcproxy_destroy(arcproxy_t *proxy)
{

    for (int ndx = 0; ndx < proxy->size; ndx++)
    {
        arcnode_t *node = &proxy->nodes[ndx];
        if (node->obj == NULL)
            continue;

        unsigned int ecount = node->count >> 16;
        unsigned int count = node->count & 0xffff;
        fprintf(stderr, "%d) %p - %d %p  count (%d, %d)\n",
            ndx,
            node,
            node->ndx,
            node->next,
            ecount, count,
            1);
    }
    arcnode_t *tnode = ndx2node(proxy, proxy->tail);
    unsigned int ecount = tnode->count >> 16;
    unsigned int count = tnode->count & 0xffff;
    fprintf(stderr, "tail@%p tail.ecount=%d ndx=%d ecount=%d count=%d\n",
        tnode,
        proxy->tail >> 16,
        tnode->ndx,
        ecount, count);

    free(proxy->nodes);
    free(proxy);
}

arcref_t arcproxy_ref_acquire(arcproxy_t *proxy)
{
    return atomic_fetch_add_explicit(&proxy->tail, LREF, memory_order_acquire);
}

void arcproxy_ref_release(arcproxy_t *proxy, arcref_t ref)
{
    arcnode_t *node =  ndx2node(proxy, ref);

    refcount_t dropcount = LREF; // drop reference
    for (;;)
    {
        refcount_t rc = atomic_fetch_sub_explicit(&node->count, dropcount, memory_order_release);
        if (rc != dropcount)
            break;



        if (node->count != 0) {
            fprintf(stderr, "node: count=%d ndx=%d -- ref=%08lx\n", node->count, node->ndx, ref);
        }
        assert(node->count == 0);



        node->dealloc(node->obj);
        node->obj = NULL;
        node->dealloc = NULL;

        arcnode_t *next = node->next;
        push(proxy, node);
        node = next;
        dropcount = 1;   // drop link from previous node
    }
}

/**
 * Get a node from node pool.
 * Requires a reference acquire to prevent ABA problem
 * 
*/
static arcnode_t *pop(arcproxy_t *proxy)
{
    arcnode_t *node;
    do
    {
        node = proxy->pool; // atomic
    }
    while(node != NULL && !atomic_compare_exchange_strong_explicit(&proxy->pool, &node, node->next, memory_order_acquire, memory_order_relaxed));
    return node;
}

static void push(arcproxy_t *proxy, arcnode_t *node)
{
    arcnode_t *node2;
    do
    {
        node2 = atomic_load_explicit(&proxy->pool, memory_order_acquire);
        node->next = node2;
    }
    while (!atomic_compare_exchange_strong_explicit(&proxy->pool, &node2, node, memory_order_release, memory_order_relaxed));
}

bool arcproxy_retire(arcproxy_t *proxy, void *obj, void (*dealloc)(void *))
{

    arcref_t ref = arcproxy_ref_acquire(proxy);     // MTTT

    arcnode_t *node = pop(proxy);
    if (node == NULL)
    {
        arcproxy_ref_release(proxy, ref);           // MTTT
        return false;
    }

    node->count = 2;                // initial link count -- tail + prevnode
    node->obj=NULL;
    node->dealloc=NULL;
    node->next=NULL;

    arcref_t ref2 = node->ndx;
    arcref_t prevref = atomic_exchange_explicit(&proxy->tail, ref2, memory_order_acquire);

    arcnode_t  *prevnode = ndx2node(proxy, prevref);
    prevnode->obj = obj;
    prevnode->dealloc = dealloc;
    prevnode->next = node;

    atomic_fetch_add(&prevnode->count, ref2count(prevref) - 1);

    arcproxy_ref_release(proxy, ref);              // MTTT

    return true;
}