#include "common.hpp"
#include "lhf/lhf.hpp"
#include <cmath>
#include <sstream>

#ifdef LHF_ENABLE_SERIALIZATION

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
	auto a = p.register_set({1, 2, 3});
	auto b = p.register_set({4, 5, 6});
	p.set_union(a, b);

	l.register_set({{2, {a, b}}});

	lhf::slz::JSON data = lhf::slz::lhf_to_json(l);
	std::cout << data << std::endl;

	PointeeLHF p2;
	PointerLHF l2({p2, p2});
	lhf::slz::lhf_from_json(l2, data);

	std::cout << l2.dump() << std::endl;

	ASSERT_EQ(l.dump(), l2.dump());
}

#endif