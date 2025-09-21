/**
 * @file lhf.hpp
 * @brief Defines the LatticeHashForest structure and related tools.
 */

#ifndef LHF_HPP
#define LHF_HPP

#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <string>

#ifdef LHF_ENABLE_PARALLEL
#include <atomic>
#include <mutex>
#include <shared_mutex>
#endif

#ifdef LHF_ENABLE_TBB
#include <tbb/tbb.h>
#include <tbb/concurrent_map.h>
#include <tbb/concurrent_vector.h>
#endif

#include "lhf_config.hpp"
#include "profiling.hpp"

namespace lhf {

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

/**
 * @brief      This struct contains the information about the operands of an
 *             operation (union, intersection, etc.)
 */
struct OperationNode {
	IndexValue left;
	IndexValue right;

	std::string to_string() const {
		std::stringstream s;
		s << "(" << left << "," << right << ")";
		return s.str();
	}

	bool operator<(const OperationNode &op) const {
		return (left < op.left) || (right < op.right);
	}

	bool operator==(const OperationNode &op) const {
		return (left == op.left) && (right == op.right);
	}
};

inline std::ostream &operator<<(std::ostream &os, const OperationNode &op) {
	return os << op.to_string();
}

};

/************************** START GLOBAL NAMESPACE ****************************/

template <>
struct std::hash<lhf::OperationNode> {
	lhf::Size operator()(const lhf::OperationNode& k) const {
		return
			std::hash<lhf::IndexValue>()(k.left) ^
			(std::hash<lhf::IndexValue>()(k.right) << 1);
	}
};

/************************** END GLOBAL NAMESPACE ******************************/

namespace lhf {

/**
 * @brief      Generic Less-than comparator for set types.
 *
 * @tparam     SetT          The set type (like std::set or std::unordered_set)
 * @tparam     ElementT      The element type of the set (the first template
 *                           param of SetT)
 * @tparam     PropertyLess  Comparator for properties
 */
template<typename OrderedSetT, typename ElementT, typename PropertyLess>
struct SetLess {
	bool operator()(const OrderedSetT *a, const OrderedSetT *b) const {
		PropertyLess less;

		auto cursor_1 = a->begin();
		const auto &cursor_end_1 = a->end();
		auto cursor_2 = b->begin();
		const auto &cursor_end_2 = b->end();

		while (cursor_1 != cursor_end_1 && cursor_2 != cursor_end_2) {
			if (less(*cursor_1, *cursor_2)) {
				return true;
			}

			if (less(*cursor_2, *cursor_1)) {
				return false;
			}

			cursor_1++;
			cursor_2++;
		}

		return a->size() < b->size();
	}
};

/**
 * @brief      Hasher for set types.
 *
 * @tparam     SetT      The set type (like std::set or std::unordered_set)
 * @tparam     ElementT  The element type of the set (the first template param
 *                       of SetT)
 * @tparam     ElementHash The hasher for the element.
 */
template<
	typename SetT,
	typename ElementT,
	typename ElementHash = DefaultHash<ElementT>>
struct SetHash {
	Size operator()(const SetT *k) const {
		// Adapted from boost::hash_combine
		size_t hash_value = 0;
		for (const auto &value : *k) {
			hash_value = compose_hash<ElementT, ElementHash>(hash_value, value);
		}

		return hash_value;
	}
};

/**
 * @brief      Generic Equality comparator for set types.
 *
 * @tparam     SetT      The set type (like std::set or std::unordered_set)
 * @tparam     ElementT  The element type of the set (the first template param
 *                       of SetT)
 * @tparam     PropertyEqual Equality Comparator for the Element.
 */
template<
	typename SetT,
	typename ElementT,
	typename PropertyEqual = DefaultEqual<ElementT>>
struct SetEqual {
	inline bool operator()(const SetT *a, const SetT *b) const {
		PropertyEqual eq;
		if (a->size() != b->size()) {
			return false;
		}

		if (a->size() == 0) {
			return true;
		}

		auto cursor_1 = a->begin();
		const auto &cursor_end_1 = a->end();
		auto cursor_2 = b->begin();

		while (cursor_1 != cursor_end_1) {
			if (!eq(*cursor_1, *cursor_2)) {
				return false;
			}

			cursor_1++;
			cursor_2++;
		}

		return true;
	}
};

#ifdef LHF_ENABLE_TBB

/**
 * @brief      Comparison operator specifically meant to accomodate TBB data
 *             structures. Please check out the more basic components before
 *             coming to this.
 *
 * @tparam     T      Element type. Typically PropertyElement.
 * @tparam     Hash   Hashing. Typically PropertySetHash.
 * @tparam     Equal  Equality. Typically PropertySetEqual.
 */
template<typename T, typename Hash, typename Equal>
struct TBBHashCompare {
	Size hash(const T v) const {
		return Hash()(v);
	}

	bool equal(const T a, const T b) const {
		return Equal()(a, b);
	}
};

#endif


/**
 * @brief      Struct that is thrown on an assertion failure.
 */
struct AssertError : public std::invalid_argument {
	AssertError(const std::string &message):
		std::invalid_argument(message.c_str()) {}
};

/**
 * @brief      Thrown if code region is unreachable.
 */
struct Unreachable : public std::runtime_error {
	Unreachable(const std::string &message = "Hit a branch marked as unreachable."):
		std::runtime_error(message.c_str()) {}
};

#define ____LHF__STR(x) #x
#define __LHF_STR(x) ____LHF__STR(x)
#define __LHF_EXCEPT(x) AssertError(x " [At: " __FILE__ ":" __LHF_STR(__LINE__) "]")

/**
 * @brief      Checks whether the the given container matches the expected
 *             form of a property set (sanity checking).
 *
 * @param[in]  cont
 */
template<typename PropertySetT, typename PropertyElementT>
static inline void verify_property_set_integrity(const PropertySetT &cont) {
	if (cont.size() == 1) {
		return;
	}

	std::unordered_set<
		PropertyElementT,
		typename PropertyElementT::Hash> k;
	const PropertyElementT *prev_val = nullptr;

	for (const PropertyElementT &val : cont) {
		if (prev_val && !(*prev_val < val)) {
			throw AssertError("Supplied property set is not sorted.");
		}

		if (k.count(val) > 0) {
			throw AssertError("Found duplicate key in given container.");
		} else {
			k.insert(val);
		}

		prev_val = &val;
	}
}

/**
 * @def        LHF_PROPERTY_SET_INTEGRITY_VALID(__set)
 * @brief      Macro and switch for `verify_property_set_integrity` inside LHF.
 * @param      __set  The property set.
 */

#ifndef LHF_DISABLE_INTEGRITY_CHECKS
#define LHF_PROPERTY_SET_INTEGRITY_VALID(__set) \
	verify_property_set_integrity<PropertySet, PropertyElement>(__set);
#else
#define LHF_PROPERTY_SET_INTEGRITY_VALID(__set)
#endif


/**
 * @def        LHF_PROPERTY_SET_INDEX_VALID(__index)
 * @brief      Check whether the index is a valid index within the property set.
 * @note       Signed/unsigned is made to be ignored here for the sanity checking.
 */

/**
 * @def        LHF_PROPERTY_SET_PAIR_VALID(__index1, __index2)
 * @brief      Check whether the pair of indexes is a valid within the property set.
 */

/**
 * @def        LHF_PROPERTY_SET_PAIR_UNEQUAL(__index1, __index2)
 * @brief      Check whether the pair of indexes are unequal. Used for sanity checking.
 */

#ifdef LHF_ENABLE_DEBUG

/// Check whether the index is a valid index within the property set.
/// @note Signed/unsigned is made to be ignored here for the sanity checking.
#define LHF_PROPERTY_SET_INDEX_VALID(__index) { \
	_Pragma("GCC diagnostic push"); \
	_Pragma("GCC diagnostic ignored \"-Wtype-limits\"") \
	if ((__index.value) < 0 || ((__index.value) > property_sets.size() - 1)) { \
		throw __LHF_EXCEPT("Invalid index supplied"); \
	} \
	_Pragma("GCC diagnostic pop") \
}

