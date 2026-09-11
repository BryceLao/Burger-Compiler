# Burger Compiler

A compiler for **Burger**, a custom programming language written in C++20.

Burger source files (`.bger`) are tokenized, parsed into an abstract syntax tree (AST), type-checked, and compiled into **x86-64 NASM assembly**. The resulting assembly is assembled and linked into a native Linux executable.

## Overview

| Stage         | Location         | Description                                                                                                   |
|---------------|------------------|---------------------------------------------------------------------------------------------------------------|
| **Tokenizer** | `src/tokenizer/` | Converts source code into a sequence of typed tokens.                                                         |
| **Parser**    | `src/parser/`    | Builds the AST using precedence climbing, resolves scopes, and performs syntax and type checking.             |
| **Generator** | `src/generator/` | Traverses the AST and generates x86-64 NASM assembly, including the runtime routines used by Burger programs. |
| **Utilities** | `src/util/`      | Provides shared compiler utilities, including the arena allocator and data type definitions.                  |

The compiler performs as much validation as possible at compile time. Errors are reported with their source line number:

```text
Line 4: Error: Cannot assign value of type 'string' to variable of type 'int'
```

Errors that cannot be determined statically, such as array bounds violations, are handled by generated runtime error handlers.

## Language Features

### Types

* `int`
* `bool`
* `char`
* `string`
* `void`
* One-dimensional arrays of `int`, `bool`, and `char`

### Variables and Scoping

* Variable declarations with `set`
* Variable reassignment
* Block scoping with `{}`

```
set int x = 10;
x = x + 5;
```

### Operators

**Arithmetic**

* `+`
* `-`
* `*`
* `/`
* `%`

**Comparison**

* `==`
* `!=`
* `<`
* `>`
* `<=`
* `>=`

**Boolean**

* `and`
* `or`
* `not`

**Unary**

* Unary negation `-`

### Control Flow

* `if`
* `else if`
* `else`
* `while`

Conditions do not require parentheses:

```text
if x > 10 {
    print("Large");
} 
else if x == 10 {
    print("Exactly 10");
} 
else {
    print("Small");
}
```

### Arrays

Arrays are dynamically allocated on the heap using the Linux `mmap` syscall.

```text
set int[] numbers = new Array[3];

numbers[0] = 10;
numbers[1] = 20;

print(numbers[0]);
print(numbers.size());
```

Arrays are one-dimensional and have a fixed size after allocation.

### Strings

Strings support:

* Concatenation with `+`
* Comparisons with `==`, `!=`, `<`, `>`, `<=`, `>=`
* `.length()` for retrieving the string length

```text
set string name = "Adam";
print("Hello " + name + "!");
print(name.length());
```

### Functions

Burger supports:

* Typed parameters
* Return values
* `void` functions
* Recursive functions
* Function calls using the `call` keyword

```text
define int factorial(int n) {
    if n <= 1 {
        return 1;
    }

    return n * call factorial(n - 1);
}

set int result = call factorial(5);
print(result);
```

### Built-ins

| Function     | Description                              |
|--------------|------------------------------------------|
| `print()`    | Prints a value                           |
| `exit()`     | Terminates the program with an exit code |
| `toString()` | Converts a value to a string             |
| `stoi()`     | Converts a string to an integer          |
| `int()`      | Casts a value to `int`                   |
| `bool()`     | Casts a value to `bool`                  |
| `char()`     | Casts a value to `char`                  |

### Input

Burger provides several input functions, all of which accept an optional prompt:

* `readInt()`
* `readBool()`
* `readChar()`
* `readNext()`
* `readLine()`

```text
set string name = readLine("Enter your name: ");
print("Hello " + name + "!");
```

## Example Program

The following program demonstrates functions, recursion, arrays, strings, and input:

```text
define int factorial(int n) {
    if n <= 1 {
        return 1;
    }

    return n * call factorial(n - 1);
}

set int result = call factorial(5);
print(result);

set int[] numbers = new Array[3];
numbers[0] = 10;
print(numbers[0]);
print(numbers.size());

set string name = readLine("Enter your name: ");
print("Hello " + name + "!");

exit(0);
```

More example programs are available in [`examples/`](examples):

* [`basics.bger`](examples/basics.bger)
* [`control_flow.bger`](examples/control_flow.bger)
* [`arrays_and_input.bger`](examples/arrays_and_input.bger)
* [`functions.bger`](examples/functions.bger)

## Requirements

Burger Compiler targets **Linux x86-64** because it generates raw x86-64 NASM assembly and uses Linux system calls.

| Tool             | Version / Purpose               |
|------------------|---------------------------------|
| **C++ compiler** | C++20 — GCC 10+ or Clang 12+    |
| **CMake**        | 3.20+                           |
| **NASM**         | Assembles generated `out.asm`   |
| **ld**           | Links the resulting object file |

## Building

Clone the repository and build with CMake:

```
git clone https://github.com/BryceLao/Burger-Compiler
cd Burger-Compiler

cmake -S . -B build
cmake --build build
```

## Running

The compiler takes the path to a Burger source file:

```bash
./build/burger examples/functions.bger
./out
```

## Language Rules

* Every statement must end with a semicolon (`;`).
* `if` and `while` conditions are not wrapped in parentheses.
* Functions must be defined before they are called.
* Function definitions cannot be nested inside other functions.
* Boolean literals are `True` and `False`.
* Character literals use single quotes and must contain exactly one character.
* String literals use double quotes.
* Arrays are one-dimensional and have a fixed size.
* `float` and `double` are not supported.
* Only single-line `//` comments are supported.
