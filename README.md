# LatticeHashForest

LatticeHashForest (LHF) is an attempt at efficient set operations based on a
multi-faceted caching mechanism under the scope of performing data flow analysis
on programs. Several speculations about the data have been made on the basis of
this scope for its implementation, some of which are:

1. Data is (often) redundant.
2. Data is (often) redundantly computed.
3. Data is (often) sparse.
4. There are (usually) set patterns and common input data used for the
   computation of new data.

LHF Is a data structure that exploits the above properties to create both a
memory and compute-efficient set operation system for data flow analyses. It is
currently implemented as a single class in a C++ header-only library.
(`lhf::LatticeHashForest`)

LHF assigns a unique number to each unique set that is inserted or computed,
which reduces operations, like checking for equality, to a simple integer
comparison. In order to prevent duplicate sets, a mapping from the set to the
unique integer is stored in a hash table. The set's contents are hashed.

LHF hashes operations like unions, intersections and differences as well,
and stores the mapping for each operation (a pair of integers denoting the two
operands, to the result, which is another integer) in a separate hash table,
allowing for efficient access to already computed information. Other inferences
from the operation, such as subset relations are also hashed in order to
possibly speed up future operations.

LHF is meant to be extended to fit the needs of a particular use case. The class
should be derived and more operations or facilities should be implemented as
required if in case LHF does not meet all needs. Although the ways in which the
tight coupling between operations still needs to be ironed out.

This project is still in active development and hence errors and issues may
arise frequently.

## Building (Test Programs) and Installing

Your system must have a compiler that supports C++11, have standard Unix/Linux
build tools and a CMake version greater than 3.23.

First create a directory called "build" in the project directory and go into it

```
mkdir build && cd build
```

Run CMake in this directory.

```
cmake ..
```

Now, run make to build the example programs.

```
make
```

If you would like to skip compiling the example programs, unset the following
flag when invoking CMake:

```
cmake .. -DENABLE_EXAMPLES=NO
```

To install the headers to your system, do the following. You may need elevated
privileges to do this.

```
make install
```

## Testing

LHF uses GoogleTest as its unit testing suit. To enable it in the LHF build,
pass another flag to the CMake configure command:

```
cmake .. -DENABLE_TESTS=YES
```

And then build as usual. Once done, the testing suite can be invoked with:

```
ctest
```

In the build directory.

## Documentation

Please refer to the [Guide](./doc/guide.md) for detailed documentation with
respect to the structure and usage of LHF.

API documentation can be generated using `gendoc.sh` in the project folder. You
must have Doxygen and the python package `json-schema-for-humans` installed in
your system.

```
bash gendoc.sh
```

After a successful invocation, the schema documentation for blueprint files
(please see the guide) and API documentation for LHF itself will be available
in `./doc_generated/schema` and `./doc_generated/doxygen` respectively.

## License

This project is currently licensed under the BSD 3-clause license. See
[LICENSE](./LICENSE) for more details.