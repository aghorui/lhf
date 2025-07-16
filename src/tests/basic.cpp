#include "common.hpp"
#include "lhf/lhf.hpp"
#include <gtest/gtest.h>
#include <iostream>

using LHF = LHFVerify<int>;
using Index = typename LHF::Index;

TEST(LHF_BasicChecks, empty_set_is_index_0) {
	LHF l;
	std::cout << l.dump() << std::endl;
	// Repetition is intentional
	ASSERT_EQ(l.register_set({}).value, lhf::EMPTY_SET_VALUE);
	ASSERT_NE(l.register_set({ 1, 2, 3, 4 }).value, lhf::EMPTY_SET_VALUE);
	ASSERT_EQ(l.register_set({}).value, lhf::EMPTY_SET_VALUE);
	ASSERT_EQ(l.register_set({}).value, lhf::EMPTY_SET_VALUE);
}

TEST(LHF_BasicChecks, index_is_empty_func_check) {
	LHF l;
	ASSERT_TRUE(l.register_set({}).is_empty());
}

TEST(LHF_BasicChecks, nonempty_set_is_not_index_0) {
	LHF l;
	ASSERT_NE(l.register_set({ 1, 2, 3, 4 }).value, lhf::EMPTY_SET_VALUE);
	ASSERT_EQ(l.register_set({}).value, lhf::EMPTY_SET_VALUE);
	ASSERT_NE(l.register_set({ 1, 2, 3, 4 }).value, lhf::EMPTY_SET_VALUE);
}

TEST(LHF_BasicChecks, set_index_is_consistent) {
	LHF l;
	ASSERT_EQ(l.register_set({ 1, 2, 3, 4 }).value, l.register_set({ 1, 2, 3, 4 }).value);
	ASSERT_EQ(l.register_set({ 1, 2, 3, 5 }).value, l.register_set({ 1, 2, 3, 5 }).value);
	ASSERT_NE(l.register_set({ 1, 2, 3, 5 }).value, l.register_set({ 1, 2, 3, 4 }).value);
}

TEST(LHF_BasicChecks, empty_operation_check) {
	LHF l;
	Index a = l.register_set({ });
	Index b = l.register_set({ });

	ASSERT_TRUE(l.set_union(a, b).is_empty());
	ASSERT_TRUE(l.set_intersection(a, b).is_empty());
	ASSERT_TRUE(l.set_difference(a, b).is_empty());
}

TEST(LHF_BasicChecks, set_union_integrity_check_empty) {
	LHF l;
	Index a = l.register_set({ });
	Index b = l.register_set({ 1, 2, 3, 5 });
	Index result = l.set_union(a, b);

	ASSERT_NE(result.value, lhf::EMPTY_SET_VALUE);
	ASSERT_EQ(b, result);
	ASSERT_NE(a, result);
	// ASSERT_EQ(l.is_subset(a, result), lhf::SubsetRelation::SUBSET);
	// ASSERT_EQ(l.is_subset(b, result), lhf::SubsetRelation::SUBSET);
}

TEST(LHF_BasicChecks, set_union_integrity_check) {
	LHF l;
	Index a = l.register_set({ 1, 2, 3, 4 });
	Index b = l.register_set({ 1, 2, 3, 5 });
	Index result = l.set_union(a, b);

	ASSERT_NE(result.value, lhf::EMPTY_SET_VALUE);
	ASSERT_NE(a, result);
	ASSERT_NE(b, result);
	ASSERT_EQ(result, l.register_set({ 1, 2, 3, 4, 5 }));
	ASSERT_EQ(result, l.set_union(b, a));
	// ASSERT_EQ(l.is_subset(a, result), lhf::SubsetRelation::SUBSET);
	// ASSERT_EQ(l.is_subset(b, result), lhf::SubsetRelation::SUBSET);

	Index c = l.register_set({ 1, 2, 3, 4, 5 });
	ASSERT_EQ(c, l.set_union(a, c));
	ASSERT_EQ(c, l.set_union(c, a));
}


TEST(LHF_BasicChecks, set_intersection_integrity_check_empty) {
	LHF l;
	Index a = l.register_set({ });
	Index b = l.register_set({ 1, 2, 3, 5 });
	Index result = l.set_intersection(a, b);

	ASSERT_TRUE(result.is_empty());
	// ASSERT_EQ(l.is_subset(a, result), lhf::SubsetRelation::SUPERSET);
	// ASSERT_EQ(l.is_subset(b, result), lhf::SubsetRelation::SUPERSET);
}

TEST(LHF_BasicChecks, set_intersection_integrity_check) {
	LHF l;
	Index a = l.register_set({ 1, 2, 3, 4 });
	Index b = l.register_set({ 1, 2, 3, 5 });
	Index result = l.set_intersection(a, b);

	ASSERT_FALSE(result.is_empty());
	ASSERT_NE(a, result);
	ASSERT_NE(b, result);
	ASSERT_EQ(result, l.register_set({ 1, 2, 3 }));
	ASSERT_EQ(result, l.set_intersection(b, a));
	// ASSERT_EQ(l.is_subset(a, result), lhf::SubsetRelation::SUBSET);
	// ASSERT_EQ(l.is_subset(b, result), lhf::SubsetRelation::SUBSET);

	Index c = l.register_set({ 1, 2, 3 });
	ASSERT_EQ(c, l.set_intersection(a, c));
	ASSERT_EQ(c, l.set_intersection(c, a));
}

TEST(LHF_BasicChecks, set_filter_check) {
	LHF l;


	Index a = l.register_set({ 1, 2, 3, 4, 99, 1002 });
	Index b = l.register_set({ 1, 2, 3, 5 });
	Index c = l.register_set({ 5 });

	LHF::UnaryOperationMap f1map;
	auto f1 = [](const LHF::PropertyElement &p){ return p.get_value() < 5; };

	LHF::UnaryOperationMap f2map;
	auto f2 = [](const LHF::PropertyElement &p){ return p.get_value() > 3; };

	Index d = l.set_filter(
		a,
		f1,
		f1map);
	ASSERT_EQ(l.size_of(d), 4);

	Index e = l.set_filter(
		b,
		f1,
		f1map);
	ASSERT_EQ(l.size_of(e), 3);

	Index e2 = l.set_filter(
		b,
		f2,
		f2map);
	ASSERT_EQ(l.size_of(e2), 1);

	Index f = l.set_filter(
		c,
		f1,
		f1map);
	ASSERT_TRUE(f.is_empty());
}


#ifdef LHF_ENABLE_DEBUG
TEST(LHF_BasicChecks, property_set_out_of_bounds_throws_exception) {
	LHF l;
	l.register_set({ 1, 2, 3, 4 });
	l.register_set_single(2);
	l.register_set_single(3);
	l.register_set_single(4);
	l.register_set_single(5);
	ASSERT_THROW(l.get_value(Index(99999999)), lhf::AssertError);
}
#endif