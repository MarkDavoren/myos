#include "alloc.h"
#include "stdtypes.h"
#include "stdio.h"
#include "utility.h"

#define MAX_HEAP_NODES 50

typedef enum {
    USED,
    FREE,
    AVAILABLE
} HeapNodeState;

typedef struct HeapNode HeapNode;
struct HeapNode {
    HeapNodeState   stat;
    Uint8*          data;
    Uint32          size;
    HeapNode*       prev;
    HeapNode*       next;
};

HeapNode  heap[MAX_HEAP_NODES];
HeapNode* head = NULL;
Uint32    bytesFree = 0;

Bool heapInit(void* base, Uint32 size)
{
    if (size < sizeof(heap)) {
        printf("Space given to heap is too small: %d\n", size);
        return false;
    }

    base += sizeof(heap);
    size -= sizeof(heap);

    bytesFree = size;

    heap[0].stat = FREE;
    heap[0].data = base;
    heap[0].size = size;
    heap[0].prev = NULL;
    heap[0].next = NULL;

    head = &heap[0];

    for (int ii = 1; ii < MAX_HEAP_NODES; ++ii) {
        heap[ii].stat = AVAILABLE;
        heap[ii].data = NULL;
        heap[ii].size = 0;
        heap[ii].prev = NULL;
        heap[ii].next = NULL;
    }

    return true;
}

void* alloc(Uint32 size)
{
    HeapNode* hp = head;
    while (hp != NULL && (hp->stat != FREE || hp->size < size)) {
        hp = hp->next;
    }

    if (hp == NULL) {
        printf("alloc: Out of memory. Requested %d bytes. freeBytes = %d\n", size, bytesFree);
        panic("HEAP");
        return NULL;    // Never gets here
    }

    bytesFree -= size;

    if (hp->size == size) {
        hp->stat = USED;
        return hp->data;
    }

    /*
     * Need to split the block up
     * Create a new heap node to hold the remainder
     */

    // Fin an available heap node
    HeapNode* np = NULL;
    for (int ii = 0; ii < MAX_HEAP_NODES; ++ii) {
        if (heap[ii].stat == AVAILABLE) {
            np = &heap[ii];
            break;
        }
    }

    if (np == NULL) {
        printf("alloc: out of heap nodes\n");
        panic("HEAP");
        return NULL;    // Never gets here
    }

    // Give it the remainder and link it into the heap node list
    np->stat = FREE;
    np->data = hp->data + size;
    np->size = hp->size - size;
    np->prev = hp;
    np->next = hp->next;
    if (np->next) {
        np->next->prev = np;
    }

    hp->stat = USED;
    hp->size = size;
    hp->next = np;

    return hp->data;
}

void free(void* chunk)
{
    HeapNode* hp = head;
    while (hp != NULL && hp->data != chunk) {
        hp = hp->next;
    }

    if (hp == NULL) {
        printf("free: Attempted to free space not in the heap: %#x\n", chunk);
        panic("HEAP");
        return;    // Never gets here
    }

    bytesFree += hp->size;

    hp->stat = FREE;

    if (hp->next != NULL && hp->next->stat == FREE) {
        // Next node is free so merge it into this one and make this one free
        HeapNode* np = hp->next;

        hp->size += hp->next->size;
        hp->next = hp->next->next;
        if (hp->next != NULL) {
            hp->next->prev = hp;
        }

        // Reclaim the next node
        np->stat = AVAILABLE;
        // The following isn't necessary, but keeps the list clean for debugging
        np->size = 0;
        np->next = NULL;
        np->prev = NULL;

    }
    
    if (hp->prev != NULL && hp->prev->stat == FREE) {
        // Previous node is free so merge this one into it
        hp->prev->size += hp->size;
        hp->prev->next = hp->next;
        if (hp->next != NULL) {
            hp->next->prev = hp->prev;
        }

        // Reclaim the next node
        hp->stat = AVAILABLE;
        // The following isn't necessary, but keeps the list clean for debugging
        hp->size = 0;
        hp->next = NULL;
        hp->prev = NULL;

    } else {
        // Nothing to merge with so set this node free
        hp->stat = FREE;
    }
}

void printHeap()
{
    printf("Heap Dump: bytesFree = %#x\n", bytesFree);

    HeapNode* pp = NULL;
    for (HeapNode* hp = head; hp != NULL; pp = hp, hp = hp->next) {
        printf("data = %#x, size = %#x, state = %d\n", hp->data, hp->size, hp->stat);
        if (hp->prev != pp) {
            printf("Invalid prev pointer\n");
        }
    }
}
