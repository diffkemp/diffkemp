# Example of using DiffKemp on a very simple program

In this example, we will:

- edit a simple program which we will analyze using DiffKemp,
- mention the difference between syntax and semantic difference,
- show how to compare programs using DiffKemp,
- explain the meaning of DiffKemp output.

## 1. Preparing a simple program

Firstly, we create a `src/` directory which will contain the simple program.
Within the directory, we create file `old.c` with the following code:

```c
int add(int a, int b, int c) {
    int first = a;
    int second = b;
    return first + second;
}

int mul(int a, int b) {
    int res = 0;
    int i = 0;
    while (i < b) {
        int tmp = add(a, res, 5);
        res = tmp;
        i++;
    }
    return res;
}
```

The program contains two functions:

- `add`, which returns the sum of two numbers, and
- `mul`, which returns the product of two numbers.

The code contains a lot of redundant code on which we will demonstrate DiffKemp
functionality.

Next, we will create a copy of `old.c` named `new.c` which we will later edit.
The file will represent changes/refactorings made to the original one.
The copy is made so we can compare the file versions with each other.

To check that the files/programs are the same, we can compare them using Linux
`diff` utility.

```sh
diff src/old.c src/new.c
```

There is no output because there is no difference between the files.

## 2. Making modifications to the program

Now let's refactor/modify the `new.c` file to look like this:

```c
int add(int a, int b, int c) {
    return a + b;
}

int mul(int a, int b) {
    int res = 0;
    for (int i = 0; i < b; i++) {
        res = add(res, a, 5);
    }
    return res;
}
```

The functions do the same thing as before, but we did some *refactoring*:

- in the `add` function:
  - the temporary variables `first` and `second` are removed,
- in the `mul` function:
  - the `while` is replaced with for-loop,
  - the `tmp` variable is removed and
  - the `a`, `res` arguments are switched in the call of `add` function.

We can again run the `diff` utility to check that the files are now
different:

```bash
diff src/old.c src/new.c
```

We will get output with a syntax diff of the files, which should look like this:

```diff
2,4c2
<     int first = a;
<     int second = b;
<     return first + second;
---
>     return a + b;
9,13c7,8
<     int i = 0;
<     while (i < b) {
<         int tmp = add(a, res, 5);
<         res = tmp;
<         i++;
---
>     for (int i = 0; i < b; i++) {
>         res = add(res, a, 5);
```

## 3. Comparing the program using DiffKemp

Let's finally use DiffKemp to see how to use it to check if function behavior
changed. When we want to use DiffKemp we have to first create a *snapshot* of
each version of a program and then we can compare these snapshots.
The comparison will tell us which function semantics have probably changed.

> [!NOTE]
> Snapshot is the source file compiled to an intermediate code (LLVM IR) with
> additional metadata. When DiffKemp is comparing programs (checking its
> semantics), it compares the intermediate code to which the program was
> compiled.

### Creating a snapshot of the programs

