TODO List
---------

More recent items are added to the top. Items followed by a question mark as in "(?)" have not been decided on.

- [ ] More comprehensive testing of everything.

- [ ] Documentation.

- [ ] Allow for non-global free variables within functions.

- [ ] Implement more standard-library functions.

  - [ ] Implement more built-in functions.
   
  - [ ] Implement more methods in standard classes.

- [x] Calculate the value stacks' maximum sizes through a compile-time analysis so as to avoid having to reallocate value stacks at runtime.

  - [x] ~~Convert abstract syntax trees to control flow graphs before emitting bytecode so as to facilitate this compile-time analysis.~~

- [x] Check for memory leaks.

  - [x] Check for compiler memory leaks (there really shouldn't be any of these).
    
  - [x] Check for virtual machine memory leaks ~~(there likely will be some of these since the garbage collection mechanism has not yet been implemented)~~.

- [x] Add a function definition mechanism. A rudimentary call mechanism for testing has already been implemented.
  
  - [x] Add a `CT_ENTRY_CODEOBJ` enum value to the constant table code, which will be used for function bytecode.
      
  - [x] Update the parser to allow for `def` statements to define functions.
    
  - [x] The function definition syntax (for proof-of-concept purposes) is currently:

      def foo {...}

   Parameters should also be incorporated so as to produce the more conventional form:

      def foo(...) {...}
          
- [x] Implement a more robust parser.

  - [x] Detect invalid programs, and report why they are invalid.
    
  - [x] Incorporate line numbers into error messages.
    
- [x] Implement pseudo-classes so everything can be used as an object.

  - [x] `int` pseudo-class
    
  - [x] `float` pseudo-class
    
  - [x] ~~Treat everything as an object and get rid of the `Value` structure in `object.h`. (?)~~
