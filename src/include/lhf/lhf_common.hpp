/**
 * @file lhf_common.hpp
 * @brief Any common components go into this file.
 */

#ifndef LHF_CONFIG_HPP
#define LHF_CONFIG_HPP

#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <functional>
#include <string>

#define LHF_VERSION_MAJOR "0"
#define LHF_VERSION_MINOR "5"
#define LHF_VERSION_PATCH "0"
#define LHF_VERSION_STRING (LHF_VERSION_MAJOR "." LHF_VERSION_MINOR "." LHF_VERSION_PATCH)

#define LHF_SORTED_VECTOR_BINARY_SEARCH_THRESHOLD 12
#define LHF_DEFAULT_BLOCK_SHIFT 5
#define LHF_DEFAULT_BLOCK_SIZE (1 << LHF_DEFAULT_BLOCK_SHIFT)
#define LHF_DEFAULT_BLOCK_MASK (LHF_DEFAULT_BLOCK_SIZE - 1)
#define LHF_DISABLE_INTERNAL_INTEGRITY_CHECK true

namespace lhf {

#define ____LHF__STR(x) #x
#define __LHF_STR(x) ____LHF__STR(x)

#ifdef LHF_ENABLE_DEBUG
#define LHF_DEBUG(x) { x };
#else
#define LHF_DEBUG(x)
#endif

using Size = std::size_t;

using String = std::string;

#ifdef LHF_ENABLE_PARALLEL

using RWMutex = std::shared_mutex;

using ReadLock = std::shared_lock<RWMutex>;

using WriteLock = std::lock_guard<RWMutex>;

#endif

using IndexValue = Size;

template<typename T>
using UniquePointer = std::unique_ptr<T>;

template<typename T>
using Vector = std::vector<T>;

template<typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template<typename K, typename V>
using OrderedMap = std::map<K, V>;

template<typename T>
using HashSet = std::unordered_set<T>;

template<typename T>
using OrderedSet = std::set<T>;

template<typename T>
using DefaultLess = std::less<T>;

template<typename T>
using DefaultHash = std::hash<T>;

template<typename T>
using DefaultEqual = std::equal_to<T>;

template<typename T>
struct DefaultPrinter {
	String operator()(T x) {
		std::stringstream s;
		s << x;
		return s.str();
	}
};

/**
 * @brief      Thrown if an Optional is accessed when the value is absent.
 */
struct AbsentValueAccessError : public std::invalid_argument {
	AbsentValueAccessError(const std::string &message):
		std::invalid_argument(message.c_str()) {}
};

/**
 * @brief      Describes an optional reference of some type T. The value may
 *             either be present or absent.
 *
 * @tparam     T    The type.
 */
template<typename T>
class OptionalRef {
	const T *value = nullptr;
	OptionalRef(): value(nullptr) {}

public:
	OptionalRef(const T &value): value(&value) {}

	/// Explicitly creates an absent value.
	static OptionalRef absent() {
		return OptionalRef();
	}

	/// Informs if the value is present.
	bool is_present() const {
		return value != nullptr;
	}

	/// Gets the underlying value. Throws an exception if it is absent.
	const T &get() {
		if (value) {
			return *value;
		} else {
			throw AbsentValueAccessError("Tried to access an absent value. "
				                         "A check is likely missing.");
		}
	}
};

/**
 * @brief      Describes an optional of some type T. The value may  either be
 *             present or absent.
 *
 * @tparam     T    The type.
 */
template<typename T>
class Optional {
	const T value{};
	const bool present;

	Optional(): present(false) {}

public:
	Optional(const T &value): value(value), present(true) {}

	/// Explicitly creates an absent value.
	static Optional absent() {
		return Optional();
	}

	/// Informs if the value is present.
	bool is_present() {
		return present;
	}

	/// Gets the underlying value. Throws an exception if it is absent.
	const T &get() {
		if (present) {
			return value;
		} else {
			throw AbsentValueAccessError("Tried to access an absent value. "
				                         "A check is likely missing.");
		}
	}
};

/**
 * @brief      Used to store a subset relation between two set indices.
 *             Because the index pair must be in sorted order to prevent
 *             duplicates, it necessitates this enum.
 */
enum SubsetRelation {
	UNKNOWN  = 0,
	SUBSET   = 1,
	SUPERSET = 2
};


// The index of the empty set. The first set that will ever be inserted
// in the property set value storage is the empty set.
static const constexpr Size EMPTY_SET_VALUE = 0;

/**
 * @brief      Composes a preexisting hash with another variable. Useful for
 *             Hashing containers. Adapted from `boost::hash_combine`.
 *
 * @param[in]  prev  The current hash
 * @param[in]  next  The value to compose with the current hash
 *
 * @tparam     T     Value type
 * @tparam     Hash  Hash for the value type
 *
 * @return     The composed hash
 */
template<typename T, typename Hash = DefaultHash<T>>
inline Size compose_hash(const Size prev, T next) {
	return prev ^ (Hash()(next) + 0x9e3779b9 + (prev << 6) + (prev >> 2));
}

};
#endif