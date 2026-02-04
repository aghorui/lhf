/**
 * @file lhf_serialization.hpp
 * @brief Describes serialization building blocks for LHF.
 *
 * @note You may notice that a majority of these functions could be inside the
 * LHF class itself. These functions are added here, and largely as templates
 * for a clean separation of concerns and dependencies from the actual LHF
 * structure. The second factor is that the serialization routine needs to
 * traverse the DAG of LHFs in order to extract all of the data, and thus is
 * made a separate entity from LHF rather than a member function.
 */

#ifndef LHF_SERIALIZATION_H
#define LHF_SERIALIZATION_H

#include "lhf_common.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace lhf {

namespace slz {

using JSON = nlohmann::json;

/**
 * @brief      Struct for serializing/deserializing values in LHF.
 *
 * @tparam     T the value to serialize.
 */
template<typename T>
struct ValueSerializer {
	JSON save(const T &val);
	T load(const JSON &j);
};


/**
 * @brief      Default serialization behavior. If the type of the property in
 *             LHF is something that can be directly serialized to JSON (like
 *             numbers, strings), or has a serializer routine already written
 *             for it (see nlohmann/json documentation), this struct will do so.
 *
 * @tparam     T the value to serialize.
 */
template<typename T>
struct DefaultValueSerializer : ValueSerializer<T> {
	JSON save(const T &val) {
		return JSON(val);
	}

