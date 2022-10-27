#include "CommonTypes.h"
#include "Core/MIPSLogger.h"
#include <vector>
// #include <map>

MIPSLoggerSettings::MIPSLoggerSettings(int max_count_) : max_count(max_count_), forbidden_ranges() {}

u32 MIPSLoggerSettings::getMaxCount() {
	return max_count;
}

bool MIPSLoggerSettings::log_address(u32 address) {
	auto lower = forbidden_ranges.lower_bound(address);
	return lower == forbidden_ranges.end() || address >= lower->first + lower->second;
}

bool MIPSLoggerSettings::forbide_range(u32 start, u32 size) {
	if (!forbidden_ranges.size()) {
		forbidden_ranges.insert({ start, size });
		return true;
	}

	auto lower = forbidden_ranges.lower_bound(start);
	if (lower == forbidden_ranges.end()) {
		// does the given range intersect the first one?
		if (forbidden_ranges.lower_bound(start + size - 1) != forbidden_ranges.end()) {
			return false;
		}
		forbidden_ranges.insert({ start, size });
		return true;
	}
	// let's check start and end
	if (start < lower->first + lower->second) {
		return false;
	}
	if (lower != forbidden_ranges.lower_bound(start + size - 1)) {
		return false;
	}
	forbidden_ranges.insert({ start, size });
	return true;
}

bool MIPSLoggerSettings::allow_range(u32 start, u32 size) {
	auto iter = forbidden_ranges.find(start);
	if (iter == forbidden_ranges.end() || iter->second != size) {
		return false;
	}
	forbidden_ranges.erase(iter);
	return true;
}

MIPSLogger::MIPSLogger() {
}

MIPSLogger::~MIPSLogger() {
}
