#include <string.h>
#include <stdio.h>
#include "lisp.h"

static Element clone(Element);

/*
Should return Element for closure property.

If the elements to cons are pairs, we want the cons to hold their references
so if their data change, the cons does too. But if the elements are atoms
(strings, symbols, numbers, boolean), we want a copy of them.
*/
Element make_cons(const Element x, const Element y) {
  // these could be new, not yet on the stack nor reachable by prev stack exprs
  save(x);
  save(y);

  Pair *p = get_next_free_ptr();
  release(2);

  p->car = clone(x);
  p->cdr = clone(y);

  Element e = {
    .type = PAIR,
    .data.pair_ptr = p
  };

  return e;
}

Element clone(const Element x) {
  Element y = {
    .type = PAIR,
    .data.pair_ptr = NULL
  };

  switch (x.type) {
  // C's pass-by-value ensures numbers are always copied.
  case NUMBER:
  case BOOLEAN:
  // We copy the reference, which is what we want with non-atomic elements.
  case PAIR:
  // Primitives procedures are stored by the address of their functions.
  case PRIMITIVE_PROCEDURE:
  // Compounds procedures are same as Pairs.
  case COMPOUND_PROCEDURE:
    return x;
  // Since we store strings and symbol elements as pointers to manually
  // allocated memory, in order to truly copy them, we'll need to allocate
  // new memory.
  case SYMBOL:
    y.type = x.type;
    y.data.symbol = string_alloc(strlen(x.data.symbol));
    strcpy(y.data.symbol, x.data.symbol);
    return y;
  }
}

Element car(const Element x) {
  return x.data.pair_ptr->car;
}

Element cdr(const Element x) {
  return x.data.pair_ptr->cdr;
}

Boolean is_true(const Element x) {
  // Anything but a strict true is false.
  return x.type == BOOLEAN && x.data.truth;
}