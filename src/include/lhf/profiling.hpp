/**
 * @file profiling.hpp
 * @brief Enables basic code-based profiling metrics in LHF.
 */

#ifndef LHF_PROFILING_H
#define LHF_PROFILING_H

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace lhf {

/**
 * @brief      Utility class for enabling code-based profiling.
 */
struct PerformanceStatistics {
	using Count = uint64_t;
	using String = std::string;
	using TimePoint = std::chrono::steady_clock::time_point;
	template <typename K, typename V> using Map = std::map<K, V>;

	struct Duration {
		bool started = false;
		TimePoint t1;
		TimePoint t2;
		long double duration = 0;

		long double get_curr_duration_ms() {
			return (std::chrono::duration_cast<std::chrono::microseconds>(t2.time_since_epoch())
						.count() -
					std::chrono::duration_cast<std::chrono::microseconds>(t1.time_since_epoch())
						.count()) /
				   1000.0;
		}

		long double get_cumul_duration_ms() { return duration; }
	};

	Map<String, Count> counters;
	Map<String, Duration> timers;

	// Timer Functions

	Duration &get_timer(const String &s) {
		if (timers.count(s) == 0) {
			timers[s] = Duration{};
		}

		return timers[s];
	}

	void timer_start(const String &s) {
		auto &d = get_timer(s);
		assert(!d.started && "timer already started");
		d.started = true;
		d.t1 = std::chrono::steady_clock::now();
	}

	void timer_end(const String &s) {
		auto &d = get_timer(s);
		assert(d.started && "timer already stopped");
		d.started = false;
		d.t2 = std::chrono::steady_clock::now();
		d.duration += d.get_curr_duration_ms();
	}

	// Counter Functions

	Count &get_counter(const String &s) {
		if (counters.count(s) == 0) {
			counters[s] = 0;
		}

		return counters[s];
	}

	void inc_counter(const String &s) { get_counter(s)++; }

	// dump

	String dump() const {
		using namespace std;
		stringstream s;

		if (counters.size() < 1 && timers.size() < 1) {
			s << endl << "Profiler: No statistics generated" << endl;
			return s.str();
		}

		s << endl << "Profiler Statistics:" << endl;
		for (auto k : counters) {
			s << "    "
				 << "'" << k.first << "'"
				 << ": " << k.second << endl;
		}
		for (auto k : timers) {
			s << "    "
				 << "'" << k.first << "'"
				 << ": " << k.second.get_cumul_duration_ms() << " ms" << endl;
		}

		return s.str();
	}
};

/**
 * @brief      The object used to enable the duration capturing mechanism.
 */
struct __CalcTime {
	const std::string key;
	PerformanceStatistics &stat;

	__CalcTime(PerformanceStatistics &stat, const std::string key)
		: key(key), stat(stat) { stat.timer_start(key); }

	~__CalcTime() { stat.timer_end(key); }
};

/**
 * @def        __lhf_calc_time(__stat, __key)
 * @brief      Creates a timer object to capture duration when going out of
 *             scope.
 *
 * @param      __stat  PerformanceStatistics object
 * @param      __key   Identifier for this duration
 */

/**
 * @def        __lhf_calc_functime(__stat)
 * @brief      Creates a timer object to capture duration of the current
 *             function when going out of scope.
 *
 * @param      __stat  PerformanceStatistics object
 */

#ifdef LHF_ENABLE_PERFORMANCE_METRICS
#define __lhf_calc_time(__stat, __key) \
	auto __LHF_TIMER_OBJECT__ = __CalcTime((__stat), (__key))
#define __lhf_calc_functime(__stat) \
	auto __LHF_TIMER_OBJECT__ = __CalcTime((__stat), __func__)
#else
#define __lhf_calc_time(__stat, __key)
#define __lhf_calc_functime(__stat)
#endif

}

#endif