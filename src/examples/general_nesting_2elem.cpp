#include <iostream>

#include "lhf/lhf.hpp"

int main() {
	using Child1LHF =
		lhf::LatticeHashForest<double>;

	using Child1Index = typename Child1LHF::Index;

	using Child2LHF =
		lhf::LatticeHashForest<int>;

	using Child2Index = typename Child2LHF::Index;

	using LHF =
		lhf::LatticeHashForest<
			int,
			lhf::DefaultLess<int>,
			lhf::DefaultHash<int>,
			lhf::DefaultEqual<int>,
			lhf::DefaultPrinter<int>,
			lhf::NestingBase<int, Child1LHF, Child2LHF>>;

	using Index = typename LHF::Index;

	Child1LHF cl1;
	Child2LHF cl2;
	LHF l({cl1, cl2});

	Child1Index a1 = cl1.register_set_single(212.23);
	Child1Index b1 = cl1.register_set({22.2, 33.4, 33.122});
	Child1Index g1 = cl1.register_set({34.121});

	Child2Index a2 = cl2.register_set_single(212);
	Child2Index b2 = cl2.register_set({22, 33, 34});
	Child2Index g2 = cl2.register_set({34});

	Index c = l.register_set_single({12, {a1, a2}});
	Index f = l.register_set_single({12, {b1, b2}});
	Index e = l.register_set_single({12, {g1, g2}});

	l.set_union(c, f);
	l.set_intersection(c, f);
	l.set_intersection(f, e);
	l.set_intersection(c, f);

	l.set_difference(c, f);

	std::cout << cl1.dump() << std::endl;
	std::cout << cl1.dump_perf() << std::endl;
	std::cout << "======================" << std::endl;
	std::cout << cl2.dump() << std::endl;
	std::cout << cl2.dump_perf() << std::endl;
	std::cout << "======================" << std::endl;
	std::cout << l.dump() << std::endl;
	std::cout << l.dump_perf() << std::endl;

	return 0;
}