/// Check whether the pair of indexes is a valid within the property set.
#define LHF_PROPERTY_SET_PAIR_VALID(__index1, __index2) \
	LHF_PROPERTY_SET_INDEX_VALID(__index1); \
	LHF_PROPERTY_SET_INDEX_VALID(__index2);

/// Check whether the pair of indexes are unequal. Used for sanity checking.
#define LHF_PROPERTY_SET_PAIR_UNEQUAL(__index1, __index2) \
	if ((__index1) == (__index2)) { \
		throw __LHF_EXCEPT("Equal set condition not handled by caller"); \
	}

#else

#define LHF_PROPERTY_SET_INDEX_VALID(__index)
#define LHF_PROPERTY_SET_PAIR_VALID(__index1, __index2)
#define LHF_PROPERTY_SET_PAIR_UNEQUAL(__index1, __index2)

#endif

#if defined(LHF_ENABLE_PARALLEL) && !defined(LHF_ENABLE_TBB)
#define LHF_PARALLEL(__x) __x
#else
#define LHF_PARALLEL(__x)
#endif

#ifdef LHF_ENABLE_EVICTION
#define LHF_EVICTION(__x) __x
#else
#define LHF_EVICTION(__x)
#endif

/**
 * @def LHF_PUSH_ONE(__cont, __val)
 * @brief      Pushes one element to a PropertySet. Use this when implementing
 *             operations.
 *
 * @param      __cont  The container object
 * @param      __val   The value
 *
 */
#define LHF_PUSH_ONE(__cont, __val) (__cont).push_back((__val))

/**
 * @def LHF_PUSH_RANGE(__cont, __start, __end)
 * @brief      Pushes a range of elements to a PropertySet using an iterator.
 *             Use this when implementing operations.
 *
 * @param      __cont  The container object
 * @param      __start The start of the range (e.g. input.begin())
 * @param      __end   The end of the range (e.g. input.end())
 *
 */
#define LHF_PUSH_RANGE(__cont, __start, __end) (__cont).insert((__cont).end(), (__start), (__end))


/**
 * @def LHF_REGISTER_SET_INTERNAL(__set, __cold)
 * @brief      Registers a set with behaviours defined for internal processing.
 *
 * @param      __set   The set
 * @param      __cold  Whether this was a cold miss or not (pointer to bool)
 */
#define LHF_REGISTER_SET_INTERNAL(__set, __cold) register_set<LHF_DISABLE_INTERNAL_INTEGRITY_CHECK>((__set), (__cold))

/**
 * @brief      The nesting type for non-nested data structures. Act as "leaf"
 *             nodes in a tree of nested LHFs.
 *
 * @tparam     PropertyT  The 'key' property
 */
template<typename PropertyT>
struct NestingNone {
	/// Compile-time value that says this is not nested.
	static constexpr bool is_nested = false;

	/// Compile-time value that says there are no nested children.
	static constexpr Size num_children = 0;

	/// Placeholder value to mark the reference lists and value lists as empty.
	struct Empty {
		Empty() {}
	};

	/// Reference list. In the base case, there are no child LHFs.
	using LHFReferenceList = Empty;

	/// Child value list. In the base case, there are no child LHFs.
	using ChildValueList = Empty;

	/**
	 * @brief      Base-case type for the elements for a property set. The
	 *             template arguments are for the 'key' type.
	 *
	 * @tparam     PropertyLess     Custom less-than comparator (if required)
	 * @tparam     PropertyHash     Custom hasher (if required)
	 * @tparam     PropertyEqual    Custom equality comaparator (if required)
	 * @tparam     PropertyPrinter  PropertyT string representation generator
	 */
	template<
		typename PropertyLess,
		typename PropertyHash,
		typename PropertyEqual,
		typename PropertyPrinter>
	struct PropertyElement {
	public:
		/// Key type is made available here if required by user.
		using InterfaceKeyType = PropertyT;

		/// Value type is made available here if required by user.
		using InterfaceValueType = PropertyT;

	protected:
		PropertyT value;

	public:
		PropertyElement(const PropertyT &value): value(value) {}

		/**
		 * @brief      Gets the 'key' value.
		 *
		 * @return     The key.
		 */
		const PropertyT &get_key() const {
			return value;
		}

		/**
		 * @brief      Gets the nested value. In the base case it simply returns
		 *             the key.
		 *
		 * @return     The value.
		 */
		const PropertyT &get_value() const {
			return value;
		}

		/**
		 * @brief      Performs an "apply" operation. In the base case it is an
		 *             identity operation and does not do anything.
		 *
		 * @return     The property element.
		 */
		PropertyElement apply() const {
			return *this;
		}

		bool operator<(const PropertyElement &b) const {
			return PropertyLess()(value, b.value);
		}

		bool operator==(const PropertyElement &b) const {
			return PropertyEqual()(value, b.value);
		}

		String to_string() const {
			return PropertyPrinter()(value);
		}

		friend std::ostream& operator<<(std::ostream& os, const PropertyElement& obj) {
			os << obj.to_string();
			return os;
		}

		struct Hash {
			Size operator()(const PropertyElement &p) const {
				return PropertyHash()(p.value);
			}
		};

		/**
		 * @brief      This forces a comparison of both the key and the
		 *             value instead of only the key. Required in instances
		 *             where distinguishing this is necessary (like in the
		 *             property set storage array).
		 *
		 *             In the base case, this is identical to the normal `==`
		 *             operator.
		 */
		struct FullEqual {
			bool operator()(const PropertyElement &a, const PropertyElement &b) const {
				return PropertyEqual()(a.value, b.value);
			}
		};
	};

};

/**
 * @def        MapAdapter
 * @brief      Enables the interface used by LHF for using a map data structure
 *             for any given map implementation. This preprocessor if-ladder
 *             should be expanded as needed for any newer implementations.
 *
 * @tparam     MapClass The Map implementation to adapt.
 */

#ifdef LHF_ENABLE_TBB

template<typename MapClass>
class MapAdapter {
public:
	using Map = MapClass;
	using Key = typename Map::key_type;
	using MappedType = typename Map::mapped_type;
	using KeyValuePair = typename Map::value_type;

protected:
	using Accessor = typename Map::const_accessor;
	Map data;

public:
	Optional<MappedType> find(const Key &key) const {
		Accessor acc;
		bool found = data.find(acc, key);
		if (!found) {
			return Optional<MappedType>::absent();
		} else {
			return acc->second;
		}
	}

	void insert(KeyValuePair &&v) {
		data.insert(std::move(v));
	}

	Size size() const {
		return data.size();
	}

	typename Map::const_iterator begin() const {
		return data.begin();
	}

	typename Map::const_iterator end() const {
		return data.end();
	}

	String to_string() const {
		std::stringstream s;

		for (auto i : data) {
			s << "      {" << i.first << " -> " << i.second << "} \n";
		}

		return s.str();
	}

};

#else

template<typename MapClass>
class MapAdapter {
public:
	using Map = MapClass;
	using Key = typename Map::key_type;
	using MappedType = typename Map::mapped_type;
	using KeyValuePair = typename Map::value_type;

protected:
	Map data;
	LHF_PARALLEL(mutable RWMutex mutex;)

public:
	Optional<MappedType> find(const Key &key) const {
		LHF_PARALLEL(ReadLock m(mutex);)
		auto value = data.find(key);
		if (value == data.end()) {
			return Optional<MappedType>::absent();
		} else {
			return value->second;
		}
	}

	void insert(KeyValuePair &&v) {
		LHF_PARALLEL(WriteLock m(mutex);)
		data.insert(std::move(v));
	}

	Size size() const {
		LHF_PARALLEL(ReadLock m(mutex);)
		return data.size();
	}

	typename Map::const_iterator begin() const {
		return data.begin();
	}

	typename Map::const_iterator end() const {
		return data.end();
	}

	String to_string() const {
		LHF_PARALLEL(ReadLock m(mutex);)
		std::stringstream s;

		for (auto i : data) {
			s << "      {" << i.first << " -> " << i.second << "} \n";
		}

		return s.str();
	}

};

#endif


/**
 * @def        InternalMap
 * @brief      Convenience declaration for using simple maps in LHF.
 */

#ifdef LHF_ENABLE_TBB

template<typename K, typename V>
using  InternalMap = MapAdapter<tbb::concurrent_hash_map<K, V>>;

