Rho Programming Language
========================

*Rho* is a small, still-in-development, dynamically typed programming language written in C99. The language is largely inspired by Python.

So far...
---------

The language is still in its very early stages of development. So far, the following have been implemented and tested:

- Parser and compiler
- Rudimentary object system
- Rudimentary virtual machine

Each of these components will likely change and evolve as time goes on.

Examples
--------

#### "hello world"

    # simple "hello world" program
    print 'hello world'

#### Arithmetic

    print ((50 - 5*6)/4)**2  # prints 25

#### `if` statements

    b = 1
    if b {
    	print "b is non-zero!"  # strings can be delimited by either " or '
	}
	
#### `while` statements

    # prints integers from 10 down to 1
    a = 10
    while a {
	    print a
	    a = a - 1
    }
