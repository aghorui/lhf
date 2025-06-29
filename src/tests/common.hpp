#include "lhf/lhf.hpp"
#include <gtest/gtest.h>
#include <utility>

template<>
struct std::hash<std::pair<float, int>> {
	std::size_t operator()(const std::pair<float, int>& k) const {
		return
			std::hash<lhf::IndexValue>()(k.first) ^
			(std::hash<lhf::IndexValue>()(k.second) << 1);
	}
};

template<typename T>
class LHFVerify: public lhf::LatticeHashForest<T> {
public:
	bool verify_relation_map_init_sizes() {
		return this->unions.size() == 0 &&
		       this->intersections.size() == 0 &&
		       this->differences.size() == 0 &&
		       this->subsets.size() == 0;
	}

	bool verify_relation_map_sizes(
		std::size_t unions, std::size_t intersections,
		std::size_t differences, std::size_t subsets) {
		return this->unions.size() == unions &&
		       this->intersections.size() == intersections &&
		       this->differences.size() == differences &&
		       this->subsets.size() == subsets;
	}
};

typedef ::testing::Types<
	bool,
	int,
	float,
	double,
	std::string,
	std::pair<float, int>> DefaultTestingTypes;