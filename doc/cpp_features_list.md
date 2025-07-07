# List of C++14 and Later Features Used in the Project

This is a list of features that have been used in this project. This may be
useful for porting it back to some earlier C++ standard if needed.

LHF currently requires C++17 to compile.

* `std::make_index_sequence`: C++14

  This is a template syntactic sugar to generate a sequence of indices to
  iterate over a tuple in the nesting implementation. An 'in-house' replacement
  can be written but the decision was made to leverage the standard library
  instead.

* `std::shared_mutex`: C++17

  This is equivalent to using `pthread_rwlock_t` as a reader-writer lock. An
  implementation for this in the C++ standard library only comes up in C++17.


# List of Compiler-Specific Pragmas Used in the Project

LHF currently uses a single GCC pragma to disable a warning for `-Wtype-limits`.
Inside of a sanity check macro. No switch has yet been added for other
compilers but can be easily added.

```c++
_Pragma("GCC diagnostic ignored \"-Wtype-limits\"")
```