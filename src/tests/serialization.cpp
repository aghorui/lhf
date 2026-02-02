#include "common.hpp"
#include "lhf/lhf.hpp"
#include <cmath>
#include <sstream>

using LHF = LHFVerify<lhf::LHFConfig<int>>;
using Index = typename LHF::Index;

TEST(LHF_SerializationChecks, check_serialization) {
	LHF l;
	Index a = l.register_set({1, 2, 3});
	Index b = l.register_set({4, 5, 6});
	l.set_union(a, b);
	std::cout << l.dump() << std::endl;
	auto j = l.to_json();
	LHF l2;
	l2.load_from_json(j);
	std::cout << l2.dump() << std::endl;
}

using PointeeLHF = lhf::LatticeHashForest<lhf::LHFConfig<int>>;
using PointerLHF = lhf::LatticeHashForest<
	lhf::LHFConfig<int>,
	lhf::NestingBase<int, PointeeLHF, PointeeLHF>
>;

TEST(LHF_SerializationChecks, check_lhf_walk) {
	PointeeLHF p;
	PointerLHF l({p, p});
	lhf::slz::JSON data = lhf::slz::to_json(l);
	std::cout << data << std::endl;

	PointerLHF l2({p, p});
	lhf::slz::load_from_json(l2, data);

	std::cout << l2.dump() << std::endl;
}