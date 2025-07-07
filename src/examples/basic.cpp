#include <iostream>
#include "lhf/lhf.hpp"

int main() {
	using LHF = lhf::LatticeHashForest<int>;
	using Index = LHF::Index;
	LHF l;

	Index a = l.register_set_single(2323);
	l.register_set_single(2323);

	Index b = l.register_set_single(2344);
	l.register_set_single(2322);

	Index c = l.register_set({ 23, 24, 25, 26 });
	Index d = l.register_set({ 23, 27 });

	l.set_union(a, b);
	l.set_union(b, a);
	l.set_difference(c, d);

	l.set_insert_single(d, 24);
	l.set_remove_single(c, 24);

	for (const LHF::PropertyElement &e : l.get_value(c)) {
		std::cout << e.to_string() << std::endl;
	}

	std::cout << l.dump() << std::endl;
	std::cout << l.dump_perf() << std::endl;
	return 0;
}