#else

template<typename K, typename V>
using InternalMap = MapAdapter<HashMap<K, V>>;

#endif

/**
 * Defines the map of operations in LHF. Template parameter can be used to set
 * an operation of any arity.
 */
template<typename T>
using OperationMap =  InternalMap<T, IndexValue>;


/**
 * @def        LHF_BINARY_NESTED_OPERATION(__op_name)
 * @brief      Declares a struct that enables the recursive/nesting behaviour of
 *             an operation.
 *
 * @param      __op_name  The operation name. Must match the function name
 *             (e.g. set_union, set_intersection etc.)
 *
 * @return     The set declaration.
 */
#define LHF_BINARY_NESTED_OPERATION(__op_name) \
	struct __NestingOperation_ ## __op_name { \
		template<typename ArgIndex, typename LHF> \
		void operator()(ArgIndex &ret, LHF &lhf, const ArgIndex &c, const ArgIndex &d) { \
			ret = lhf . __op_name (c, d); \
		} \
	};

/**
 * @def        LHF_PERFORM_BINARY_NESTED_OPERATION(__op_name, __reflist, __arg1, __arg2)
 * @brief      Actually enables the nesting. this must be placed in the
 *             operation body where the nested behaviour is needed.
 *
 * @param      __op_name  The operation name.
 * @param      __reflist  The reference list of the LHF
 * @param      __arg1     LHS argument of the binary operation
 * @param      __arg2     RHS argument of the binary operation
 *
 */
