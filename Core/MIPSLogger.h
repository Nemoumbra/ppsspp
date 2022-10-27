#pragma once
#include <map>

class MIPSLoggerSettings {
public:
	int max_count;
	//std::vector <std::pair<int, int>> forbidden;

	std::map <u32, u32> forbidden_ranges; // make private?
	MIPSLoggerSettings(int max_count_);
	bool log_address(u32 address);
	bool forbide_range(u32 start, u32 size);
	bool allow_range(u32 start, u32 size);
};
