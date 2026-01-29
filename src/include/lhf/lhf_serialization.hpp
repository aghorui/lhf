#ifndef LHF_SERIALIZATION_H
#define LHF_SERIALIZATION_H

#include <nlohmann/json.hpp>

namespace lhf {

using JsonObject = nlohmann::json;

template<typename T>
struct ValueSerializer {
	JsonObject save(const T &val) const;
	T load(const JsonObject &j) const;
};

template<typename T>
struct DefaultValueSerializer : ValueSerializer<T> {
	JsonObject save(const T &val) const {
		return JsonObject(val);
	}

	T load(const JsonObject &j) const {
		return j.get<T>();
	}
};

}; // END namespace

#endif