#define LHF_PERFORM_BINARY_NESTED_OPERATION(__op_name, __reflist, __arg1, __arg2) \
	((__arg1) . template apply<__NestingOperation_ ## __op_name>((__reflist), (__arg2)))

/**
 * @brief      Describes the standard nesting structure. Act as "non-leaf" nodes
 *             in a tree of nested LHFs.
 *
 * @tparam     PropertyT  The key property that the LHF acts upon.
 * @tparam     ChildT     the children of the property that are nested.
 */
template<typename PropertyT, typename ...ChildT>
struct NestingBase {
	/// Compile-time value that says this is nested.
	static constexpr bool is_nested = true;

	/// Compile-time value that says that the number of nested children
	/// are equal to the number of parameters in the template parameter pack.
	static constexpr Size num_children = sizeof...(ChildT);

	/// Reference list. References to all the nested LHFs are presented here.
	using LHFReferenceList = std::tuple<ChildT&...>;

	/// Child index tuple. This contains index types of all of the child LHFs.
	/// This is what is used to store indices to the nested values.
	using ChildValueList = std::tuple<typename ChildT::Index...>;

	/**
	 * @brief      Type for the elements for a property set in the nested. The
	 *             template arguments are for the 'key' type.
	 *
	 * @tparam     PropertyLess     Custom less-than comparator (if required)
	 * @tparam     PropertyHash     Custom hasher (if required)
	 * @tparam     PropertyEqual    Custom equality comaparator (if required)
	 * @tparam     PropertyPrinter  PropertyT string representation generator
	 */
	template<
		typename PropertyLess,
		typename PropertyHash,
		typename PropertyEqual,
		typename PropertyPrinter>
	struct PropertyElement {
	public:
		/// Key type is made available here if required by user.
		using InterfaceKeyType = PropertyT;

		/// Value type is made available here if required by user.
		using InterfaceValueType = PropertyT;

	private:
		PropertyT key;
		ChildValueList value;

	public:
		PropertyElement(
			const PropertyT &key,
			const ChildValueList &value):
			key(key),
			value(value) {}

		/**
		 * @brief      Gets the 'key' value.
		 *
		 * @return     The key.
		 */
		const PropertyT &get_key() const {
			return key;
		}


		/**
		 * @brief      Gets the nested value. It returns a tuple of values
		 *             containing indices to each of the nested sets of
		 *             different LHFs.
		 *
		 * @return     The value.
		 */
		const ChildValueList &get_value() const {
			return value;
		}

		/**
		 * @brief      Actually performs the template "apply" operation. This
		 *             sets up template syntax to go over each element of the
		 *             tuple statically to execute the operation specified by
		 *             the `Operation` template parameter.
		 *
		 * @note       This feature in particular requires C++14 and above.
		 *
		 * @param      ret        The list of results.
		 * @param[in]  lhf        The list of references to the LHF.
		 * @param[in]  arg_value  The list of values in the right-hand side
		 *                        operand.
		 *
		 * @tparam     Operation  The operation to perform on each child.
		 * @tparam     Indices    Parameter pack to enable template iteration.
		 */
		template <typename Operation, Size... Indices>
		void apply_internal(
			ChildValueList &ret,
			const LHFReferenceList &lhf,
			const ChildValueList &arg_value,
			std::index_sequence<Indices...>) const {
			((Operation()(std::get<Indices>(ret),
				std::get<Indices>(lhf),
				std::get<Indices>(value),
				std::get<Indices>(arg_value)
				)), ...);
		}

		/**
		 * @brief      Performs the "apply" operation on the list of nested
		 *             children specified by the `Operation` parameter.
		 *
		 * @param[in]  lhf        The list of references to the LHF.
		 * @param[in]  arg        The right-hand side operand.
		 *
		 * @tparam     Operation  The operation to perform.
		 *
		 * @return     A new `PropertyElement` that contains the result of the
		 *             apply operation.
		 */
		template<typename Operation>
		PropertyElement apply(
			const LHFReferenceList &lhf,
			const PropertyElement &arg) const {
			ChildValueList ret;
			apply_internal<Operation>(
				ret,
				lhf,
				arg.value,
				std::make_index_sequence<num_children>{});
			return PropertyElement(key, ret);
		}

		bool operator<(const PropertyElement &b) const {
			return PropertyLess()(key, b.key);
		}

		bool operator==(const PropertyElement &b) const {
			return PropertyEqual()(key, b.key);
		}

		String to_string() const {
			std::stringstream s;
			s << PropertyPrinter()(key);
			s << " -> [ ";
			std::apply(
				[&s](auto&&... args) {
					((s << args.value << ' '), ...);
				}, value);
			s << "]";
			return s.str();
		}

		friend std::ostream& operator<<(std::ostream& os, const PropertyElement& obj) {
			os << obj.to_string();
			return os;
		}

		struct Hash {
			Size operator()(const PropertyElement &p) const {
				return PropertyHash()(p.key);
			}
		};

		/**
		 * @brief      This forces a comparison of both the key and the
		 *             value instead of only the key. Required in instances
		 *             where distinguishing this is necessary (like in the
		 *             property set storage array)
		 */
		struct FullEqual {
			bool operator()(const PropertyElement &a, const PropertyElement &b) const {
				return PropertyEqual()(a.key, b.key) && (a.value == b.value);
			}
		};
	};
};

/**
 * @brief      Operation performance Statistics.
 */
struct OperationPerf {
	/// Number of direct hits (operation pair in map)
	size_t hits = 0;

	/// Number of equal hits (both arguments consist of the same set)
	size_t equal_hits = 0;

	/// Number of subset hits (operation pair not in but resolvable using
	/// subset relation)
	size_t subset_hits = 0;

	/// Number of empty hits (operation is optimised because at least one of
	/// the sets is empty)
	size_t empty_hits = 0;

	/// Number of cold misses (operation pair not in map, and neither
	/// resultant set in map. Neither node in lattice exists, nor the edges)
	size_t cold_misses = 0;

	/// Number of edge misses (operation pair not in map, but resultant set
	/// in map. Node in lattice exists, but not the edges)
	size_t edge_misses = 0;

	String to_string() const {
		std::stringstream s;
		s << "      " << "Hits       : " << hits << "\n"
		  << "      " << "Equal Hits : " << equal_hits << "\n"
		  << "      " << "Subset Hits: " << subset_hits << "\n"
		  << "      " << "Empty Hits : " << empty_hits << "\n"
		  << "      " << "Cold Misses: " << cold_misses << "\n"
		  << "      " << "Edge Misses: " << edge_misses << "\n";
		return s.str();
	}
};

/**
 * @def        LHF_PERF_INC(__oper, __category)
 * @brief      Increments the invocation count of the given category and operator.
 *
 * @note       Conditionally enabled if `LHF_ENABLE_PERFORMANCE_METRICS` is set.
 *
 * @param      __oper      The operator (e.g. `unions`)
 * @param      __category  The category (e.g. `hits`)
 */

#ifdef LHF_ENABLE_PERFORMANCE_METRICS
#define LHF_PERF_INC(__oper, __category) (perf[__LHF_STR(__oper)] . __category ++)
#else
#define LHF_PERF_INC(__oper, __category)
#endif


/**
 * @brief      The main LatticeHashForest structure.
 *             This class can be used as-is with a type or derived for
 *             additional functionality as needed
 *
 *
 * @tparam     PropertyT      The type of the property.
 *                            The property type must satisfy the following:
 *                            * It must be hashable with std::hash
 *                            * It must be less-than comparable
 *                            * It can be checked for equality
 *
 * @tparam     PropertyLess     Custom less-than comparator (if required)
 * @tparam     PropertyHash     Custom hasher (if required)
 * @tparam     PropertyEqual    Custom equality comaparator (if required)
 * @tparam     PropertyPrinter  PropertyT string representation generator
 * @tparam     Nesting          Nesting behaviour of the LHF
 */
template <
	typename PropertyT,
	typename PropertyLess = DefaultLess<PropertyT>,
	typename PropertyHash = DefaultHash<PropertyT>,
	typename PropertyEqual = DefaultEqual<PropertyT>,
	typename PropertyPrinter = DefaultPrinter<PropertyT>,
	typename Nesting = NestingNone<PropertyT>,
	Size BLOCK_SIZE = LHF_DEFAULT_BLOCK_SIZE,
	Size BLOCK_MASK = LHF_DEFAULT_BLOCK_MASK>
class LatticeHashForest {
public:
	/**
	 * @brief      Index returned by an operation. Being defined inside the
	 *             class ensures type safety and possible future extensions.
	 */
	struct Index {
		IndexValue value;

		Index(IndexValue idx = EMPTY_SET_VALUE): value(idx) {}

		bool is_empty() const {
			return value == EMPTY_SET_VALUE;
		}

		bool operator==(const Index &b) const {
			return value == b.value;
		}

		bool operator!=(const Index &b) const {
			return value != b.value;
		}

		bool operator<(const Index &b) const {
			return value < b.value;
		}

		bool operator>(const Index &b) const {
			return value > b.value;
		}

		String to_string() const {
			return std::to_string(value);
		}

		friend std::ostream& operator<<(std::ostream& os, const Index& obj) {
			os << obj.to_string();
			return os;
		}

		struct Hash {
			Size operator()(const Index &idx) const {
				return DefaultHash<IndexValue>()(idx.value);
			}
		};
	};

	/**
	 * The canonical element of property sets. Changes based on the nesting
	 * behaviour supplied.
	 */
	using PropertyElement =
		typename Nesting::template PropertyElement<
			PropertyLess,
			PropertyHash,
			PropertyEqual,
			PropertyPrinter>;

	/**
	 * The storage structure for property elements. Currently implemented as
	 * sorted vectors.
	 */
	using PropertySet = std::vector<PropertyElement>;

	using PropertySetHash =
		SetHash<
			PropertySet,
			PropertyElement,
			typename PropertyElement::Hash>;

	using PropertySetFullEqual =
		SetEqual<
			PropertySet,
			PropertyElement,
			typename PropertyElement::FullEqual>;

	/**
	 * The structure responsible for mapping property sets to their respective
	 * unique indices. When a key-value pair is actually inserted into the map,
	 * the key is a pointer to a valid storage location held by a member of
	 * the property set storage vector.
	 *
	 * @note The reason the 'key type' of the map is a pointer to a property set
	 *       is because of several reasons:
	 *
	 *       * Allows us to query arbitrary/user created property sets on the
	 *         map.
	 *       * Makes it not necessary to actually directly store the property
	 *         set as a key.
	 *       * Saves us from having to do some sort of complicated manuever to
	 *         reserve an index value temporarily and rewrite the hash and
	 *         equality comparators to retrieve the property sets from the
	 *         indices instead.
	 *
	 *       Careful handling, especially in the case of reallocating structures
	 *       like vectors is needed so that the address of the data does not
	 *       change. It must remain static for the duration of the existence of
	 *       the LHF instance.
	 */
#ifdef LHF_ENABLE_TBB
	using PropertySetMap =
		MapAdapter<tbb::concurrent_hash_map<
			const PropertySet *, IndexValue,
			TBBHashCompare<
				const PropertySet *,
				PropertySetHash,
				PropertySetFullEqual>>>;
#else
	using PropertySetMap =
		MapAdapter<std::unordered_map<
			const PropertySet *, IndexValue,
			PropertySetHash,
			PropertySetFullEqual>>;
#endif

	using UnaryOperationMap = OperationMap<IndexValue>;
	using BinaryOperationMap = OperationMap<OperationNode>;
	using RefList = typename Nesting::LHFReferenceList;

protected:
	RefList reflist;

#ifdef LHF_ENABLE_PERFORMANCE_METRICS
	PerformanceStatistics stat;
	HashMap<String, OperationPerf> perf;
#endif

	struct PropertySetHolder {
		using PtrContainer = UniquePointer<PropertySet>;
		using Ptr = typename PtrContainer::pointer;

		mutable PtrContainer ptr;

		PropertySetHolder(Ptr &&p): ptr(p) {}

		Ptr get() const {
			return ptr.get();
		}

		bool is_evicted() const {
#ifdef LHF_ENABLE_EVICTION
			return ptr.get() == nullptr;
#else
			return false;
#endif
		}

#ifdef LHF_ENABLE_EVICTION

		void evict() {
#if LHF_DEBUG
			if (!is_present()) {
				throw AssertError("Tried to evict an already absent property set");
			}
#endif
			ptr.release();
		}

		void reassign(Ptr &&p) {
#if LHF_DEBUG
			if (is_present()) {
				throw AssertError("Tried to reassign when a property set is already present");
			}
#endif
			ptr.reset(p);
		}

		void swap(PropertySetHolder &p) {
#if LHF_DEBUG
			if (is_present()) {
				throw AssertError("Tried to reassign when a property set is already present");
			} else if (!p.is_present()) {
				throw AssertError("Attempted to swap to a null pointer");
			}
#endif
			ptr.swap(p.ptr);
		}

#endif
	};

#if defined(LHF_ENABLE_TBB)

	class PropertySetStorage {
	protected:
		/// @note Not marking this as mutable will not allow us to get a
		///       non-const reference on index-based access. Non-constness
		//        is important for eviction to work.
		mutable tbb::concurrent_vector<PropertySetHolder> data = {};

	public:
		/**
		 * @brief      Retuns a mutable reference to the property set holder at
		 *             a given set index. This is useful for eviction based
		 *             functions.
		 *
		 * @param[in]  idx   Set index
		 *
		 * @return     A mutable propety set holder reference.
		 */
		PropertySetHolder &at_mutable(const Index &idx) const {
			return data.at(idx.value);
		}

		const PropertySetHolder &at(const Index &idx) const {
			return data.at(idx.value);
		}

		Index push_back(PropertySetHolder &&p) {
			auto it = data.push_back(std::move(p));
			return it - data.begin();
		}

		Size size() const {
			return data.size();
		}
	};

#elif defined(LHF_ENABLE_PARALLEL)

	class PropertySetStorage {
	protected:
		/// @note Not marking this as mutable will not allow us to get a
		///       non-const reference on index-based access. Non-constness
		//        is important for eviction to work.
		mutable Vector<Vector<PropertySetHolder>> data = {};

		mutable RWMutex mutex;
		mutable RWMutex realloc_mutex;

		std::atomic<Size> total_elems = 0;
		std::atomic<Size> block_base = 0;

	public:
		PropertySetStorage() {
			data.push_back({});
			data.back().reserve(BLOCK_SIZE);
		}

		/**
		 * @brief      Retuns a mutable reference to the property set holder at
		 *             a given set index. This is useful for eviction based
		 *             functions.
		 *
		 * @param[in]  idx   Set index
		 *
		 * @return     A mutable propety set holder reference.
		 */
		PropertySetHolder &at_mutable(const Index &idx) const {
			ReadLock m(realloc_mutex);
			if (idx.value >= block_base) {
				return data.back().at(idx.value & BLOCK_MASK);
			} else {
				// TODO fix this
				return data.at((idx.value & (~BLOCK_MASK)) >> 5).at(idx.value & BLOCK_MASK);
			}
		}

		const PropertySetHolder &at(const Index &idx) const {
			ReadLock m(realloc_mutex);
			if (idx.value >= block_base) {
				return data.back().at(idx.value & BLOCK_MASK);
			} else {
				// TODO fix this
				return data.at((idx.value & (~BLOCK_MASK)) >> 5).at(idx.value & BLOCK_MASK);
			}
		}

		Index push_back(PropertySetHolder &&p) {
			WriteLock m(mutex);
			if (data.back().size() >= BLOCK_SIZE) {
				WriteLock r(realloc_mutex);
				data.push_back({});
				data.back().reserve(BLOCK_SIZE);
				block_base += BLOCK_SIZE;
			}
			data.back().push_back(std::move(p));
			total_elems++;
			return Index(total_elems - 1);
		}

		Size size() const {
			return total_elems;
		}
	};

#else

	class PropertySetStorage {
	protected:
		/// @note Not marking this as mutable will not allow us to get a
		///       non-const reference on index-based access. Non-constness
		//        is important for eviction to work.
		mutable Vector<PropertySetHolder> data = {};

	public:
		/**
		 * @brief      Retuns a mutable reference to the property set holder at
		 *             a given set index. This is useful for eviction based
		 *             functions.
		 *
		 * @param[in]  idx   Set index
		 *
		 * @return     A mutable propety set holder reference.
		 */
		PropertySetHolder &at_mutable(const Index &idx) const {
			return data.at(idx.value);
		}

		const PropertySetHolder &at(const Index &idx) const {
			return data.at(idx.value);
		}

		Index push_back(PropertySetHolder &&p) {
			data.push_back(std::move(p));
			return data.size() - 1;
		}

		Size size() const {
			return data.size();
		}
	};

#endif

	// The property set storage array.
	PropertySetStorage property_sets = {};

	// The property set -> Index in storage array mapping.
	PropertySetMap property_set_map = {};

	BinaryOperationMap unions = {};
	BinaryOperationMap intersections = {};
	BinaryOperationMap differences = {};

	InternalMap<OperationNode, SubsetRelation> subsets = {};

	/**
	 * @brief      Stores index `a` as the subset of index `b` if a < b,
	 *             else stores index `a` as the superset of index `b`
	 *
	 * @param[in]  a     The index of the first set
	 * @param[in]  b     The index of the second set.
	 */
	void store_subset(const Index &a, const Index &b) {
		LHF_PROPERTY_SET_PAIR_VALID(a, b)
		LHF_PROPERTY_SET_PAIR_UNEQUAL(a, b)
		__lhf_calc_functime(stat);

		// We need to maintain the operation pair in index-order here as well.
		if (a > b) {
			subsets.insert({{b.value, a.value}, SUPERSET});
		} else {
			subsets.insert({{a.value, b.value}, SUBSET});
		}
	}

public:
	explicit LatticeHashForest(RefList reflist = {}): reflist(reflist) {
		// INSERT EMPTY SET AT INDEX 0
		register_set({ });
	}

	inline bool is_empty(const Index &i) const {
		return i.is_empty();
	}

	/**
	 * @brief      Returns whether we currently know whether a is a subset or a
	 *             superset of b.
	 *
	 * @param[in]  a     The first set
	 * @param[in]  b     The second set
	 *
	 * @return     Enum value telling if it's a subset, superset or unknown
	 */
	SubsetRelation is_subset(const Index &a, const Index &b) const {
		LHF_PROPERTY_SET_PAIR_VALID(a, b)

		auto i = subsets.find({a.value, b.value});

		if (!i.is_present()) {
			return UNKNOWN;
		} else {
			return i.get();
		}
	}

	/**
	 * @brief         Inserts a (or gets an existing) single-element set into
	 *                property set storage.
	 *
	 * @param[in]  c  The single-element property set.
	 *
	 * @return        Index of the newly created/existing set.
	 *
	 * @todo          Check whether the cache hit check can be removed.
	 */
	Index register_set_single(const PropertyElement &c) {
		__lhf_calc_functime(stat);

		PropertySetHolder new_set = PropertySetHolder(new PropertySet{c});

		auto result = property_set_map.find(new_set.get());

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));

			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).swap(new_set);
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			return Index(result.get());
		}
	}

	/**
	 * @brief      Inserts a (or gets an existing) single-element set into
	 *             the property set storage, and reports whether this set
	 *             was already  present or not.
	 *
	 * @param[in]  c     The single-element property set.
	 * @param[out] cold  Report if this was a cold miss.
	 *
	 * @return     Index of the newly created set.
	 */
	Index register_set_single(const PropertyElement &c, bool &cold) {
		__lhf_calc_functime(stat);

		PropertySetHolder new_set = PropertySetHolder(new PropertySet{c});
		auto result = property_set_map.find(new_set.get());

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));

			cold = true;
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).swap(new_set);
			cold = false;
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			cold = false;
			return Index(result.get());
		}
	}

	/**
	 * Deduplicates and sorts a vector (to function equivalently to a set).
	 *
	 * The deduplicate-sort function here is based on a stackoverflow answer
	 * (chosen for speed metrics): https://stackoverflow.com/a/24477023
	 *
	 * Ideally, this shouldn't be used or require being used.
	 */
	void prepare_vector_set(PropertySet &c) {
		std::unordered_set<
			PropertyElement,
			typename PropertyElement::Hash,
			typename PropertyElement::FullEqual> deduplicator;

		for (auto &i : c) {
			deduplicator.insert(i);
		}

		c.assign(deduplicator.begin(), deduplicator.end());
		std::sort(c.begin(), c.end());
	}

	template <bool disable_integrity_check = false>
	Index register_set(const PropertySet &c) {
		__lhf_calc_functime(stat);

		if (!disable_integrity_check) {
			LHF_PROPERTY_SET_INTEGRITY_VALID(c);
		}

		auto result = property_set_map.find(&c);

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			PropertySetHolder new_set(new PropertySet(c));
			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).reassign(new PropertySet(c));
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			return Index(result.get());
		}
	}

	template <bool disable_integrity_check = false>
	Index register_set(const PropertySet &c, bool &cold) {
		__lhf_calc_functime(stat);

		if (!disable_integrity_check) {
			LHF_PROPERTY_SET_INTEGRITY_VALID(c);
		}

		auto result = property_set_map.find(&c);

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			PropertySetHolder new_set(new PropertySet(c));
			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));

			cold = true;
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).reassign(new PropertySet(c));
			cold = false;
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			cold = false;
			return Index(result.get());
		}
	}

	template <bool disable_integrity_check = false>
	Index register_set(PropertySet &&c) {
		__lhf_calc_functime(stat);

		if (!disable_integrity_check) {
			LHF_PROPERTY_SET_INTEGRITY_VALID(c);
		}

		auto result = property_set_map.find(&c);

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			PropertySetHolder new_set(new PropertySet(c));
			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).reassign(new PropertySet(c));
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			return Index(result.get());
		}
	}

	template <bool disable_integrity_check = false>
	Index register_set(PropertySet &&c, bool &cold) {
		__lhf_calc_functime(stat);

		if (!disable_integrity_check) {
			LHF_PROPERTY_SET_INTEGRITY_VALID(c);
		}

		auto result = property_set_map.find(&c);

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			PropertySetHolder new_set(new PropertySet(c));
			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));

			cold = true;
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).reassign(new PropertySet(c));
			cold = false;
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			cold = false;
			return Index(result.get());
		}
	}

	template<typename Iterator, bool disable_integrity_check = false>
	Index register_set(Iterator begin, Iterator end) {
		__lhf_calc_functime(stat);

		PropertySetHolder new_set(new PropertySet(begin, end));

		if (!disable_integrity_check) {
			LHF_PROPERTY_SET_INTEGRITY_VALID(*new_set.get());
		}

		auto result = property_set_map.find(new_set.get());

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).reassign(std::move(new_set));
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			return Index(result.get());
		}
	}

	template<typename Iterator, bool disable_integrity_check = false>
	Index register_set(Iterator begin, Iterator end, bool &cold) {
		__lhf_calc_functime(stat);

		PropertySetHolder new_set(new PropertySet(begin, end));

		if (!disable_integrity_check) {
			LHF_PROPERTY_SET_INTEGRITY_VALID(*new_set.get());
		}

		auto result = property_set_map.find(new_set.get());

		if (!result.is_present()) {
			LHF_PERF_INC(property_sets, cold_misses);

			Index ret = property_sets.push_back(std::move(new_set));
			property_set_map.insert(std::make_pair(property_sets.at(ret).get(), ret.value));
			cold = true;
			return ret;
		}
		LHF_EVICTION(else if (is_evicted(result.get())) {
			property_sets.at_mutable(result.get()).reassign(std::move(new_set));
			cold = false;
			return Index(result.get());
		})
		else {
			LHF_PERF_INC(property_sets, hits);
			cold = false;
			return Index(result.get());
		}
	}

