`.rhoc` File Specification
=======================

**Version 0.0.1**

This document specifies the format of `.rhoc` (pronounced "row see") files, which are produced as the result of compiling Rho source code.

Every `.rhoc` file has the following global structure:

  | Rhoc Layout                  |
  |------------------------------|
  | Magic bytes (`magic`)        |
  | Metadata                     |
  | Line number table (`lnotab`) |
  | Symbol table (`symtab`)      |
  | Constant table (`consttab`)  |
  | Program bytecode (`code`)    |

Each component is described in detail below.

Types
-----

Before describing each section, we specify several data types used in `.rhoc` files.

  | Type     | Description                                           |
  |----------|-------------------------------------------------------|
  | `byte`   | Unsigned 8-bit value                                  |
  | `uint16` | Unsigned 16-bit value (little endian)                 |
  | `int32`  | Signed (2's complement), 32-bit value (little endian) |
  | `float`  | IEEE 754 64-bit floating point value                  |
  | `str`    | Series of bytes terminated with a null byte (`0x00`)  |

Magic Bytes
-----------

Every Rhoc file begins with the following 4 bytes:

    Value    Type    Notes
    =======================
    0xFE     byte
    0xED     byte
    0xF0     byte
    0x0D     byte    

Metadata
---------

The metadata section has the following layout:

    Value            Type            Notes
    =======================================
    vstack_depth     uint16
    try_catch_depth  uint16

- `vstack_depth` represents the maximum number of elements that can possibly be on the value stack at any given time as a result of executing the associated bytecode.

- `try_catch_depth` represents the maximum try-catch depth of the associated bytecode. For example, the following code fragment:

      try {
          try {
              try {
                  foo()
              } catch(Exception) {}
          } catch (Exception) {}
      } catch (Exception) {}

  would compile to bytecode with a `try_catch_depth` of 3.

Line Number Table
-----------------

Note: The line number table is based on CPython's line number table, described [here](http://svn.python.org/projects/python/trunk/Objects/lnotab_notes.txt).

The line number table begins with a `uint16` (`S`) representing the size of the line number table. Following `S` is another `uint16` (`L`) representing the first line number of the code associated with the given line number table.

The line number table itself is a series of (`d_ins`, `d_line`) pairs which encode the line number information of the compiled program. `d_ins` and `d_line` are `byte` values. The pair (`0`,`0`) indicates the end of the line number table. Hence, the overall layout is as follows:

    Value      Type     Notes
    ==========================
    S          uint16
    L          uint16
    d_ins_1    byte
    d_line_1   byte
    d_ins_2    byte
    d_line_2   byte
    ...
    d_ins_N    byte     N == S/2 - 2 (-2 because of two null bytes)
    d_line_N   byte
    0          byte
    0          byte

The (`d_ins`, `d_line`) pairs indicate bytecode- and corresponding line-offsets, starting from the given initial line number `L`. Hence:

- The opcode at offset 0 is on line `L`.
- The opcode at offset `d_ins_1` is on line `L + d_line_1`.
- The opcode at offset `d_ins_1 + d_ins_2` is on line `L + d_line_1 + d_line_2`.
- And so forth...

If either `d_ins` or `d_line` is greater than 255, the (`d_ins`, `d_line`) pair is actually represented with multiple pairs. There are three cases to consider:

##### Case 1: `d_ins > 255` only

    d_ins
    d_line

is expanded to

    255
    d_line
    d_ins - 255
    0
    
If `d_ins - 255` is itself greater than 255, the process is applied recursively to the (`d_ins - 255`, `0`) pair.

##### Case 2: `d_line > 255` only

    d_ins
    d_line

is expanded to

    d_ins
    255
    0
    d_line - 255
    
If `d_line - 255` is itself greater than 255, the process is applied recursively to the (`0`, `d_line - 255`) pair.

##### Case 3: `d_ins > 255` and `d_line > 255`

    d_ins
    d_line

is expanded to

    255
    255
    d_ins - 255
    d_line - 255
    
If either `d_ins - 255` or `d_line - 255` is greater than 255, the process is applied recursively to the (`d_ins - 255`, `d_line - 255`) pair.

Symbol Table
------------

The symbol table has the following layout:

    Value             Type           Notes
    =======================================
    ST_ENTRY_BEGIN    byte           0x10
    n_locals          uint16
    local_1           str
    local_2           str
    ...
    local_A           str            A == n_locals
    n_attrs           uint16
    attr_1            str
    attr_2            str
    ...
    attr_B            str            B == n_attrs
    n_frees           uint16
    free_1            str
    free_2            str
    ...
    free_C            str            C == n_frees
    ST_ENTRY_END      byte           0x11

- `local`s refer to local variables (bound in a function's scope).
- `attr`s refer to attributes (attributes used in a function's scope)
- `free`s refers to free variables (used but not bound in a function's scope).

Constant Table
--------------

The constant table has the following layout:

    Value             Type           Notes
    =======================================
    CT_ENTRY_BEGIN    byte           0x20
    ct_size           uint16
    entry_type_1      byte
    entry_1           (?)
    entry_type_2      byte
    entry_2           (?)
    ...
    entry_type_N      byte           N == ct_size
    entry_N           (?)
    CT_ENTRY_END      byte           0x25
    
The format of each `entry` is determined by the corresponding `entry_type`. There are several possible values `entry_type` can take on:

##### `CT_ENTRY_INT` (`0x21`)

The corresponding `entry` is of type `int32`.

##### `CT_ENTRY_FLOAT` (`0x22`)

The corresponding `entry` is of type `float`.

##### `CT_ENTRY_STR` (`0x23`)

The corresponding `entry` is of type `str`.

##### `CT_ENTRY_CODEOBJ` (`0x24`)

The corresponding `entry` in this case is more complicated. It has the following layout:

    Value             Type           Notes
    =======================================
    code_len          uint16
    name              str
    arg_count         uint16
    stack_depth       uint16
    try_catch_depth   uint16
    code_1            byte
    code_2            byte
    ...
    code_M            byte           M == code_len

- `code_len` is the number of bytes in the function's bytecode.
- `arg_count` is the number of arguments taken by the encoded function.
- `stack_depth` is the maximum value stack depth.
- `try_catch_depth` is the maximum try-catch depth
- `code` contains the bytecode of the encoded function, but begins with the function's line number table, symbol table, and constant table (in this order).

Program Bytecode
----------------

Rho opcodes are all one byte in length, and may take a number of `uint16` arguments, which are placed directly after the opcode itself. Below, each opcode and its functionality is described. Note that the first opcode has a value of `0x30`, and that the values increment sequentially, meaning the second opcode has a value of `0x31`, the third a value of `0x32` and so forth.

##### `INS_NOP`
No operation.

##### `INS_LOAD_CONST(n)`
Pushes the `n`th constant from the constant table onto the value stack.

##### `INS_LOAD_NULL`
Pushes the null value (currently defined to be the constant `0`) onto the value stack.

##### `INS_ADD`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a + b` onto the value stack.

##### `INS_SUB`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a - b` onto the value stack.

##### `INS_MUL`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a * b` onto the value stack.

##### `INS_DIV`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a / b` onto the value stack.

##### `INS_MOD`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a % b` onto the value stack.

##### `INS_POW`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a ** b` (power) onto the value stack.

##### `INS_BITAND`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a & b` (bitwise-AND) onto the value stack.

##### `INS_BITOR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a | b` (bitwise-OR) onto the value stack.

##### `INS_XOR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a ^ b` (XOR) onto the value stack.

##### `INS_BITNOT`
Pops one values `a` off of the value stack; pushes `~a` (bitwise-NOT) onto the value stack.

##### `INS_SHIFTL`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a << b` onto the value stack.

##### `INS_SHIFTR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a >> b` onto the value stack.

##### `INS_AND`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a and b` (logical-AND) onto the value stack.

##### `INS_OR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a or b` (logical-OR) onto the value stack.

##### `INS_NOT`
Pops one values `a` off of the value stack; pushes `!a` (logical-NOT) onto the value stack.

##### `INS_EQUAL`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a == b` onto the value stack.

##### `INS_NOTEQ`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a != b` onto the value stack.

##### `INS_LT`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a < b` onto the value stack.

##### `INS_GT`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a > b` onto the value stack.

##### `INS_LE`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a <= b` onto the value stack.

##### `INS_GE`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a >= b` onto the value stack.

##### `INS_UPLUS`
Pops one values `a` off of the value stack; pushes `+a` onto the value stack.

##### `INS_UMINUS`
Pops one values `a` off of the value stack; pushes `-a` onto the value stack.

##### `INS_IADD`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a += b` onto the value stack.

##### `INS_ISUB`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a -= b` onto the value stack.

##### `INS_IMUL`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a *= b` onto the value stack.

##### `INS_IDIV`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a /= b` onto the value stack.

##### `INS_IMOD`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a %= b` onto the value stack.

##### `INS_IPOW`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a **= b` onto the value stack.

##### `INS_IBITAND`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a &= b` (bitwise-AND) onto the value stack.

##### `INS_IBITOR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a |= b` (bitwise-OR) onto the value stack.

##### `INS_IXOR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a ^= b` (XOR) onto the value stack.

##### `INS_ISHIFTL`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a <<= b` onto the value stack.

##### `INS_ISHIFTR`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a >>= b` onto the value stack.

##### `INS_IN`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a in b` (whether `b` "contains" `a`) onto the value stack.

##### `INS_STORE(n)`
Pops a value off the value stack and stores it in the `n`th local variable. 

##### `INS_LOAD(n)`
Pushes the value of the `n`th local variable onto the value stack if it has been initialized, and throws a _name error_ otherwise.

##### `INS_LOAD_GLOBAL(n)`
Pushes the value of the `n`th global variable onto the value stack if it has been initialized, and throws a _name error_ otherwise.

##### `INS_LOAD_ATTR(n)`
Pops one value `a` off of the value stack and pushes `a.s` where `s` is the `n`th attribute (from the symbol table).

##### `INS_SET_ATTR(n)`
Pops two values `a` (1st pop) and `b` (2nd pop) off of the value stack; sets `a.s = b` where `s` is the value of the `n`th attribute (from the symbol table).

##### `INS_LOAD_INDEX`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a[b]` onto the value stack.

##### `INS_SET_INDEX`
Pops two values `c` (1st pop), `b` (2nd pop) and `a` (3rd pop) off of the value stack; sets `b[c] = a`.

##### `INS_APPLY`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a @ b` onto the value stack, where `@` is the _@pplication_ operator (i.e. it applies the function `a` to every element of the sequence `b`).

##### `INS_IAPPLY`
Pops two values `b` (1st pop) and `a` (2nd pop) off of the value stack; pushes `a @= b` onto the value stack, where `@` is the _@pplication_ operator (i.e. it applies the function `a` to every element of the sequence `b`).

##### `INS_LOAD_NAME(n)`
Pushes the value associated with the `n`th free variable onto the value stack if it has been initialized, and throws a _name error_ otherwise.

##### `INS_PRINT`
Pops a value off of the value stack and prints it to standard-out.

##### `INS_JMP(offset)`
Jumps forward by `offset` _bytes_ (not instructions).

##### `INS_JMP_BACK(offset)`
Jumps backward by `offset` _bytes_ (not instructions).

##### `INS_JMP_IF_TRUE(offset)`
Pops a value off of the value stack and jumps forward by `offset` _bytes_ (not instructions) if the value is _non-zero_.

##### `INS_JMP_IF_FALSE(offset)`
Pops a value off of the value stack and jumps forward by `offset` _bytes_ (not instructions) if the value is not _non-zero_.

##### `INS_JMP_BACK_IF_TRUE(offset)`
Pops a value off of the value stack and jumps backward by `offset` _bytes_ (not instructions) if the value is _non-zero_.

##### `INS_JMP_BACK_IF_FALSE(offset)`
Pops a value off of the value stack and jumps backward by `offset` _bytes_ (not instructions) if the value is not _non-zero_.

##### `INS_JMP_IF_TRUE_ELSE_POP(offset)`
Jumps forward by `offset` _bytes_ (not instructions) if the value at the top of the value stack is _non-zero_, and pops a value off of the value stack otherwise.

##### `INS_JMP_IF_FALSE_ELSE_POP(offset)`
Jumps forward by `offset` _bytes_ (not instructions) if the value at the top of the value stack is not _non-zero_, and pops a value off of the value stack otherwise.

##### `INS_CALL(args)`
The low byte `L` of `args` is the number of unnamed arguments, while the high byte `H` is the number of named arguments. The value stack is expected to look like this when this opcode is reached (from top to bottom):

- function
- named argument 1
- name 1 (string object)
- named argument 2
- name 2 (string object)
- ...
- named argument `H`
- name `H` (string object)
- unnamed argument 1
- unnamed argument 2
- ...
- unnamed argument `L`

All of these values are popped off of the value stack, the function is called, and the return value is pushed on the stack.


##### `INS_RETURN`
Pops a value off of the value stack and returns it, thereby terminating the current function.

##### `INS_THROW`
Pops a value (expected to be an instance of the _exception class_ or of a subclass thereof) off of the value stack and throws it, thereby terminating the current function.

##### `INS_TRY_BEGIN(length, handler_offset)`
Marks the beginning of a `try`-block of length `length` bytes, with a handler at a forward offset of `handler_offset` with respect to the position of this opcode.

##### `INS_TRY_END`
Marks the end of a `try` block; used for internal bookkeeping of `try`-`catch` blocks.

##### `INS_JMP_IF_EXC_MISMATCH(offset)`
Pops two values off of the value stack: `t` (first pop) which is expected to be a (not necessarily proper) subclass of the _exception class_, and `e` (second pop) which is expected to be an instance of the _exception class_ or of a subclass thereof. Jumps forward `offset` bytes (not instructions) if `e` is an instance of `t` or of any subclass thereof.

##### `INS_MAKE_LIST(count)`
Pops `count` values off of the value stack, places them in a list (in the order in which they were popped), and pushes the resulting list back onto the value stack.

##### `INS_MAKE_TUPLE`
Pops `count` values off of the value stack, places them in a tuple (in the order in which they were popped), and pushes the resulting tuple back onto the value stack.

##### `INS_IMPORT(n)`
Imports the module whose name is given by the `n`th name in the current function's symbol table's _local_ entries. Throws an error if the module cannot be loaded.

##### `INS_EXPORT(n)`
Pops a value `v` off of the value stack and places it in the current module's _export dictionary_ using the `n`th name in the current function's symbol table's _local_ entries as the key.

##### `INS_EXPORT_GLOBAL(n)`
Pops a value `v` off of the value stack and places it in the current module's _export dictionary_ using the `n`th name in the top-level symbol table's _local_ entries as the key.

##### `INS_GET_ITER`
Pops a value `v` off of the value stack, obtains an iterator over `v` (via `v`'s `iter` method), and pushes the resulting iterator onto the stack. Throws a "type exception" if `v` does not have an `iter` method.

##### `INS_LOOP_ITER(offset)`
The value at the top of the value stack is expected to be an iterator `v`. Obtains the next element `x` from `v` (via `v`'s `iternext` method) and, if `x` is not a sentinel value marking the end of iteration, pushes `x` onto the value stack, otherwise jumps forward `offset` bytes (not instructions).

##### `INS_MAKE_FUNCOBJ(n)`
The value at the top of the value stack is expected to be a CodeObject `c`. Converts `c` into a FuncObject with `n` default parameters found on the stack below `c`. Pops `c` and the `n` values below off of the value stack and pushes the newly created FuncObject.

##### `INS_POP`
Pops one value off of the value stack.

##### `INS_DUP`
Pushes `v` onto the value stack where `v` is the value currently at the top of the stack.

##### `INS_DUP_TWO`
Pushes `v2` and `v1` onto the value stack (in that order) where `v1` is the value on top of the stack and `v2` the value just below `v1`.

##### `INS_ROT`
Swaps `v1` and `v2` on the value stack where `v1` is the value on top of the stack and `v2` the value just below `v1`.

##### `INS_ROT_THREE`
Pops `v1` off of the value stack and inserts it directly below `v3`, where `v1` is the value on top of the value stack, `v2` the value just below `v1` and `v3` the value just below `v2`.

