#include <stdio.h>
#include <stdlib.h>
#include "lisp.h"

#define HEAP_LIMIT 1500  // Currently, a clean env takes up around 18 addresses.
#define STACK_LIMIT 1000

// Array of pair addresses (not array of pairs)
static Pair *heap1[HEAP_LIMIT];
static Pair *heap2[HEAP_LIMIT];
static Pair **curr_heap = heap1;
static int index_h = 0;  // Index of next free slot

// An analogous array to heap to track if we should keep element at array
// index during GC.
static Boolean keep[HEAP_LIMIT] = {FALSE};

// This is the Lisp-level stack used to store the env for each Lisp
// stack frame. The GC uses this stack to know which roots to keep.
// Array of pair addresses (not array of pairs).
static Element stack[STACK_LIMIT];
static int index_s = 0;  // Index of next free slot

static Boolean already_deleted(void *);
static void mark_to_keep(Pair *);
static void add_to_deleted(void *);
static void cleanup_pair(Pair *);
static void reset_deleted(void);
static void gc(void);

// We'll use this to keep a running list of deleted addresses
struct linked_list_node {
  void *value;  // Holds pointer.
  struct linked_list_node *next;
};

typedef struct linked_list_node AddressNode;

static AddressNode *deleted = NULL;

char *string_alloc(int n) {
  return (char *) calloc(1, n + 1);
}
 
Pair *get_next_free_ptr(void) {
  Pair *p = calloc(1, sizeof(Pair));

  if (index_h == HEAP_LIMIT)
    gc();

  if (index_h == HEAP_LIMIT) {
    fprintf(stderr, "Heap overflow: heap height: %d\n", index_h);
    exit(HEAP_OVERFLOW);
  }

  curr_heap[index_h] = p;
  index_h += 1;

  return p;
}

// For GC, need to use all pairs saved on stack as roots to traverse from.
// Anything that the roots can reach, we need to save.
void gc(void) {
  // Mark addresses referenced by stack.
  for (int i = 0; i < HEAP_LIMIT; i++)
    for (int j = 0; j < index_s; j++)
      if (
        stack[j].type == PAIR ||
        stack[j].type == COMPOUND_PROCEDURE
      ) {
        if (curr_heap[i] == stack[j].data.pair_ptr)
          mark_to_keep(curr_heap[i]);
      }

  // Sweep phase
  index_h = 0;
  Pair **next_heap = curr_heap == heap1 ? heap2 : heap1;

  for (int i = 0; i < HEAP_LIMIT; i++) {
    Pair *p = curr_heap[i];
    if (keep[i]) {
      // Copy address to other heap.
      next_heap[index_h] = p;
      index_h += 1;
    }
    else
      cleanup_pair(p);
  }

  // Swap heaps.
  curr_heap = next_heap;
  // Reset marks.
  for (int i = 0; i < HEAP_LIMIT; i++)
    keep[i] = FALSE;

  // Reset list of deleted addresses too.
  reset_deleted();
}

void reset_deleted(void) {
  AddressNode *curr = deleted;
  AddressNode *next;

  // Traverse to last node.
  while (curr) {
    next = curr->next; // Save reference to next node.
    free(curr);
    curr = next;
  }

  deleted = NULL;
}

void mark_to_keep(Pair *p) {
  if (p)
    for (int i = 0; i < HEAP_LIMIT; i++)
      if (curr_heap[i] == p && !keep[i]) {
        keep[i] = TRUE;

        // This is icky since compound procedures are still pairs underneath.
        // How can this be improved?
        if (p->car.type == PAIR || p->car.type == COMPOUND_PROCEDURE)
          mark_to_keep(p->car.data.pair_ptr);
    
        if (p->cdr.type == PAIR || p->cdr.type == COMPOUND_PROCEDURE)
          mark_to_keep(p->cdr.data.pair_ptr);
      }
}

Boolean already_deleted(void *ptr) {
  AddressNode *curr = deleted;

  while (curr != NULL) {
    if (curr->value == ptr)
      return TRUE;
    curr = curr->next;
  }
  return FALSE;
}

void add_to_deleted(void *ptr) {
  AddressNode *curr = deleted;

  // Create first node if no nodes yet.
  if (!deleted) {
    deleted = calloc(1, sizeof(AddressNode));
    deleted->value = ptr;
  } else {
    AddressNode *prev;

    // Traverse to last node.
    while (curr) {
      prev = curr;
      curr = curr->next;
    }

    // Then add new address as new node
    prev->next = calloc(1, sizeof(AddressNode));
    prev->next->value = ptr;
  }
}

void cleanup_pair(Pair *p) {
  if (p) {
    Element pcar = p->car;
    Element pcdr = p->cdr;

    // Check that it hasn't been freed yet.
    if (!already_deleted(p)) {
      free(p);
      add_to_deleted(p);
    }

    cleanup_element(pcar);
    cleanup_element(pcdr);
  }
}

void cleanup_element(Element e) {
  if (e.type == SYMBOL)
    if (!already_deleted(e.data.symbol)) {
      free(e.data.symbol);
      add_to_deleted(e.data.symbol);
    }
  // No other cleanup needed except those that needed malloc.
}

void save(const Element e) {
  if (index_s == STACK_LIMIT) {
    fprintf(stderr, "Stack overflow: stack height: %d\n", index_s);
    exit(STACK_OVERFLOW);
  }

  // TODO: not all are pairs, evaluate impact
  stack[index_s] = e;
  index_s += 1;
}

// Be very careful that this doesn't decrement to below 0, else it modifies
// other data!
void release(int i) {
  index_s -= i;
}