#ifdef LHF_ENABLE_EVICTION
	bool is_evicted(const Index &index) const {
		return property_sets.at(index.value).is_evicted();
	}
#endif

#ifdef LHF_ENABLE_EVICTION
	void evict_set(const Index &index) {
#ifdef LHF_DEBUG
		if (index.is_empty()) {
			throw AssertError("Tried to evict the empty set");
		}
#endif
		property_sets.at_mutable(index.value).evict();
	}
#endif

	/**
	 * @brief      Gets the actual property set specified by index.
	 *
	 * @param[in]  index  The index
	 *
	 * @return     The property set.
	 */
	inline const PropertySet &get_value(const Index &index) const {
		LHF_PROPERTY_SET_INDEX_VALID(index);
#if defined(LHF_DEBUG) && defined(LHF_ENABLE_EVICTION)
		if (is_evicted(index)) {
			throw AssertError("Tried to access and evicted set");
		}
#endif
		return *property_sets.at(index.value).get();
	}

	/**
	 * @brief      Returns the total number of property sets currently in the
	 *             LHF.
	 *
	 * @return     The count.
	 */
	inline Size property_set_count() const {
		return property_sets.size();
	}

	/**
	 * @brief      Returns the size of the set at `index`
	 *
	 * @param[in]  index  The index
	 *
	 * @return     size of the set.
	 */
	inline Size size_of(const Index &index) const {
		if (index == EMPTY_SET_VALUE) {
			return 0;
		} else {
			return get_value(index).size();
		}
	}

	/**
	 * @brief      Less than comparator for operations. You MUST use this
	 *             instead of directly using anything else like "<"
	 *
	 * @param[in]  a     LHS Property
	 * @param[in]  b     RHS Property
	 *
	 * @return     Result of doing a < b according to provided semantics.
	 */
	static inline bool less(const PropertyElement &a, const PropertyElement &b) {
		return a < b;
	}

	static inline bool less_key(const PropertyElement &a, const PropertyT &b) {
		return PropertyLess()(a.get_key(), b);
	}

	static inline bool less_key(const PropertyElement &a, const PropertyElement &b) {
		return PropertyLess()(a.get_key(), b.get_key());
	}

	/**
	 * @brief      Equality comparator for operations. You MUST use this
	 *             instead of directly using anything else like "<"
	 *
	 * @param[in]  a     LHS Property
	 * @param[in]  b     RHS Property
	 *
	 * @return     Result of doing a == b according to provided semantics.
	 */
	static inline bool equal(const PropertyElement &a, const PropertyElement &b) {
		return a == b;
	}

	static inline bool equal_key(const PropertyElement &a, const PropertyT &b) {
		return PropertyEqual()(a.get_key(), b);
	}

	static inline bool equal_key(const PropertyElement &a, const PropertyElement &b) {
		return PropertyEqual()(a.get_key(), b.get_key());
	}

	/**
	 * @brief      Finds a property element in the set based on the key
	 *             provided.
	 *
	 * @param[in]  index  Set Index
	 * @param[in]  p      Key
	 *
	 * @return     An optional that contains a property element if the key was
	 *             found.
	 */
	inline OptionalRef<PropertyElement> find_key(const Index &index, const PropertyT &p) const {
		if (is_empty(index)) {
			return OptionalRef<PropertyElement>::absent();
		}

		const PropertySet &s = get_value(index);

		if (s.size() <= LHF_SORTED_VECTOR_BINARY_SEARCH_THRESHOLD) {
			for (const PropertyElement &i : s) {
				if (equal_key(i, p)) {
					return OptionalRef<PropertyElement>(i);
				}
			}
		} else {
			// Binary search implementation
			long int low = 0;
			long int high = s.size() - 1;

			while (low <= high) {
				long int mid = low + (high - low) / 2;

				if (equal_key(s[mid], p)) {
					return OptionalRef<PropertyElement>(s[mid]);
				} else if (less_key(s[mid], p)) {
					low = mid + 1;
				} else {
					high = mid - 1;
				}
			}
		}

		return OptionalRef<PropertyElement>::absent();
	}

	/**
	 * @brief      Determines whether the property set at `index` contains the
	 *             element `prop` or not.
	 *
	 * @param[in]  index  The index
	 * @param[in]  prop   The property
	 *
	 * @return     `true` if `prop` is in `index`, false otherwise.
	 */
	inline bool contains(const Index &index, const PropertyElement &prop) const {
		if (is_empty(index)) {
			return false;
		}

		const PropertySet &s = get_value(index);

		if (s.size() <= LHF_SORTED_VECTOR_BINARY_SEARCH_THRESHOLD) {
			for (PropertyElement i : s) {
				if (equal_key(i, prop)) {
					return true;
				}
			}
		} else {
			// Binary search implementation
			long int low = 0;
			long int high = s.size() - 1;

			while (low <= high) {
				long int mid = low + (high - low) / 2;
				if (equal_key(s[mid], prop)) {
					return true;
				} else if (less_key(s[mid], prop)) {
					low = mid + 1;
				} else {
					high = mid - 1;
				}
			}
		}

		return false;
	}

	/**
	 * @brief      Calculates, or returns a cached result of the union
	 *             of `a` and `b`
	 *
	 * @param[in]  a     The first set
	 * @param[in]  b     The second set
	 *
	 * @return     Index of the new property set.
	 */
	LHF_BINARY_NESTED_OPERATION(set_union)
	Index set_union(const Index &_a, const Index &_b) {
		LHF_PROPERTY_SET_PAIR_VALID(_a, _b);
		__lhf_calc_functime(stat);

		if (_a == _b) {
			LHF_PERF_INC(unions, equal_hits);
			return Index(_a);
		}

		if (is_empty(_a)) {
			LHF_PERF_INC(unions, empty_hits);
			return Index(_b);
		} else if (is_empty(_b)) {
			LHF_PERF_INC(unions,empty_hits);
			return Index(_a);
		}

		const Index &a = std::min(_a, _b);
		const Index &b = std::max(_a, _b);

		SubsetRelation r = is_subset(a, b);

		if (r == SUBSET) {
			LHF_PERF_INC(unions, subset_hits);
			return Index(b);
		} else if (r == SUPERSET) {
			LHF_PERF_INC(unions, subset_hits);
			return Index(a);
		}

		auto result = unions.find({a.value, b.value});

		if (!result.is_present() LHF_EVICTION(|| is_evicted(result.get()))) {
			PropertySet new_set;
			const PropertySet &first = get_value(a);
			const PropertySet &second = get_value(b);

			// The union implementation here is adapted from the example
			// suggested implementation provided of std::set_union from
			// cppreference.com
			auto cursor_1 = first.begin();
			const auto &cursor_end_1 = first.end();
			auto cursor_2 = second.begin();
			const auto &cursor_end_2 = second.end();

			while (cursor_1 != cursor_end_1) {
				if (cursor_2 == cursor_end_2) {
					LHF_PUSH_RANGE(new_set, cursor_1, cursor_end_1);
					break;
				}

				if (less(*cursor_2, *cursor_1)) {
					LHF_PUSH_ONE(new_set, *cursor_2);
					cursor_2++;
				} else {
					if (!(less(*cursor_1, *cursor_2))) {
						if constexpr (Nesting::is_nested) {
							PropertyElement new_elem =
								LHF_PERFORM_BINARY_NESTED_OPERATION(
									set_union, reflist, *cursor_1, *cursor_2);
							LHF_PUSH_ONE(new_set, new_elem);
						} else {
							LHF_PUSH_ONE(new_set, *cursor_1);
						}
						cursor_2++;
					} else {
						LHF_PUSH_ONE(new_set, *cursor_1);
					}
					cursor_1++;
				}
			}

			LHF_PUSH_RANGE(new_set, cursor_2, cursor_end_2);

			bool cold = false;
			Index ret;

			LHF_EVICTION(if (result.is_present() && is_evicted(result.get())) {
				ret = result.get();
				property_sets.at_mutable(ret).reassign(new PropertySet(std::move(new_set)));
			} else) {
				ret = LHF_REGISTER_SET_INTERNAL(std::move(new_set), cold);

				unions.insert({{a.value, b.value}, ret.value});

				if (ret == a) {
					store_subset(b, ret);
				} else if (ret == b) {
					store_subset(a, ret);
				} else {
					store_subset(a, ret);
					store_subset(b, ret);
				}
			}

			if (cold) {
				LHF_PERF_INC(unions, cold_misses);
			} else {
				LHF_PERF_INC(unions, edge_misses);
			}

			return Index(ret);
		} else {
			LHF_PERF_INC(unions, hits);
			return Index(result.get());
		}
	}

	/**
	 * @brief      Inserts a single element from a given set (and returns the
	 *             index of the set). This is a wrapper over the union
	 *             operation.
	 *
	 * @param[in]  a     The set to insert the element to
	 * @param[in]  b     The element to be inserted.
	 *
	 * @return     Index of the new PropertySet.
	 */
	Index set_insert_single(const Index &a, const PropertyElement &b) {
		return set_union(a, register_set_single(b));
	}

	/**
	 * @brief      Calculates, or returns a cached result of the difference
	 *             of `a` from `b`
	 *
	 * @param[in]  a     The first set (what to subtract from)
	 * @param[in]  b     The second set (what will be subtracted)
	 *
	 * @return     Index of the new PropertySet.
	 */
	LHF_BINARY_NESTED_OPERATION(set_difference)
	Index set_difference(const Index &a, const Index &b) {
		LHF_PROPERTY_SET_PAIR_VALID(a, b);
		__lhf_calc_functime(stat);

		if (a == b) {
			LHF_PERF_INC(differences, equal_hits);
			return Index(EMPTY_SET_VALUE);
		}

		if (is_empty(a)) {
			LHF_PERF_INC(differences, empty_hits);
			return Index(EMPTY_SET_VALUE);
		} else if (is_empty(b)) {
			LHF_PERF_INC(differences, empty_hits);
			return Index(a);
		}

		auto result = differences.find({a.value, b.value});

		if (!result.is_present() LHF_EVICTION(|| is_evicted(result.get()))) {
			PropertySet new_set;
			const PropertySet &first = get_value(a);
			const PropertySet &second = get_value(b);

			// The difference implementation here is adapted from the example
			// suggested implementation provided of std::set_difference from
			// cppreference.com
			auto cursor_1 = first.begin();
			const auto &cursor_end_1 = first.end();
			auto cursor_2 = second.begin();
			const auto &cursor_end_2 = second.end();

			while (cursor_1 != cursor_end_1) {
				if (cursor_2 == cursor_end_2) {
					LHF_PUSH_RANGE(new_set, cursor_1, cursor_end_1);
					break;
				}

				if (less(*cursor_1, *cursor_2)) {
					LHF_PUSH_ONE(new_set, *cursor_1);
					cursor_1++;
				} else {
					if (!(less(*cursor_2, *cursor_1))) {
						if constexpr (Nesting::is_nested) {
							PropertyElement new_elem =
								LHF_PERFORM_BINARY_NESTED_OPERATION(
									set_difference, reflist, *cursor_1, *cursor_2);
							LHF_PUSH_ONE(new_set, new_elem);
						}
						cursor_1++;
					}
					cursor_2++;
				}
			}

			bool cold = false;
			Index ret;

			LHF_EVICTION(if (result.is_present() && is_evicted(result.get())) {
				ret = result.get();
				property_sets.at_mutable(ret).reassign(new PropertySet(std::move(new_set)));
			} else) {
				ret = LHF_REGISTER_SET_INTERNAL(std::move(new_set), cold);
				differences.insert({{a.value, b.value}, ret.value});

				if (ret != a) {
					store_subset(ret, a);
				} else {
					intersections.insert({
						{
							std::min(a.value, b.value),
							std::max(a.value, b.value)
						}, EMPTY_SET_VALUE});
				}
			}

			if (cold) {
				LHF_PERF_INC(differences, cold_misses);
			} else {
				LHF_PERF_INC(differences, edge_misses);
			}

			return Index(ret);
		} else {
			LHF_PERF_INC(differences, hits);
			return Index(result.get());
		}
	}

	/**
	 * @brief      Removes a single element from a given set (and returns the
	 *             index of the set). This is a wrapper over the diffrerence
	 *             operation.
	 *
	 * @param[in]  a     The set to remove the element from
	 * @param[in]  b     The element to be removed
	 *
	 * @return     Index of the new PropertySet.
	 */
	Index set_remove_single(const Index &a, const PropertyElement &b) {
		return set_difference(a, register_set_single(b));
	}

	/**
	 * @brief      Removes a single element from a given set if the "key"
	 *             element matches.
	 *
	 * @param[in]  a     The set to remove the element from
	 * @param[in]  p     The key of the element with that is to be removed
	 *
	 * @return     Index of the new PropertySet.
	 */
	Index set_remove_single_key(const Index &a, const PropertyT &p) {
		PropertySet new_set;
		const PropertySet &first = get_value(a);

		auto cursor_1 = first.begin();
		const auto &cursor_end_1 = first.end();

		for (;cursor_1 != cursor_end_1; cursor_1++) {
			if (!PropertyEqual()(cursor_1->get_key(), p)) {
				LHF_PUSH_ONE(new_set, *cursor_1);
			}
		}
		return register_set<true>(std::move(new_set));
	}

	/**
	 * @brief      Calculates, or returns a cached result of the intersection
	 *             of `a` and `b`
	 *
	 * @param[in]  a     The first set
	 * @param[in]  b     The second set
	 *
	 * @return     Index of the new property set.
	 */
	LHF_BINARY_NESTED_OPERATION(set_intersection)
	Index set_intersection(const Index &_a, const Index &_b) {
		LHF_PROPERTY_SET_PAIR_VALID(_a, _b);
		__lhf_calc_functime(stat);

		if (_a == _b) {
			LHF_PERF_INC(intersections, equal_hits);
			return Index(_a);
		}

		if (is_empty(_a) || is_empty(_b)) {
			LHF_PERF_INC(intersections, empty_hits);
			return Index(EMPTY_SET_VALUE);
		}

		const Index &a = std::min(_a, _b);
		const Index &b = std::max(_a, _b);

		SubsetRelation r = is_subset(a, b);

		if (r == SUBSET) {
			LHF_PERF_INC(intersections, subset_hits);
			return Index(a);
		} else if (r == SUPERSET) {
			LHF_PERF_INC(intersections, subset_hits);
			return Index(b);
		}

		auto result = intersections.find({a.value, b.value});

		if (!result.is_present() LHF_EVICTION(|| is_evicted(result.get()))) {
			PropertySet new_set;
			const PropertySet &first = get_value(a);
			const PropertySet &second = get_value(b);

			// The intersection implementation here is adapted from the example
			// suggested implementation provided for std::set_intersection from
			// cppreference.com
			auto cursor_1 = first.begin();
			const auto &cursor_end_1 = first.end();
			auto cursor_2 = second.begin();
			const auto &cursor_end_2 = second.end();

			while (cursor_1 != cursor_end_1 && cursor_2 != cursor_end_2)
			{
				if (less(*cursor_1,*cursor_2)) {
					cursor_1++;
				} else {
					if (!(less(*cursor_2, *cursor_1))) {
						if constexpr (Nesting::is_nested) {
							PropertyElement new_elem =
								LHF_PERFORM_BINARY_NESTED_OPERATION(set_intersection, reflist, *cursor_1, *cursor_2);
							LHF_PUSH_ONE(new_set, new_elem);
						} else {
							LHF_PUSH_ONE(new_set, *cursor_1);
						}
						cursor_1++;
					}
					cursor_2++;
				}
			}

			bool cold = false;
			Index ret;

			LHF_EVICTION(if (result.is_present() && is_evicted(result.get())) {
				ret = result.get();
				property_sets.at_mutable(ret).reassign(new PropertySet(std::move(new_set)));
			} else){
				ret = LHF_REGISTER_SET_INTERNAL(std::move(new_set), cold);
				intersections.insert({{a.value, b.value}, ret.value});

				if (ret != a) {
					store_subset(ret, a);
				} else if (ret != b) {
					store_subset(ret, b);
				} else {
					store_subset(ret, a);
					store_subset(ret, b);
				}
			}

			if (cold) {
				LHF_PERF_INC(intersections, cold_misses);
			} else {
				LHF_PERF_INC(intersections, edge_misses);
			}
			return Index(ret);
		}

		LHF_PERF_INC(intersections, hits);
		return Index(result.get());
	}


	/**
	 * @brief      Filters a set based on a criterion function.
	 *             This is supposed to be an abstract filtering mechanism that
	 *             derived classes will use to implement caching on a filter
	 *             operation rather than letting them implement their own.
	 *
	 * @param[in]  s            The set to filter
	 * @param[in]  filter_func  The filter function (can be a lambda)
	 * @param      cache        The cache to use (possibly defined by the user)
	 *
	 * @tparam     is_sort_bounded  Useful for telling the function that the
	 *                              filter criterion will have a lower and an
	 *                              upper bound in a sorted list. This can
	 *                              potentially result in a faster filtering.
	 *
	 * @todo Implement sort bound optimization
	 * @todo Implement bounding as a separate function instead
	 *
	 * @return     Index of the filtered set.
	 */
	Index set_filter(
		Index s,
		std::function<bool(const PropertyElement &)> filter_func,
		UnaryOperationMap &cache) {
		LHF_PROPERTY_SET_INDEX_VALID(s);
		__lhf_calc_functime(stat);

		if (is_empty(s)) {
			return s;
		}

		auto result = cache.find(s.value);

		if (!result.is_present() LHF_EVICTION(|| is_evicted(result.get()))) {
			PropertySet new_set;
			for (const PropertyElement &value : get_value(s)) {
				if (filter_func(value)) {
					LHF_PUSH_ONE(new_set, value);
				}
			}

			bool cold;
			Index ret;

			LHF_EVICTION(if (result.is_present() && is_evicted(result.get())) {
				ret = result.get();
				property_sets.at_mutable(ret).reassign(new PropertySet(std::move(new_set)));
			} else) {
				ret = LHF_REGISTER_SET_INTERNAL(std::move(new_set), cold);
				cache.insert(std::make_pair(s.value, ret.value));
			}

			if (cold) {
				LHF_PERF_INC(filter, cold_misses);
			} else {
				LHF_PERF_INC(filter, edge_misses);
			}

			return Index(ret);
		} else {
			LHF_PERF_INC(filter, hits);
			return Index(result.get());
		}
	}

	/**
	 * @brief      Converts the property set to a string.
	 *
	 * @param[in]  set   The set
	 *
	 * @return     The string representation of the set.
	 */
	static String property_set_to_string(const PropertySet &set) {
		std::stringstream s;
		s << "{ ";
		for (const PropertyElement &p : set) {
			s << p.to_string() << " ";
		}
		s << "}";

		return s.str();
	}

	String property_set_to_string(const Index &idx) const {
		return property_set_to_string(get_value(idx));
	}

	/**
	 * @brief      Dumps the current state of the LHF to a string.
	 */
	String dump() const {
		std::stringstream s;

		s << "{\n";

		s << "    " << "Unions: " << "(Count: " << unions.size() << ")\n";
		s << unions.to_string();
		s << "\n";

		s << "    " << "Differences:" << "(Count: " << differences.size() << ")\n";
		s << differences.to_string();
		s << "\n";

		s << "    " << "Intersections: " << "(Count: " << intersections.size() << ")\n";
		s << intersections.to_string();
		s << "\n";

		s << "    " << "Subsets: " << "(Count: " << subsets.size() << ")\n";
		for (auto i : subsets) {
			s << "      " << i.first << " -> " << (i.second == SUBSET ? "sub" : "sup") << "\n";
		}
		s << "\n";

		s << "    " << "PropertySets: " << "(Count: " << property_sets.size() << ")\n";
		for (size_t i = 0; i < property_sets.size(); i++) {
			s << "      "
				<< i << " : " << property_set_to_string(*property_sets.at(i).get()) << "\n";
		}
		s << "}\n";

		return s.str();
	}

	friend std::ostream& operator<<(std::ostream& os, const LatticeHashForest& obj) {
		os << obj.dump();
		return os;
	}

