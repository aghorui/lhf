#include <iostream>

#include "lhf/lhf.hpp"

int main() {
	using ChildLHF =
		lhf::LatticeHashForest<double>;

	using ChildIndex = typename ChildLHF::Index;

	using LHF =
		lhf::LatticeHashForest<
			int,
			lhf::DefaultLess<int>,
			lhf::DefaultHash<int>,
			lhf::DefaultEqual<int>,
			lhf::DefaultPrinter<int>,
			lhf::NestingBase<int, ChildLHF>>;

	using Index = typename LHF::Index;

	ChildLHF cl;
	LHF l(LHF::RefList{cl});

	ChildIndex a = cl.register_set_single(212);
	ChildIndex b = cl.register_set({22, 33, 33});
	ChildIndex g = cl.register_set({ 34});

	Index c = l.register_set_single({12, {a}});
	Index f = l.register_set_single({12, {b}});
	Index e = l.register_set_single({12, {g}});

	l.set_union(c, f);
	l.set_intersection(c, f);
	l.set_intersection(f, e);
	l.set_intersection(c, f);

	l.set_difference(c, f);


	std::cout << cl.dump() << std::endl;
	std::cout << cl.dump_perf() << std::endl;
	std::cout << "======================" << std::endl;
	std::cout << l.dump() << std::endl;
	std::cout << l.dump_perf() << std::endl;

	return 0;
}