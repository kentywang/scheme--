# SCHEME--

Demo here: [scheme-minus-minus.netlify.app](https://scheme-minus-minus.netlify.app)

A Scheme interpreter developed from scratch in C. Scheme-- runs as a 
read-evaluate-print loop. It maintains a virtual stack and a virtual 
heap that store addresses to Lisp pairs.

The reader parses live user input into a linked list composed of Lisp
pairs, which is then fed to the evaluator. The evaluator also builds
its output as a list, which is ultimately passed to the printer to be
printed. 

During the read phase and the evaluate phase, space is dynamically
allocated on the heap in C to store newly-created pairs. These
addresses are then stored on the statically-allocated virtual heap in
Scheme--.

The heap is partitioned into a working side and free side so that
when the working side is full, its useful pairs are copied to the
free side and the roles of the two sides swap. A pair is useful only
if it can affect the outcome of current or future evaluation. The
virtual stack maintains the addresses of all the root pairs that
evaluation may need so that the garbage collector can use those pairs
as starting nodes to traverse and mark all its lineage as useful. The
addresses on the virtual heap that aren't marked are then freed from
memory in C.

Tail-call optimization is handled by a while-loop implementation of
the evaluation procedure, which I thought preferable over relying on
the finicky nature of C compilers to tail-call optimize and also
preferable over a lower-level modeling of the execution flow using
registers and call stacks (Scheme--'s stack is only used for
preserving intermediate values when GCing).

### Overview of features
#### Quotations and proper parsing & printing
```
λ 》(list 'apple 2 3)
(apple 2 3)


λ 》    '(1   2 (a b   ''(x y))      3        4    )
(1 2 (a b (quote (quote (x y)))) 3 4)
```
#### Definitions, lambda expressions and conditionals
```
λ 》(define id (lambda (x) x))
nil

λ 》(id id)
#<procedure>


λ 》(define kons (lambda (x y) (lambda (m) (m x y))))
nil

λ 》(define kar (lambda (z) (z (lambda (p q) p))))
nil

λ 》(kar (kons 'a 'b))
a


λ 》(define enum-interval
      (lambda (start end)
        (if (= start end)
            (list start)
            (cons start
                  (enum-interval (+ 1 start)
                                 end)))))
nil
                                 
λ 》(enum-interval 1 10)
(1 2 3 4 5 6 7 10)


λ 》(((lambda (x) (x x))
      (lambda (fact-gen)
        (lambda (n)
          (if (= 0 n)
              1
              (* n ((fact-gen fact-gen) (+ n -1)))))))
     9)
362880
```
#### Closures
```
λ 》(define x (lambda () k))
    (define k 5)
    (x)
5
```
#### Tail-call optimization
```
λ 》(define count-to-a-million
      (lambda (x) 
        (if (= x 999999) 
            'done 
            (count-to-a-million (+ x 1)))))
nil

λ 》(count-to-a-million 0)
done
```
#### Others
- Cleanroom garbage collection implementation (mark-and-sweep)
- Verbose option for tracing evaluation (WIP)

### Lessons learned
- Creating a parser was quite a task on its own. Even for that alone I'm
  proud of my work.
- Creating a garbage collector was even more challenging, but I learned a lot
  about memory allocation and the expected lifecycle of data during a REPL
  cycle.
- There's three things to consider: not freeing everything (memory leak),
  freeing things twice (easy to notice), and freeing too much (like what I
  experienced when I freed intermediate values used during eval).
- Freeing too much happens when I don't capture all the roots to mark to keep.
- Every malloc needs an accompanying free, ideally in the same function.
- But that only solves the memory leak issue; it doesn't address the
  freeing too much issue.
- To solve that, I need to make sure we preserve all intermediate values so
  they don't get freed during GC. One way to do that is before every malloc
  we make sure to push our current work to the stack.
- The pattern I've found useful is for each function to be responsible for
  preserving its own intermediate values to prevent GC from taking them, 
  but they needn't have any obligation to preserve those values once it
  passes them to another function. It's up to the next function to
  preserve them (if it needs them). Simultaneously, calling functions must
  assume its called functions that involve allocating new memory will destroy
  any data in unsaved addresses during the function execution.
- Using calloc is a good idea if I don't want to use memory addresses with
  random initial data.
- For mutating an object's pointer member, I can't pass the pointer into
  a function; I must pass the object.
- fseek and fgets don't work with live stdin (as opposed to from a text file).
  For fgets, the program waits until we have _new_ input before getting.
- () and [] has higher precedence than ++ and *, despite some online sources
  stating otherwise.
- Going back to a codebase after almost a year in a language that you spent
  around 3 weeks learning is terrifying.

### Todos
- Improve GC performance under allocation pressure.
- Add more primitives and other usual language features.
- Flesh out verbose mode.

### Running it in a terminal
I compile it with
```sh
clang data.c memory.c env.c eval.c main.c primitive.c print.c read.c
```
and then run it via
```sh
./a.out
```

### Running it on a browser
For compiling to JS and Wasm for browser use, I use
```sh
emcc data.c memory.c env.c eval.c main.c primitive.c print.c read.c -o example-core.js -sNO_EXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1
```
and serve it on localhost:8080 via
```sh
ruby server.rb
```

### Acknowledgements
Appreciate Mame's [xterm-pty](https://github.com/mame/xterm-pty/) for introducing a
simple way of getting this C program running on the browser using Emscripten and xtermjs.
