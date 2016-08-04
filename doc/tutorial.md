# Rho Tutorial


## Table of Contents

1. [Basics](#basics)
2. [Collections](#collections)
3. [Control Flow](#control-flow)
4. [Functions](#functions)
5. [Generators](#generators)
6. [Actors](#actors)
7. [Errors and Exceptions](#errors-and-exceptions)

## Basics

### "Hello, World!"

"Hello, World!" in Rho is as simple as

<pre>
<b>print</b> "Hello, World!"
</pre>

### Arithmetic

The standard arithmetic operators work as you'd expect:

<pre>
<b>print</b> 2 + 2              <i># prints 4</i>
<b>print</b> ((50 - 5*6)/4)**2  <i># prints 25</i>
<b>print</b> 0.1 + 0.2*3.0      <i># prints 0.7</i>
</pre>

### Strings

Strings can be enclosed by double or single quotes:

<pre>
<b>print</b> "Hello, World!"  <i># double quotes</i>
<b>print</b> 'Hello, World!'  <i># single quotes</i>
</pre>

This makes printing a literal `"` or `'` easy:

<pre>
<b>print</b> '"'  <i># print a double quotation mark</i>
<b>print</b> "'"  <i># print a single quotation mark</i>
</pre>

String concatenation can be done with the addition operator:

<pre>
<b>print</b> "Hello" + "World!"  <i># prints "HelloWorld!"</i>
</pre>

The global `str` function can be used to obtain the string representation of non-string objects:

<pre>
<b>print</b> str(42) + "!"  <i># prints "42!"</i>
</pre>

### Variables

Variables are assigned with `=`:

<pre>
foo = 42
bar = "hello"
baz = 3.14
</pre>

Compound assignments work as you'd expect:

<pre>
n = 1
n += 2  <i># n is now 3</i>
n *= 3  <i># n is now 9</i>
</pre>

Notice that you don't have to explicitly declare variables beforehand.


## Collections

### Lists

Lists can be created as such:

<pre>
my_list = [1, 1, 2, 3, 5]
</pre>

Lists are 0-indexed:

<pre>
my_list = [1, 1, 2, 3, 5]
<b>print</b> my_list[2]  <i># prints 2</i>
my_list[3] = 99
<b>print</b> my_list  <i># prints [1, 1, 2, 99, 5]</i>
</pre>

### Tuples

While lists are created with square brackets, tuples are created with parenthesis:

<pre>
my_tup = (1, 2, 3)
</pre>

Unlike lists, tuples are immutable, so their contents cannot be changed after they are created.

### Sets

Sets are orderless collections that cannot contain duplicate elements. For example:

<pre>
my_set = {1, 2, 3}  <i># same as, say, {3, 2, 1}</i>
</pre>

Sets are particularly effective at quickly performing containment checks:

<pre>
<b>print</b> 2 <b>in</b> my_set  <i># true</i>
<b>print</b> 4 <b>in</b> my_set  <i># false</i>
</pre>

Elements can be added to sets using the `add()` method:

<pre>
my_set.add(4)
</pre>

### Dictionaries

Dictionaries are orderless, associative collections. For example, a dictionary associating 'a' to 1, 'b' to 2 and 'c' to 3 would be:

<pre>
my_dict = {'a': 1, 'b': 2, 'c': 3}
</pre>

Retrieving the value associated with a particular key is similar to indexing a list:

<pre>
<b>print</b> my_dict['b']  <i># 2</i>
</pre>

Adding new values isn't much more complicated:

<pre>
my_dict['d'] = 4
</pre>


## Control Flow

###`if`-statements

`if`-statements are used to conditionally execute a block of code. For example:

<pre>
n = 2

<b>if</b> n + 2 == 4 {
    <b>print</b> "yes!"  <i># this line will be reached</i>
}
</pre>

An optional `else` clause can be included to execute a block of code when the `if` condition is not true:

<pre>
n = 2

<b>if</b> n + 2 != 4 {
    <b>print</b> "yes!"
} <b>else</b> {
    <b>print</b> "no!"  <i># this line will be reached</i>
}
</pre>

Note that, while curly braces are required for multi-statement blocks, single-statement blocks can be preceded by a `:`. The following snippet is equivalent to the last:

<pre>
n = 2

<b>if</b> n + 2 != 4: <b>print</b> "yes!"
<b>else</b>: <b>print</b> "no!"  <i># this line will be reached</i>
</pre>

### `while`-statements

`while`-statements repeatedly execute a block of code so long as the specified condition is true. For example, the following snippet prints all of the integers from 10 down to 1:

<pre>
n = 10

<b>while</b> n >= 1 {
    <b>print</b> n
    n -= 1
}
</pre>

### `for`-statements

`for`-statements are used to iterate over collections (or _iterators_, to be exact). This snippet prints 1, 1, 4, 9, 25 (each on its own line):

<pre>
<b>for</b> n <b>in</b> [1, 1, 2, 3, 5] {
    <b>print</b> n**2
}
</pre>

If you want to iterate over consecutive integers, you can use a range expression:

<pre>
<b>for</b> n <b>in</b> 1..10 {  <i># 1..10 covers 1 (inc.) to 10 (exc.)</i>
    <b>print</b> n
}
</pre>


## Functions

The `def` keyword is used to define functions. For example:

<pre>
<b>def</b> hypot(x, y) {
    <b>return</b> (x**2 + y**2)**0.5
}
</pre>

Functions can be nested:

<pre>
<b>def</b> hypot(x, y) {
    <b>def</b> square(x) {
        <b>return</b> x**2
    }

    <b>return</b> (square(x) + square(y))**0.5
}
</pre>

Functions can take default arguments:

<pre>
<b>def</b> hypot(x=42, y=3.14) {
    <b>return</b> (x**2 + y**2)**0.5
}
</pre>

### Anonymous Functions

Anonymous functions are preceded by a `:`. The first argument to an anonymous function is `$1`, the second is `$2` and so on. The same `hypot` function written as an anonymous function is:

<pre>
hypot = (:($1**2 + $2**2)**0.5)
</pre>


## Generators

Generators are very similar to functions, except that they are declared with the `gen` keyword. Moreover, while regular functions terminate via `return`, generators produce values via `produce`. For example, the following generator will produce the Fibonacci numbers:

<pre>
<b>gen</b> fib() {
	prev = 0
	curr = 1
	<b>while</b> 1 {
		next = prev + curr
		prev = curr
		curr = next
		<b>produce</b> prev
	}
}
</pre>

Now, to invoke the generator:

<pre>
v = fib()

<i># prints the first 10 Fibonacci numbers:</i>
<b>for</b> i <b>in</b> 0..10 { <b>print</b> next(v) }
</pre>

Generators can also be iterated over directly with a `for`-loop. This snippet is equivalent to the one above:

<pre>
v = fib()

<i># prints the first 10 Fibonacci numbers:</i>
i = 0
<b>for</b> n <b>in</b> v {  <i># direct iteration</i>
	<b>print</b> n
	i += 1
	<b>if</b> i == 10 { <b>break</b> }
}
</pre>


## Actors

Actors are also similar to functions, but are executed in a separate thread. The keyword `act` is used to define them:

<pre>
<b>act</b> counter(n) {
    <b>for</b> i <b>in</b> 1..n {
        <b>print</b> i
    }
}
</pre>

Now, multiple instances of this actor can be invoked, and each will run in its own thread:

<pre>
a1 = counter(4)
a2 = counter(5)

a1.start()
a2.start()
</pre>

`a1` and `a2` will commence counting independently. Here's one possible output:

<pre>
1
1
2
2
3
3
4
</pre>

### Messages and Futures

Actors can receive and reply to messages sent by the main program or from other actors. Actor instances have a `send()` method for others to send messages to them. Within an actor definition, the keyword `receive` is used to receive messages. Messages are received as "message objects", which wrap the sent value, and allow for replying via a `reply()` method. The `send()` method returns a "future object", which has a `get()` method for obtaining the replied value. To illustrate, consider the following "echo" actor:

<pre>
<b>act</b> echo() {
    <b>while</b> 1 {
        <b>receive</b> msg  <i># blocking</i>
        msg.reply(msg.contents())
    }
}
</pre>

Now, to instantiate and send messages to the actor:

<pre>
a1 = echo()
a1.start()
future = a1.send('hello!')
<b>print</b> future.get()  <i># prints 'hello!'</i>
a1.stop()  <i># infinitely-looping actors can be stopped with this method</i>
</pre>

If `get()` is used without arguments, it is blocking; otherwise, a timeout value in milliseconds can be specified as its only argument.

Notice also that we used the actor's `stop()` method here, since this actor loops indefinitely. In reality, this method sends a special kill-message to the actor indicating that it should return.


## Errors and Exceptions

Errors and exceptions both typically indicate that something went wrong. They differ in that errors indicate irrecoverable failures whereas exceptions can be caught and handled.

Errors include, for example, divide-by-zero errors and unbound-reference errors:

<pre>
<b>print</b> 1/0  <i># divide-by-zero error</i>
<b>print</b> x    <i># unbound-reference error</i>
</pre>

Exceptions include, for example, attribute exceptions and index exceptions:

<pre>
<b>print</b> foo.bar     <i># attribute exception ('foo' doesn't have member 'bar')</i>
<b>print</b> [1,2,3][3]  <i># index exception (index 4 is out of bounds)</i>
</pre>

### `try`-`catch`

Unlike errors, exceptions can be caught with a `try`-`catch` statement. For instance, imagine we want to check if `foo` has member `bar`; we can use a `try`-`catch`:

<pre>
<b>try</b> {
    bar = foo.bar
} <b>catch</b> (AttributeException) {
    <b>print</b> "'foo' has no member 'bar'!"
}
</pre>




