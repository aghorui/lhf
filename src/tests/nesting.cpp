#include "lhf/lhf.hpp"
#include <gtest/gtest.h>
#include <utility>

using StringLHF = lhf::LatticeHashForest<std::string>;
using FloatLHF = lhf::LatticeHashForest<float>;
using TwoNestedLHF = lhf::LatticeHashForest<
	int,
	lhf::DefaultLess<int>,
	lhf::DefaultHash<int>,
	lhf::DefaultEqual<int>,
	lhf::DefaultPrinter<int>,
	lhf::NestingBase<int, StringLHF, FloatLHF>>;

TEST(LHF_NestingChecks, check_property_element_api) {
	StringLHF s;
	FloatLHF f;
	TwoNestedLHF l({s, f});

	std::string stra = "sad";
	std::string strb = "zxc";
	StringLHF::Index sa = s.register_set({ std::move(stra), std::move(strb) });
	FloatLHF::Index fa = f.register_set({123.23, 4213.324});

	TwoNestedLHF::Index ta = l.register_set({
		{1, {sa, fa}}
	});

	for (TwoNestedLHF::PropertyElement i : l.get_value(ta)) {
		std::cout << i.value0().value << std::endl;
		std::cout << i.value<1>().value << std::endl;
	}

	std::cout << s.dump() << std::endl;
	std::cout << f.dump() << std::endl;
	std::cout << l.dump() << std::endl;
}