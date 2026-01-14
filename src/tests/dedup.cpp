#include "common.hpp"
#include "lhf/lhf.hpp"
#include <gtest/gtest.h>
#include <iostream>

struct Payload {
	int a;
	int b;
	std::string c;
	Payload(int a, int b, const std::string &c): a(a), b(b), c(c) {}

	bool operator<(const Payload &right) const {
		return make_tuple(a, b, c) < make_tuple(right.a, right.b, right.c);
	}

	bool operator==(const Payload &right) const {
		return make_tuple(a, b, c) == make_tuple(right.a, right.b, right.c);
	}

	friend std::ostream& operator<<(std::ostream& os, const Payload& obj) {
		os << "Payload(" << obj.a << ", " << obj.b << ", " << obj.c << ")";
		return os;
	}
};

template <>
struct std::hash<Payload> {
	lhf::Size operator()(const Payload& k) const {
		lhf::Size ret = std::hash<int>()(k.a);
		ret = lhf::compose_hash(ret, std::hash<int>()(k.b));
		ret = lhf::compose_hash(ret, std::hash<std::string>()(k.c));
		return ret;
	}
};

using Dedup = lhf::Deduplicator<Payload>;
using Index = Dedup::Index;

TEST(LHF_DeduplicatorBasicChecks, dedup_insertion) {
	Dedup dedup;

	Index r1 = dedup.register_value(Payload(4, 2, "abc"));
	Index r2 = dedup.register_value(Payload(7, 5, "abc"));
	Index r3 = dedup.register_value(Payload(9, 10, "abc"));

	// rvalue insertion
	Index a = dedup.register_value(Payload(1, 2, "abc"));
	Index b = dedup.register_value(Payload(1, 2, "abc"));
	EXPECT_EQ(a, b);
	EXPECT_NE(a, r1);
	EXPECT_NE(a, r2);
	EXPECT_NE(a, r3);


	Payload p = Payload(Payload(1, 2, "abc"));
	Index c = dedup.register_value(p);
	EXPECT_EQ(a, c);

	std::cout << dedup.dump() << std::endl;
}

TEST(LHF_DeduplicatorBasicChecks, dedup_ptr_insertion) {
	Dedup dedup;

	Payload *res1 = new Payload(3, 4, "pqr");
	Payload *res2 = new Payload(4, 5, "pqr");

	Index a = dedup.register_value(Payload(3, 4, "pqr"));
	Index b = dedup.register_ptr(res1);
	Index c = dedup.register_ptr(res2);

	EXPECT_EQ(a, b);
	EXPECT_NE(b, c);

	std::cout << dedup.dump() << std::endl;
}