#ifdef LHF_ENABLE_PERFORMANCE_METRICS
	/**
	 * @brief      Dumps performance information as a string.
	 * @note       Conditionally enabled if `LHF_ENABLE_PERFORMANCE_METRICS` is
	 *             set.
	 * @return     String containing performance information as a human-readable
	 *             string.
	 */
	String dump_perf() const {
		std::stringstream s;
		s << "Performance Profile: \n";
		for (auto &p : perf) {
			s << p.first << "\n"
			  << p.second.to_string() << "\n";
		}
		s << stat.dump();
		return s.str();
	}
#endif

}; // END LatticeHashForest

/**
 * @brief      An LHF-like structure for scalar values. It does not implement
 *             any special operations besides deduplication.
 *
 * @tparam     PropertyT      The type of the property.
 *                            The property type must satisfy the following:
 *                            * It must be hashable with std::hash
 *                            * It must be less-than comparable
 *                            * It can be checked for equality
 *
 * @tparam     PropertyLess     Custom less-than comparator (if required)
 * @tparam     PropertyEqual    Custom equality comaparator (if required)
 * @tparam     PropertyPrinter  PropertyT string representation generator
 */
template <
	typename PropertyT,
	typename PropertyLess = DefaultLess<PropertyT>,
	typename PropertyHash = DefaultHash<PropertyT>,
	typename PropertyEqual = DefaultEqual<PropertyT>,
	typename PropertyPrinter = DefaultPrinter<PropertyT>>
