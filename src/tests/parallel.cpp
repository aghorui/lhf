#include <chrono>
#include <thread>
#include "common.hpp"
#include <gtest/gtest.h>

using LHF = LHFVerify<int>;
using Index = typename LHF::Index;

#if defined(LHF_ENABLE_TBB) || defined(LHF_ENABLE_PARALLEL)

TEST(LHF_ParallelChecks, parallel_stress_test) {
	LHF l;
	std::thread t1([&](){
		for (int i = 0; i < 10000; i++) {
			l.register_set_single(i);
		}
	});

	std::thread t2([&](){
		for (int i = 0; i < 1000; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			l.set_union(Index(l.property_set_count() / 2), Index(l.property_set_count() * 3 / 4));
			l.set_difference(Index(l.property_set_count() / 6), Index(l.property_set_count() / 4));
			l.set_intersection(Index(l.property_set_count() / 5), Index(l.property_set_count() / 3));
		}
	});
	std::thread t3([&](){
		for (int i = 0; i < 1000; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			l.set_union(Index(l.property_set_count() / 6), Index(l.property_set_count() / 4));
			l.set_difference(Index(l.property_set_count() / 2), Index(l.property_set_count() * 3 / 4));
			l.set_intersection(Index(l.property_set_count() / 3), Index(l.property_set_count() / 5));
		}
	});
	std::thread t4([&](){
		for (int i = 0; i < 1000; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			l.set_union(Index(l.property_set_count() / 7), Index(l.property_set_count() / 4));
			l.set_difference(Index(l.property_set_count() / 4), Index(l.property_set_count() / 3));
			l.set_intersection(Index(l.property_set_count() / 6), Index(l.property_set_count() * 2 / 3));
		}
	});


	t1.join();
	t2.join();
	t3.join();
	t4.join();

	std::cout << l.dump() << std::endl;
	std::cout << l.dump_perf() << std::endl;
}

#endif