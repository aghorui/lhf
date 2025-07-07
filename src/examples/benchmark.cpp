#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>

#include "lhf/lhf.hpp"

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s [test filename]\n", argv[0]);
		return 1;
	}

	enum {
		OPERTYPE,
		ARG1,
		ARG2,
		RESULT
	} state = OPERTYPE;

	using LHF = lhf::LatticeHashForest<int>;
	using Index = LHF::Index;
	using PropertySet = LHF::PropertySet;

	LHF l;
	PropertySet arg1;
	PropertySet arg2;
	PropertySet result;

	char opertype = '\0';

	std::ifstream file(argv[1]);

	if (file.bad()) {
		printf("File not found\n");
		exit(1);
	}


	unsigned int num_tests;
	std::string line;

	if (!std::getline(file, line)) {
		printf("Malformed file: Expected first line to be a numerical value.\n");
		return 1;
	}

	{
		std::istringstream s(line);
		s >> num_tests;
	}

	unsigned int count = 0;

	while (std::getline(file, line)) {
		std::istringstream s(line);

		switch (state) {
		case OPERTYPE: {
			std::string opertype_str;
			s >> opertype_str;
			opertype = opertype_str[0];
			//std::cout << opertype << " | " << opertype_str << std::endl;

			if(!(opertype == 'U' || opertype == 'I' || opertype == 'D')) {
				printf("Illegal operator: %c\n", opertype);
				exit(1);
			}
			state = ARG1;
			break;
		}

		case ARG1: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val;
				s >> val;
				LHF_PUSH_ONE(arg1, val);
			}
			state = ARG2;
			break;
		}

		case ARG2: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val;
				s >> val;
				LHF_PUSH_ONE(arg2, val);
			}
			state = RESULT;
			break;
		}

		case RESULT: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val;
				s >> val;
				LHF_PUSH_ONE(result, val);
			}

			// std::cout
			// 	<< "      ARG1:" << lhf::container_to_string(arg1)   << "\n"
			// 	<< "      ARG2:" << lhf::container_to_string(arg2)   << "\n";

			// for (auto &t : l.property_sets) {
			// 	std::cout << "          values:" << t.get()  << " " << lhf::ptr_container_to_string(t) << "\n";
			// }

			// std::cout << "          list size: " << l.property_sets.size() << "\n";
			// assert(l.property_sets.size() >= 1);
			// std::cout << "          list elem 0: " << lhf::ptr_container_to_string(l.property_sets[0]) << "\n";
			// assert(l.property_sets.size() >= 1);

#ifdef LHF_USE_SORTED_VECTOR_FOR_PROPERTY_SETS
			l.prepare_vector_set(result);
			l.prepare_vector_set(arg1);
			l.prepare_vector_set(arg2);
#endif

			Index larg1 = l.register_set(arg1);
			// assert(l.property_sets.size() >= 1);

			Index larg2 = l.register_set(arg2);
			// assert(l.property_sets.size() >= 1);

			Index new_set;
			switch (opertype) {
			case 'U': new_set = l.set_union(larg1, larg2); break;
			case 'I': new_set = l.set_intersection(larg1, larg2); break;
			case 'D': new_set = l.set_difference(larg1, larg2); break;
			}

			// std::cout << "Propset Index: " << l.register_set(result) << "\n";
			// std::cout << "Propset Index: " << l.register_set(result) << "\n";
			if (new_set != l.register_set(result)) {
				std::cout
					<< "Could not match the following operation: ";

				switch (opertype) {
				case 'U': std::cout << "UNION\n"; break;
				case 'I': std::cout << "INTERSECTION\n";  break;
				case 'D': std::cout << "DIFFERENCE\n";  break;
				}

				std::cout
					<< "      ARG1:" << l.property_set_to_string(arg1)   << "\n"
					<< "      ARG2:" << l.property_set_to_string(arg2)   << "\n"
					<< "  EXPECTED:" << l.property_set_to_string(result) << "\n"
					<< "       GOT:" << l.property_set_to_string(l.get_value(new_set)) << "\n";

				return 1;
			}

			count++;
			printf("Completed %d/%d\n", count, num_tests);
			if (count >= num_tests) {
				goto end;
			}
			arg1.clear();
			arg2.clear();
			result.clear();
			state = OPERTYPE;
			break;
		}

		}
	}

end:
	std::cout << l.dump_perf();
	return 0;
}