#include "common.hpp"
#include "lhf/lhf.hpp"
#include <cmath>
#include <sstream>

struct Cmplx {
	float real;
	float cmplx;

	float magnitude() const {
		return std::sqrt(real * real + cmplx * cmplx);
	}

	bool operator<(const Cmplx &b) const {
		return magnitude() < b.magnitude();
	}

	bool operator==(const Cmplx &b) const {
		return magnitude() == b.magnitude();
	}

	std::string to_string() const {
		std::stringstream s;
		s << real << "+ i" << cmplx;
		return s.str();
	}

	struct Hash {
		lhf::Size operator()(const Cmplx &a) const {
			return std::hash<float>()(a.magnitude());
		}
	};

	struct Printer {
		std::string operator()(const Cmplx &a) const {
			return a.to_string();
		}
	};

	struct LHFConfig : lhf::LHFConfig<Cmplx> {
		using PropertyPrinter = Printer;
		using PropertyHash = Hash;
	};
};

using LHF = lhf::LatticeHashForest<Cmplx::LHFConfig>;
using Index = LHF::Index;

TEST(LHF_ConfigStructTests, basic_config_usage_test) {
	LHF l;

	Index a = l.register_set_single(Cmplx{1, 2});
	Index b = l.register_set({Cmplx{2, 2}, Cmplx{3, 3}, Cmplx{4, 4}});
	Index c = l.set_union(a, b);
	Index d = l.set_union(b, a);
	EXPECT_EQ(c, d);

	std::cout << l.dump() << "\n";
}