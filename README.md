# LatticeHashForest

LatticeHashForest (LHF) is a data representation caching mechanism that is meant
to facilitate aggressive caching of redundant data, and operations performed on
such redundant data. LHF aims to reduce both the memory footprint as well as the
CPU time used by an application for every operation performed on its data when
compared to naive storage and operations on it by reducing each unique instance
of data to a unique integer identifier, and reducing the cost of performing
any bulk operation on data as much as possible. It builds on the following
assumptions for some given information system:

1. Data is often repetitive.
2. Data is often redundantly computed.
3. Data is often sparse.
4. There are (usually) set patterns and common input data used for the
   computation of new data.

LHF finds its main applications in the domain of compilers and code
optimization, specifically in the field of data-flow analyses, such as Liveness
Analysis and Points-to Analysis.

The current LHF toolset consists of a C++ implementation of the mechanism, and
a Python script that generates an automatically set-up interface from a given
input description.

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

LHF is meant to be extended to fit the needs of a particular use case. The
class should be derived and more operations or facilities should be implemented
as required if in case the default LHF class does not meet all needs.

This project is still in active development.

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