To create the snapshot we will use DiffKemp
[`build` command](../usage.md#a-build-snapshot-generation-of-make-based-projects).
The command takes the path to the source file and the path to where it can
create the snapshot. We will build snapshots of both files and save them to
`old-snapshot` and `new-snapshot` directories.

```sh
diffkemp build src/old.c old-snapshot
diffkemp build src/new.c new-snapshot
```

### Comparing the program and interpreting the result

Now when we have created the snapshots of both programs, we can compare
the programs (or more precisely, the created snapshots). For this, we will use
DiffKemp [`compare` command](../usage.md#2-semantic-comparison-command)
to which we will give paths to the snapshots. By default, the command will
compare all functions located in the file/program. We can also specify the
`--stdout` argument to get results to standard output.

```sh
diffkemp compare --stdout old-snapshot new-snapshot
```

Wait, there is no output?! That's right, similar to the classic diff utility,
DiffKemp `compare` command does not output anything if it evaluates the programs
(individual functions) as equal. Even if there is a syntactic difference
which we could see using the diff utility, the semantic of the functions
remained the same.

## 4. Making a semantic difference

Let's make more changes to `src/new.c` file:

```c
int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    int res = 0;
    for (int i = 0; i < b; i++) {
        res = add(res, a);
    }
    return res;
}
```

We have removed the `c` parameter from `add` function, because it was not used
in the function. In the `mul` function we removed the `5` from arguments given
to `add` function.

Now let's rebuild the snapshot and compare them again:

```sh
diffkemp build src/new.c new-snapshot
diffkemp compare --stdout old-snapshot new-snapshot
```

This time there is an output:

```sh
add:
  add differs:
  {{{
    Diff:
    *************** int add(int a, int b, int c) {
    *** 1,5 ***
    ! int add(int a, int b, int c) {
    !     int first = a;
    !     int second = b;
    !     return first + second;
      }
    --- 1,3 ---
    ! int add(int a, int b) {
    !     return a + b;
      }
  }}}
```

It tells us that there was found a difference. Let's break the output down:

```sh
add:
```

The first line means that the compared function `add` has probably
different semantic.

```sh
  add differs:
```

The second line tells us that the compared function differs in function `add`
(in itself). Right now it does not have to make sense for us, why DiffKemp
repeats the function name, but it will make sense later.

```diff
  {{{
    Diff:
    *************** int add(int a, int b, int c) {
    *** 1,5 ***
    ! int add(int a, int b, int c) {
    !     int first = a;
    !     int second = b;
    !     return first + second;
      }
    --- 1,3 ---
    ! int add(int a, int b) {
    !     return a + b;
      }
  }}}
```

The last part of the output shows us the syntax difference of the function
that was evaluated as semantically changed. This is for us to see the
problematic change which is most probably the cause of the semantic difference.

The `add` function differs because it accepts different amounts of arguments,
this would break functions/libraries which would use the function.
But let's say that the `add` function is some inner library function that
we do not mind if its behavior changes, but we provide `mul` function,
whose behavior should remain the same even after refactoring. Let's compare
just `mul` function, we can do this by using `-f` argument in `compare` command.

```sh
diffkemp compare --stdout old-snapshot new-snapshot -f mul
```

The output is again empty because the changes in `add` function did not impact
the behavior of the `mul` function.

## 5. DiffKemp compares functions including called functions

Because it does not have to be necessarily obvious that when we compare only
`mul` function DiffKemp also checks the called functions (in this case `add`
function), let's again edit the `src/new.c` file to look like this:

```c
int add(int a, int b, int c) {
    return a + b + c;
}

int mul(int a, int b) {
    int res = 0;
    int i = 0;
    while (i < b) {
        int tmp = add(a, res, 5);
        res = tmp;
        i++;
    }
    return res;
}
```

Most of the changes were removed (the file looks almost the same as
`src/old.c`), but the main difference is that in the function `add` is the sum
of not only `a` and `b` but also `c`. So the function `add` has different
semantics than it had before.

Let's again rebuild the snapshot and compare just the function `mul`.

```sh
diffkemp build src/new.c new-snapshot
diffkemp compare --stdout old-snapshot new-snapshot -f mul
```

The output looks like this:

```txt
mul:
  add differs:
  {{{
    Callstack (old-snapshot):
    add at old.c:11

    Callstack (new-snapshot):
    add at new.c:9

    Diff:
    *************** int add(int a, int b, int c) {
    *** 1,5 ***
      int add(int a, int b, int c) {
    !     int first = a;
    !     int second = b;
    !     return first + second;
      }
    --- 1,3 ---
      int add(int a, int b, int c) {
    !     return a + b + c;
      }
  }}}
```

Let's break it down:

```txt
mul:
  add differs:
```

We can see that the compared function `mul` has different semantics, which is
because in the function `add` was found semantic difference.

```txt
    Diff:
    *************** int add(int a, int b, int c) {
    *** 1,5 ***
      int add(int a, int b, int c) {
    !     int first = a;
    !     int second = b;
    !     return first + second;
      }
    --- 1,3 ---
      int add(int a, int b, int c) {
    !     return a + b + c;
      }
```

The last part of the output is again the syntax difference of the differing
(`add`) function.

```txt
    Callstack (old-snapshot):
    add at old.c:11

    Callstack (new-snapshot):
    add at new.c:9
```

We have skipped one part of the output which we did not see previously.
The part contains call stacks, which is a list of called functions from the
compared function (`mul`) to the differing function (`add`). For each function,
there is information about in which file was the function called and on which
line. We can see that the function `add` was called in the old version in the
`old.c` file on line `11` and in the new version in the file `new.c` on
line `9`. Since there are no other functions in the call stack, it indicates
that `add` was directly called from `mul`.

To explain the call stacks more, consider a program structure like this:

```c
int baz() {
  // Contains semantic difference.
}
int bar() {
  // ...
  baz(); // line 20
  // ...
}

// This function we want to compare (the function that our library provides).
int foo() {
  // ...
  bar(); // line 50
  // ...
}
```

There are three functions `baz`, `bar` and `foo`. `foo` calls `bar` function
which calls `baz` function. The `baz` function has a different semantics than it
had before. If we compared the function `foo`, the DiffKemp output would look
something like this:

```txt
foo:
  baz differs:
  {{{
    Callstack (old-snapshot):
    bar at old.c:50
    baz at old.c:20

    Callstack (new-snapshot):
    bar at new.c:50
    baz at new.c:20

    Diff:
    ...
  }}}
```

This would tell us that the compared function `foo` contains the semantic
difference, the difference was found in `baz` function which is called on line
`20` from function `bar` which is called on line `50` from the compared
function.

## 6. Summary

That's all for the simple program example, we learned, that:

- Firstly we must create *snapshot* of each version we want to compare using
  the `diffkemp build` command.
- Then we can compare the programs using the snapshots with `diffkemp compare`
  command.
- If DiffKemp evaluates the programs (functions) as semantically equal the
  `compare` command does not output anything.
- DiffKemp compares functions including the called functions.
- If DiffKemp finds a semantic difference it shows in the output:
  - which compared function is semantically not equal,
  - in which function was the difference found,
  - call stacks from the compared function to the differing function,
  - syntactic difference of the differing function.