struct Deduplicator {
	/**
	 * @brief      Index returned by an operation. The struct ensures type
	 *             safety and possible future extensions.
	 */
	struct Index {
		IndexValue value;

		Index(IndexValue idx = EMPTY_SET_VALUE): value(idx) {}

		bool empty() const {
			return value == EMPTY_SET_VALUE;
		}

		bool operator==(const Index &b) const {
			return value == b.value;
		}

		bool operator!=(const Index &b) const {
			return value != b.value;
		}

		bool operator<(const Index &b) const {
			return value < b.value;
		}

		bool operator>(const Index &b) const {
			return value > b.value;
		}
	};


	using PropertyMap =
		std::unordered_map<
			PropertyT *, IndexValue,
			PropertyHash,
			PropertyEqual>;

	// The property storage array.
	Vector<UniquePointer<PropertyT>> property_list = {};

	// The property -> Index in storage array mapping.
	PropertyMap property_map = {};

	/**
	 * @brief         Inserts a (or gets an existing) element into property
	                  storage.
	 *
	 * @param[in]  c  The single-element property set.
	 *
	 * @return        Index of the newly created/existing set.
	 *
	 * @todo          Check whether the cache hit check can be removed.
	 */
	Index register_value(const PropertyT &c) {

		UniquePointer<PropertyT> new_value =
			UniquePointer<PropertyT>(new PropertyT{c});

		auto cursor = property_map.find(new_value.get());

		if (cursor == property_map.end()) {
			// LHF_PERF_INC(property_sets, cold_misses);
			property_list.push_back(std::move(new_value));
			IndexValue ret = property_list.size() - 1;
			property_map.insert(std::make_pair(property_list[ret].get(), ret));
			return Index(ret);
		}

		// LHF_PERF_INC(property_sets, hits);
		return Index(cursor->second);
	}

};

}; // END NAMESPACE

#endif
