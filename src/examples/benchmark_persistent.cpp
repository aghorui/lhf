#include <cstdlib>
#include <iostream>
#include <cassert>
#include <random>

#include "lhf/lhf.hpp"

enum Operations {
	UNION = 0,
	DIFFERENCE,
	INTERSECTION,
	DECISION_MAX
};

using LHF = lhf::LatticeHashForest<int>;
using Index = LHF::Index;
using PropertySet = LHF::PropertySet;

#define PROPERTYSET_MIN_THRESHOLD 4

// std::clamp is implemented c++17 onwards
template<typename T>
T clamp(const T& v, const T& l, const T& h) {
	if (v > h) {
		return h;
	} else if (v < l) {
		return l;
	} else {
		return v;
	}
}

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: %s [num_unique_properties] [num_iterations] [distribution (optional) = 'normal' | 'uniform'] [stddev (optional)]\n", argv[0]);
		return 1;
	}

	double stddev = 0.1;
	bool use_normal_dist = false;
	int num_unique_properties = atoi(argv[1]);
	int num_iterations = atoi(argv[2]);

	if (argc >= 4) {
		std::string arg_str(argv[3]);
		if (arg_str == "uniform") {
			use_normal_dist = false;
		} else if (arg_str == "normal") {
			use_normal_dist = true;
		} else {
			std::cout << "Unknown Option: '" << arg_str << "'." << std::endl
			          << "Value must be 'normal' or 'uniform'." << std::endl;
		}
	}

	if (argc >= 5) {
		stddev = atof(argv[4]);
	}

	if (num_unique_properties <= 0 || num_iterations <= 0 || stddev <= 0) {
		std::cout << "All numerical arguments must be values greater than 0." << std::endl;
		return 1;
	}


	lhf::LatticeHashForest<int> l;

	std::random_device rd;
	std::mt19937 eng(rd());
	std::uniform_int_distribution<> unique_prop_gen(0, num_unique_properties - 1);
	std::bernoulli_distribution decision_gen(0.6);
	std::uniform_int_distribution<> operation_decision_gen(0, DECISION_MAX - 1);

	for (int i = 0; i < num_iterations; i++) {
		if (l.property_set_count() < PROPERTYSET_MIN_THRESHOLD || (decision_gen(eng))) {
			int unique_prop = unique_prop_gen(eng);
			// std::cout << "* Insert " << unique_prop <<  std::endl;
			l.register_set_single(unique_prop);
		} else {
			Index arg1;
			Index arg2;
			// std::cout << "* CValuesize " << l.property_sets.size() << std::endl;
			if (!use_normal_dist) {
				std::uniform_int_distribution<> index_gen(0, l.property_set_count() - 1);
				arg1 = index_gen(eng);
				arg2 = index_gen(eng);
			} else {
				std::normal_distribution<> index_gen(0, stddev);
				arg1 =
					clamp(
						Index(std::abs(index_gen(eng)) * (l.property_set_count() - 1)),
						Index(0),
						Index(l.property_set_count() - 1));
				arg2 =
					clamp(
						Index(std::abs(index_gen(eng)) * (l.property_set_count() - 1)),
						Index(0),
						Index(l.property_set_count() - 1));
			}

			switch (operation_decision_gen(eng)) {
			case UNION:
				// std::cout << "* Union " << arg1 << " " << arg2 << std::endl;
				l.set_union(arg1, arg2);
				break;

			case DIFFERENCE:
				// std::cout << "* Difference " << arg1 << " " << arg2 << std::endl;
				l.set_difference(arg1, arg2);
				break;

			case INTERSECTION:
				// std::cout << "* Intersection " << arg1 << " " << arg2 << std::endl;
				l.set_intersection(arg1, arg2);
				break;
			}
		}

		std::cout << "Completed: " << i << "/" << num_iterations << std::endl;
	}

	std::cout << l.dump_perf();
	// std::cout << l.dump();

	return 0;
}