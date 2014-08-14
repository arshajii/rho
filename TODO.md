TODO List
---------

The list is not in any particular order, although related items should be grouped. Items followed by a question mark as in "(?)" have not been decided on.

- [ ] Calculate the value stacks' maximum sizes through a compile-time analysis so as to avoid having to reallocate value stacks at runtime.

    - [ ] Convert abstract syntax trees to control flow graphs before emitting bytecode so as to facilitate this compile-time analysis.

- [ ] Check for memory leaks using Valgrind or another similar software. (I have yet to do this because Valgrind is not compatible with my OS.)

    - [ ] Check for compiler memory leaks (there really shouldn't be any of these).
    
    - [ ] Check for virtual machine memory leaks (there likely will be some of these since the garbage collection mechanism has not yet been implemented).

- [ ] Add a function definition mechanism. A rudimentary call mechanism for testing has already been implemented.
  
    - [x] Add a `CT_ENTRY_CODEOBJ` enum value to the constant table code, which will be used for function bytecode.
      
    - [x] Update the parser to allow for `def` statements to define functions.
    
    - [ ] The function definition syntax (for proof-of-concept purposes) is currently:

            def foo {...}

        Parameters should also be incorporated so as to produce the more conventional form:

            def foo(...) {...}
          
- [x] Implement a more robust parser.

    - [x] Detect invalid programs, and report why they are invalid.
    
    - [x] Incorporate line numbers into error messages.
    
- [ ] Implement pseudo-classes so everything can be used as an object.

    - [ ] `int` pseudo-class
    
    - [ ] `float` pseudo-class
    
    - [ ] Treat everything as an object and get rid of the `Value` structure in `object.h`. (?)
    
- [ ] More comprehensive testing of everything.
