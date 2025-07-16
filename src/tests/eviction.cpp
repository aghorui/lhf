#include "common.hpp"
#include <gtest/gtest.h>

using LHF = LHFVerify<int>;
using Index = typename LHF::Index;

#ifdef LHF_ENABLE_EVICTION

TEST(LHF_EvictionChecks, basic_eviction_test) {
	LHF l;
	Index a = l.register_set({1, 2, 3, 4, 5, 6, 7});
	ASSERT_FALSE(l.is_evicted(a));
	l.evict_set(a);
	ASSERT_TRUE(l.is_evicted(a));
}

#ifdef LHF_ENABLE_DEBUG

TEST(LHF_EvictionChecks, exception_thrown_on_evicting_empty_set) {
	LHF l;
	ASSERT_THROW(l.evict_set(0), lhf::AssertError);
}

#endif

TEST(LHF_EvictionChecks, eviction_stress_test) {
	LHF l;
	for (int i = 20; i < 10020; i++) {
		l.register_set({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, i });
	}

	for (int i = 1; i < 10001; i++) {
		l.evict_set(i);
	}
}

TEST(LHF_EvictionChecks, eviction_operation_test) {
	LHF l;
	Index a = l.register_set({1, 2});
	Index b = l.register_set({2, 3});
	Index c = l.set_union(a, b);

	ASSERT_FALSE(l.is_evicted(c));
	l.evict_set(c);
	ASSERT_TRUE(l.is_evicted(c));
	l.set_union(a, b);
	ASSERT_FALSE(l.is_evicted(c));
}

#endif