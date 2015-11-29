Rho Programming Language
========================

*Rho* is a small, still-in-development, dynamically typed programming language written in C99. The language is largely inspired by Python.

So far...
---------

The language is still in its early stages of development. So far, the following have been implemented and tested:

- Parser and compiler
- ~~Rudimentary~~ object system
- ~~Rudimentary~~ virtual machine

Each of these components will likely change and evolve as time goes on.

Examples
--------

#### "hello world"

<pre>
<i># simple "hello world" program</i>
<b>print</b> 'hello world'
</pre>

#### Arithmetic

<pre>
<b>print</b> ((50 - 5*6)/4)**2  <i># prints 25</i>
</pre>

#### `if` statements

<pre>
b = 1
<b>if</b> b {
    <b>print</b> "b is non-zero!"  <i># strings can be delimited by either " or '</i>
} <b>else</b> {
    <b>print</b> "b is zero!"
}
</pre>

#### `while` statements

<pre>
<i># print integers from 1 to 10:</i>
a = 1
<b>while</b> a <= 10 {
    <b>print</b> a
    a += 1
}
</pre>

#### Functions

<pre>
<i># factorial function</i>
<b>def</b> fact(n) {
    <b>if</b> n < 2 { <b>return</b> 1 } <b>else</b> { <b>return</b> n * fact(n - 1) }
}

<i># hypotenuse function</i>
<b>def</b> hypot(a, b) {
    <i># functions can be nested:</i>
    <b>def</b> square(x) {
        <b>return</b> x**2
    }

    <i># functions can be anonymous:</i>
	sqrt = :($1 ** 0.5)

    <b>return</b> sqrt(square(a) + square(b))
}
</pre>
