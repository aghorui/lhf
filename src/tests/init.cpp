#include "common.hpp"
#include <gtest/gtest.h>

template <typename T>
class LHF_InitCheck : public ::testing::Test {
public:
	using LHF = LHFVerify<T>;
	using Index = typename LHF::Index;
	LHF l;
};

TYPED_TEST_SUITE(LHF_InitCheck, DefaultTestingTypes);

TYPED_TEST(LHF_InitCheck, contains_only_one_elem) {
	ASSERT_EQ(this->l.property_set_count(), 1);
}

TYPED_TEST(LHF_InitCheck, sole_elem_is_empty) {
	using Index = typename LHF_InitCheck<TypeParam>::Index;
	ASSERT_EQ(this->l.get_value(Index(0)).size(), 0);
}

TYPED_TEST(LHF_InitCheck, operation_maps_are_empty) {
	ASSERT_TRUE(this->l.verify_relation_map_init_sizes());
}

TYPED_TEST(LHF_InitCheck, subset_is_empty) {
	ASSERT_TRUE(this->l.verify_relation_map_init_sizes());
}