	T load(const JSON &j) {
		return j.get<T>();
	}
};

struct SerializationError : public std::invalid_argument {
	SerializationError(const String &message):
		std::invalid_argument(message.c_str()) {}
};

/**
 * @brief      Converts OperationNode -> integer maps to JSON.
 *
 * @param[in]  map   a BinaryOperationMap or analogue.
 *
 * @return     The JSON representation.
 */
template<typename MapT>
JSON binary_operation_map_to_json(const MapT &map) {
	JSON ret = JSON::array();
	for (auto &i : map) {
		ret.push_back(JSON::array({JSON::array({i.first.left, i.first.right}), i.second}));
	}
	return ret;
}

/**
 * @brief      Inserts data from the JSON representation to the equivalent C++
 *             data strucure for the OperationNode -> integer map.
 *
 * @param[in]  map   a BinaryOperationMap or analogue.
 * @param[in]  obj   The JSON object.
 *
 */
template<typename MapT>
void binary_operation_map_from_json(MapT &map, const JSON &obj) {
	map.clear();

	if (!obj.is_array()) {
		throw SerializationError("Expected array (root)");
	}

	for (auto &tuple : obj) {
		if (!tuple.is_array() || !(tuple.size() == 2)) {
			throw SerializationError("Expected array of size 2 (root[*])");
		} else if (!tuple[0].is_array() || !(tuple[0].size() == 2)) {
			throw SerializationError("Expected array of size 2 (root[*][0])");
		} else if (!tuple[0][0].is_number_integer() || !tuple[0][1].is_number_integer()) {
			throw SerializationError("Expected array of size 2 (root[*][0][0,1])");
		} else if (!tuple[1].is_number_integer()) {
			throw SerializationError("Expected array of size 2 (root[*][1])");
		}
		map.insert({{tuple[0][0], tuple[0][1]}, tuple[1]});
	}
}

/**
 * @brief      Converts an integer -> integer map to JSON
 *
 * @param[in]  map   The UnaryOperationMap or an analogue.
 *
 * @return     The JSON representation.
 */
template<typename MapT>
JSON unary_operation_map_to_json(const MapT &map) {
	JSON ret = JSON::array();
	for (auto &i : map) {
		JSON::array({i.first, i.second});
	}
	return ret;
}

/**
 * @brief      Inserts data from the JSON representation to the equivalent C++
 *             data strucure for the integer -> integer map.
 *
 * @param[in]  map   a UnaryOperationMap or analogue.
 * @param[in]  obj   The JSON object.
 *
 */
template<typename MapT>
void unary_operation_map_from_json(MapT &map, const JSON &obj) {
	map.clear();

	if (!obj.is_array()) {
		throw SerializationError("Expected array (root)");
	}

	for (auto &tuple : obj) {
		if (!tuple.is_array() || !(tuple.size() == 2)) {
			throw SerializationError("Expected array of size 2 (root[*])");
		} else if (!tuple[0].is_number_integer() || !tuple[1].is_number_integer()) {
			throw SerializationError("Expected array of size 2 (root[*][0,1])");
		}
		map.insert({tuple[0], tuple[1]});
	}
}

/**
 * @brief      Converts a storage array to its JSON representation.
 *
 * @param[in]  store       A PropertySetStorage or an analogue.
 * @param      serializer  A serializer structure.
 *
 * @return     The JSON representation.
 */
template<typename StoreT, typename Serializer>
JSON storage_array_to_json(const StoreT &store, Serializer &serializer) {
	JSON ret = JSON::array();
	for (Size set_index = 0; set_index < store.size(); set_index++) {
		JSON set_arr = JSON::array();
		for (auto &elem : *store.at(set_index).get()) {
			set_arr.push_back(serializer.save(elem.get_key()));
		}
		ret.push_back(set_arr);
	}
	return ret;
}

/**
 * @brief      Inserts data from the JSON representation to the equivalent C++
 *             data strucure for a PropertySetStorage. It needs direct access
 *             to the LHF object to use `register_set`.
 *
 * @param      lhf         The LHF object
 * @param[in]  obj         The JSON representation
 * @param      serializer  The serializer
 */
template<typename LHFT, typename Serializer>
void register_storage_from_json(LHFT &lhf, const JSON &obj, Serializer &serializer) {

	using PropertyElement = typename LHFT::PropertyElement;

	if (!obj.is_array()) {
		throw SerializationError("Expected array (root)");
	}

	for (Size set_index = 0; set_index < obj.size(); set_index++) {
		Vector<PropertyElement> data;
		if (!obj[set_index].is_array()) {
			throw SerializationError("Expected array (root[*])");
		}
		for (auto &i : obj[set_index]) {
			data.push_back(PropertyElement(serializer.load(i)));
		}
		lhf.register_set(std::move(data));
	}
}

/**
 * @brief      Converts an LHF storage array to JSON in the nested case.
 *
 * @param[in]  store       A PropertySetStorage object or analogue.
 * @param[in]  serializer  A Serializer object.
 *
 * @return     The JSON Representation
 */
template<typename Serializer, typename StoreT>
JSON storage_array_to_json_nested(const StoreT &store, Serializer &serializer) {
	JSON ret = JSON::array();

	for (Size set_index = 0; set_index < store.size(); set_index++) {
		JSON set_arr = JSON::array();

		for (auto &elem : *store.at(set_index).get()) {
			JSON child_arr = JSON::array();

			std::apply([&child_arr](const auto&... args) {
				(child_arr.push_back(args.value), ...);
			}, elem.get_value());

			set_arr.push_back(
				JSON::array({
					serializer.save(elem.get_key()),
					child_arr
				})
			);
		}
		ret.push_back(set_arr);
	}
	return ret;
}

template <typename Tuple, std::size_t... Is>
Tuple json_list_to_tuple_internal(const JSON& l, std::index_sequence<Is...>) {
	return Tuple{ (l[Is].template get<size_t>())... };
}

template <typename Tuple>
Tuple json_list_to_tuple(const JSON& l) {
	constexpr std::size_t N = std::tuple_size_v<Tuple>;
	return json_list_to_tuple_internal<Tuple>(l, std::make_index_sequence<N>{});
}

/**
 * @brief      Inserts data from the JSON representation to the equivalent C++
 *             data strucure for a PropertySetStorage in the nested case. It
 *             needs direct access to the LHF object to use `register_set`.
 *
 * @param      lhf         The LHF object
 * @param[in]  obj         The JSON representation
 * @param      serializer  The serializer
 */
template<typename Serializer, typename LHFT>
void register_storage_from_json_nested(LHFT &lhf, const JSON &obj, Serializer &serializer) {

	using PropertyElement = typename LHFT::PropertyElement;
	using ChildValueList = typename LHFT::Nesting::ChildValueList;
	constexpr const Size num_children = LHFT::Nesting::num_children;

	if (!obj.is_array()) {
		throw SerializationError("Expected array (root)");
	}

	for (Size set_index = 0; set_index < obj.size(); set_index++) {
		Vector<PropertyElement> data;
		if (!obj[set_index].is_array()) {
			throw SerializationError("Expected array (root[*])");
		}
		for (auto &i : obj[set_index]) {
			if (i.size() != 2) {
				throw SerializationError("Expected array of size 2 (root[*][*])");
			} else if (!i[1].is_array() || i[1].size() != num_children) {
				throw SerializationError(
					"Expected array of size " + std::to_string(num_children) +
					" (root[*][1])");
			}
			auto cvl = json_list_to_tuple<ChildValueList>(i[1]);
			data.push_back(PropertyElement(serializer.load(i[0]), cvl));
		}
		lhf.register_set(std::move(data));
	}
}

template<typename LHFT>
void lhf_to_json_internal(
	LHFT &root, JSON &obj,
	HashSet<void *> &visited, String &path) {

	if (visited.count(&root) > 0) {
		// LHF_DEBUG(std::cout << "Already found '" << root.name << "' <" << &root << ">\n";);
		// LHF_DEBUG(std::cout << "Path: " << path << "\n";);
		return;
	} else {
		// LHF_DEBUG(std::cout << "Visited '" << root.name << "' <" << &root << ">\n";);
		// LHF_DEBUG(std::cout << "Path: " << path << "\n";);
		visited.insert(&root);
	}

	obj[path] = root.to_json();

	std::apply([&](auto&... child_refs) {
		// Local counter for this node's children
		[[maybe_unused]] Size i = 0;

		// The fold expression calls this lambda for EACH child in the pack
		([&](auto& child) {
			String current_path = path + std::to_string(i++) + "/";
			lhf_to_json_internal(child, obj, visited, current_path);
		}(child_refs), ...);
	}, root.get_reflist());
}

/**
 * @brief      Converts an LHF and its referenced child LHFs to JSON.
 *             The individual LHFs are identified using paths starting from the
 *             root LHF (the one supplied as a parameter). A depth-first search
 *             is performed without any traversals to already visited
 *             references. A JSON object is then created using the string paths
 *             as the keys referring to each LHF.
 *
 * @param      root  The LHF object to serialize.
 * @return     The   JSON representation.
 */
template<typename LHFT>
JSON lhf_to_json(LHFT &root) {
	HashSet<void *> visited = {};
	String path = "/";
	JSON obj = JSON::object();
	obj["lhf_version"] = LHF_VERSION_STRING;
	lhf_to_json_internal(root, obj, visited, path);

	return obj;
}

template<typename LHFT>
void lhf_from_json_internal(
	LHFT &root, const JSON &obj,
	HashSet<void *> &visited, String &path) {

	if (visited.count(&root) > 0) {
		// LHF_DEBUG(std::cout << "Already found '" << root.name << "' <" << &root << ">\n";);
		// LHF_DEBUG(std::cout << "Path: " << path << "\n";);
		return;
	} else {
		// LHF_DEBUG(std::cout << "Visited '" << root.name << "' <" << &root << ">\n";);
		// LHF_DEBUG(std::cout << "Path: " << path << "\n";);
		visited.insert(&root);
	}

	root.load_from_json(obj[path]);

	std::apply([&](auto&... child_refs) {
		// Local counter for this node's children
		[[maybe_unused]] Size i = 0;

		// The fold expression calls this lambda for EACH child in the pack
		([&](auto& child) {
			String current_path = path + std::to_string(i++) + "/";
			lhf_from_json_internal(child, obj, visited, current_path);
		}(child_refs), ...);
	}, root.get_reflist());
}

/**
 * @brief      Loads all data from the supplied JSON object into LHF.
 *
 * @param      root  The LHF object to load data into.
 * @param[in]  obj   The object.
 */
template<typename LHFT>
void lhf_from_json(LHFT &root, const JSON &obj) {
	HashSet<void *> visited = {};
	String path = "/";
	lhf_from_json_internal(root, obj, visited, path);
}

/**
 * @brief      Returns a string representation of the supplied JSON object.
 *
 * @param[in]  obj   The object
 *
 * @return     String representation of obj.
 */
inline String json_to_string(const JSON &obj) {
	return obj.dump();
}

inline Vector<uint8_t> json_to_bson(const JSON &obj) {
	return JSON::to_bson(obj);
}

/**
 * @brief      Writes a JSON object to a file.
 *
 * @param[in]  obj        The object
 * @param[in]  file_path  The file path
 *
 * @return     true on success, false on failure.
 */
inline bool json_to_file(const JSON &obj, const String &file_path) {
	std::ofstream f(file_path);

	if (!f.is_open()) {
		return false;
	}

	f << obj;
	return true;
}

inline bool json_to_file_bson(const JSON &obj, const String &file_path) {
	std::ofstream f(file_path, std::ios::out | std::ios::binary);

	if (!f.is_open()) {
		return false;
	}

	Vector<uint8_t> data = json_to_bson(obj);
	f.write(reinterpret_cast<const char *>(data.data()), data.size());
	return true;
}

inline JSON load_json_file(const String &file_path) {
	std::ifstream f(file_path);

	if (!f.is_open()) {
		throw SerializationError("Could not open '" + file_path + "'");
	}

	return JSON::parse(f);
}

inline JSON load_bson_file(const String &file_path) {
	std::ifstream f(file_path);

	if (!f.is_open()) {
		throw SerializationError("Could not open '" + file_path + "'");
	}

	return JSON::from_bson(f);
}

template<typename LHFT>
bool save(const LHFT &lhf, const String &file_path) {
	return json_to_file(lhf_to_json(lhf), file_path);
}

template<typename LHFT>
bool save_bson(const LHFT &lhf, const String &file_path) {
	return json_to_file_bson(lhf_to_json(lhf), file_path);
}

template<typename LHFT>
void load(LHFT &lhf, const String &file_path) {
	lhf_from_json(lhf, load_json_file(file_path));
}

template<typename LHFT>
void load_bson(LHFT &lhf, const String &file_path) {
	lhf_from_json(lhf, load_bson_file(file_path));
}

}; // END namespace slz

}; // END namespace lhf

#endif