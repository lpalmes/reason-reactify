/**
		Resumable exceptions

This module implements and illustrates resumable exceptions in OCaml.
When an exception is raised, an exception handler has the option of
either aborting the computation and returning the (alternative) value,
or resuming the computation, passing it a reply. That is, resumable
exceptions are strictly the generalization of the native OCaml exceptions.
Our implementation maintains as much polymorphism as possible (that
is, the handler can process exceptions with the payload of many types;
because the resumptive exception can be  thrown at several locations,
the type of the resumption value may differ. This is fully supported).


As regular exceptions, resumable exceptions must be declared, with the
ordinary keyword 'exception'. A resumable exception amounts to two (or
more) ordinary exceptions. The first is what used to raise the
(resumable) exception.  The second is used to extract the returned
result. If there are resumptions of several types, we need to declare
the corresponding resumptive exceptions.

We introduce two new functions:

   [rhandle : (exn -> 'a) -> (unit -> 'a) -> 'a]

Here, [rhandle handler thunk] evaluates the thunk in the dynamic environment
extended with the binding of the exception handler to [handler]. If the
thunk terminated normally, its value is returned. A resumable exception
is passed to the [handler]. The handler is evaluated in the environment
of the [rhandle] call. If the handler returns normally, the further
evaluation of the thunk is aborted and the result of the handler becomes
the result of [rhandle]. If the handler raises an exception [RE_resume x],
the value [x] is passed to the resumption part of the [rraise] function.
If the [handler] raises any other exception, it is handled by the handler
that was in effect right before the evaluation of the [rhandle] expression.

The other function raises the resumable exception:
   [rraise : exn -> (exn -> 'a) -> 'a]

Its first argument is the exception (like that of the regular [raise] form).
The second argument receives the resumable exception and should unpack
it and return the resumption result, with which to continue the computation.

It should be clear from our implementation that we (ab)use open unions
implicit in ML exceptions.

Our inspiration is Luc Moreau's approach to defining the semantics of
regular and resumable exceptions, via dynamic binding and an Abort
operator.  See section 8, `Semantics of exceptions' of his `Syntactic
theory of Dynamic Binding' (HOSC, 11, 233-279 (1998)). We observe that
we never needed explicit delimited continuations and never needed
capturing them. This is because resuming an exception is absolutely no
different from the invocation of the handler as a regular function
(modulo change in the dynamic environment to the parent handler).

  $Id: resumable.ml,v 1.2 2006/06/14 22:10:04 oleg Exp oleg $

*/;

exception RE_resume(exn);
exception RE_abort;

/* Reification and reflection of exceptions */
type eeither('a) =
  | Left(exn)
  | Right('a);

let reflect =
  fun
  | Left(e) => raise(e)
  | Right(x) => x;

/* Global mutable location, to implement the dynamic environment
      of resumable exception handlers, using shallow binding.
   */
let handler_g: ref(exn => unit) = ref(_ => failwith("uncaught exception"));

let rraise = (v: exn, h) =>
  try (
    {
      handler_g^(v);
      assert(false);
    }
  ) {
  | RE_resume(e) => h(e)
  | e => raise(e)
  };

/* A convenience function for use in the exception handler */
let resume = x => raise(RE_resume(x));

let rhandle = (h, body) => {
  let old_handler = handler_g^;
  let exc_result = ref(() => failwith("bottom"));
  let rec new_handler = (v: exn) => {
    let () = handler_g := old_handler;
    let re =
      try (
        {
          let r = h(v);
          let () = exc_result := (() => r);
          RE_abort;
        }
      ) {
      | e => e
      };

    let () = handler_g := new_handler;
    raise(re);
  };

  let () = handler_g := new_handler;
  let result =
    try (Right(body())) {
    | RE_abort => Right(exc_result^())
    | e => Left(e)
    };

  let () = handler_g := old_handler;
  reflect(result);
};

/* --------------------------------------------- Tests */

/* Declare the first resumable exception. It has resumptions of two types */
exception Foo(int);
exception Foo_r1(bool);
exception Foo_r2(string);

/* Declare the second resumable exception */
exception Bar(char);
exception Bar_r(int);

let handler = v =>
  try (raise(v)) {
  | Foo(x) =>
    Printf.printf("Got Foo of %d\n", x);
    if (x < 100) {
      resume(Foo_r1(x < 4));
    } else {
      resume(Foo_r2("xxx"));
    };
  | Bar(c) =>
    Printf.printf("Got Bar of %c\n", c);
    if (c < 'E') {
      resume(Bar_r(int_of_char(c) + 1));
    } else {
      42.0;
    };
  }; /* aborting */

let () = {
  let r =
    rhandle(
      handler,
      () => {
        for (i in 1 to 5) {
          let v =
            rraise(Foo(i), e =>
              try (raise(e)) {
              | Foo_r1(x) => x
              }
            );
          let () = Printf.printf("Resumed Foo1 with %b\n", v);
          let v =
            rraise(Foo(100 + i), e =>
              try (raise(e)) {
              | Foo_r2(x) => x
              }
            );
          Printf.printf("Resumed Foo2 with %s\n", v);
        };
        for (i in 65 to 100) {
          let v =
            rraise(Bar(char_of_int(i)), e =>
              try (raise(e)) {
              | Bar_r(x) => x
              }
            );
          Printf.printf("Resumed Bar with %d\n", v);
        };
        assert(false);
      },
    );
  Printf.printf("\nFinal result %g\n", r);
};

/* Obtained result:

   Got Foo of 1
   Resumed Foo1 with true
   Got Foo of 101
   Resumed Foo2 with xxx
   Got Foo of 2
   Resumed Foo1 with true
   Got Foo of 102
   Resumed Foo2 with xxx
   Got Foo of 3
   Resumed Foo1 with true
   Got Foo of 103
   Resumed Foo2 with xxx
   Got Foo of 4
   Resumed Foo1 with false
   Got Foo of 104
   Resumed Foo2 with xxx
   Got Foo of 5
   Resumed Foo1 with false
   Got Foo of 105
   Resumed Foo2 with xxx
   Got Bar of A
   Resumed Bar with 66
   Got Bar of B
   Resumed Bar with 67
   Got Bar of C
   Resumed Bar with 68
   Got Bar of D
   Resumed Bar with 69
   Got Bar of E

   Final result 42
   */