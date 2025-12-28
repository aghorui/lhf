# Proposed Features

This document tracks proposed features that may be implemented in the future.

# LHF Configuration Struct
**Status: Nascent**

Instead of having inconveniently large number of template parameters for a
single LHF, we might encapsulate the parameters in a single struct. Users may
copy a skeleton of the struct and modify it for their own use.

```cpp
struct ConfigInt {
	using Property = int;
	using PropertyLess = lhf::DefaultLess<int>;
	using PropertyHash = lhf::DefaultHash<int>;
	using PropertyEqual = lhf::DefaultEqual<int>;
	using PropertyPrinter  = lhf::DefaultPrinter<int>;
};

// ...

using LHF = lhf::LatticeHashForest<ConfigInt, NestingBase<ConfigInt, OtherLHF>>;

// OR

struct ConfigFL {
	using Property = int;
	using PropertyLess = lhf::DefaultLess<int>;
	using PropertyHash = lhf::DefaultHash<int>;
	using PropertyEqual = lhf::DefaultEqual<int>;
	using PropertyPrinter  = lhf::DefaultPrinter<int>;
};
```