#pragma once
#include <map>
#include <memory>

class MIPSLoggerSettings {
private:
	std::map <u32, u32> forbidden_ranges;
	u32 max_count;
public:
	MIPSLoggerSettings(int max_count_);
	u32 getMaxCount();
	bool log_address(u32 address);
	bool forbide_range(u32 start, u32 size);
	bool allow_range(u32 start, u32 size);
};

class MIPSLogger {
public:
	std::shared_ptr <MIPSLoggerSettings> cur_settings;
	MIPSLogger();
	~MIPSLogger();
};
