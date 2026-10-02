#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lisp.h"

static Element list_of_values(Element, Element);
static Element make_procedure(Element, Element);
static Element eval_definition(Element, Element);

static Boolean self_evaluating(Element);
static Boolean variable(Element);
static Boolean application(Element);
static Boolean special_form(char *, Element);

// Abstractions around car/cdr
static Element operator(Element);
static Element operands(Element);
static Element procedure_parameters(Element);
static Element procedure_body(Element);
static Element procedure_environment(Element);
static Element text_of_quotation(Element);
static Element definition_variable(Element);
static Element definition_expression(Element);
static Element predicate(Element);
static Element consequent(Element);
static Element alternative(Element);

Element eval_dispatch(Element exp, Element env) {
  do {
    // These cases don't allocate memory, so GC won't get triggered here, so
    // we don't need to save pointers.
    if (self_evaluating(exp))
      return exp;

    if (variable(exp))
      return lookup_variable_value(exp.data.symbol, env);

    if (special_form(QUOTE, exp))
      return text_of_quotation(exp);

    // The rest do though.
    save(exp);
    save(env);

    if (special_form(DEFINE, exp)) {
      Element result = eval_definition(exp, env);
      release(2);
      return result;

    } else if (special_form(IF, exp)) {
      Element conditional = eval_dispatch(predicate(exp), env);
      release(2);

      exp = is_true(conditional) ? consequent(exp) : alternative(exp);
      continue;

    } else if (special_form(LAMBDA, exp)) {
      Element result = make_procedure(exp, env);
      release(2);
      return result;

    } else if (application(exp)) {
      Element procedure = eval_dispatch(operator(exp), env);
      save(procedure);

      Pair *arguments = list_of_values(operands(exp), env).data.pair_ptr;
      release(3);

      if (procedure.type == PRIMITIVE_PROCEDURE)
        return (*procedure.data.func_ptr)(arguments);

      if (procedure.type == COMPOUND_PROCEDURE) {
        // Apply the function
        Element params = procedure_parameters(procedure);
        Element proc_env = procedure_environment(procedure);
        Element proc_body = procedure_body(procedure);
        save(procedure);

        Element arg_bindings = make_frame(
                params,
                (Element) {
                        .type = PAIR,
                        .data.pair_ptr = arguments
                }
        );

        Element proc_env_with_args = make_cons(arg_bindings, proc_env);
        release(1);

        // Evaluate the sequence of expressions in the function
        // Check if there's more expressions after the head.
        while (cdr(proc_body).data.pair_ptr) {
          save(proc_body);
          save(proc_env_with_args);

          eval_dispatch(car(proc_body), proc_env_with_args);
          release(2);

          proc_body = cdr(proc_body);
        }

        exp = car(proc_body);
        env = proc_env_with_args;
        continue;
      }

      // Not a procedure. TODO: Print operator.
      fprintf(stderr, "Not a procedure.\n");

      // TODO: we shouldn't really return NIL, we should just exit. But to keep the REPL going, we'll try and avoid exiting, so we need to return some value.
      return NIL;
      //exit(NOT_PROCEDURE);
    }
    // TODO: Add cond, and, or, let, assignment cases
    // Warning, we aren't release memory if these above cases aren't true.
    // Currently fine since we error out here.

    // TODO: Let print_element print to stderr.
    fprintf(stderr, "Unknown expression type.\n");
    exit(BAD_EXPRESSION);
  } while (TRUE);
}

Element list_of_values(const Element operands, const Element env) {
  if (!operands.data.pair_ptr) {
    return (Element){
      .type = PAIR,
      .data.pair_ptr = NULL
    };
  }

  Element first_value = eval_dispatch(car(operands), env);
  save(first_value);

  Element rest = list_of_values(cdr(operands), env);
  release(1);

  return make_cons(first_value, rest);
}

// Modifies the environment to insert a new binding.
Element eval_definition(const Element exp, const Element env) {
  Element def_name = definition_variable(exp);
  Element def_exp = definition_expression(exp);
  Element def_val = eval_dispatch(def_exp, env);

  Element binding = make_cons(def_name, def_val);

  // TODO: Add check to see if variable exists in first frame.
  // If we had our own version of set-car!, we could use it here.
  env.data.pair_ptr->car = make_cons(binding, first_frame(env));

  return NIL;
}

Element make_procedure(const Element exp, const Element env) {
  return (Element){
    .type = COMPOUND_PROCEDURE,
    .data.pair_ptr = make_cons(
      car(cdr(exp)),  // lambda parameters
      make_cons(cdr(cdr(exp)), env  // lambda body and env
      )
    ).data.pair_ptr
  };
}

Boolean self_evaluating(const Element exp) {
  // TODO: Check if string, bools too.
  return (
    exp.type == NUMBER ? TRUE :
    exp.type == PAIR && exp.data.pair_ptr == NULL ? TRUE :
    FALSE
  );
}

Boolean variable(const Element exp) {
  return exp.type == SYMBOL;
}

Boolean application(const Element exp) {
  return exp.type == PAIR;
}

Boolean special_form(char *symbol, const Element exp) {
  return (
    exp.type == PAIR &&
    car(exp).type == SYMBOL &&
    strcmp(car(exp).data.symbol, symbol) == 0
  );
}

Element operator(const Element exp) { return car(exp); }
Element operands(const Element exp) { return cdr(exp); }

// Gets the abc in (quote abc)
Element text_of_quotation(const Element exp) { return car(cdr(exp)); }

// Here, exp looks like this: ((x) (+ x 1) env)
Element procedure_parameters(const Element exp) { return car(exp); }
Element procedure_body(const Element exp) { return car(cdr(exp)); }
Element procedure_environment(const Element exp) { return cdr(cdr(exp)); }

// Gets the x or (+ 1 2) in (define x (+ 1 2))
Element definition_variable(const Element exp) { return car(cdr(exp)); }
Element definition_expression(const Element exp) { return car(cdr(cdr(exp))); }

// Gets the p/c/a in (if p c a)
Element predicate(const Element exp) { return car(cdr(exp)); }
Element consequent(const Element exp) { return car(cdr(cdr(exp))); }
Element alternative(const Element exp) { return car(cdr(cdr(cdr(exp)))); }
