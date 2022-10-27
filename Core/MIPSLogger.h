#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include "MIPS/MIPS.h"
#include "Core/Debugger/DisassemblyManager.h"


class MIPSLoggerSettings {
private:
	std::map <u32, u32> forbidden_ranges;
	u32 max_count;
	std::map<u32, std::string> additional_info;
public:
	MIPSLoggerSettings(int max_count_);
	u32 getMaxCount();
	bool log_address(u32 address);
	bool forbide_range(u32 start, u32 size);
	bool allow_range(u32 start, u32 size);
	void update_additional_log(u32 address, std::string log_info);
	bool remove_additional_log(u32 address);
	bool get_additional_log(u32 address, std::string& log_info);
};

class MIPSLogger {
	std::vector <std::string> logs_storage;
	bool logging_on;
	DisassemblyManager disasm;
	DisassemblyLineInfo disasm_line;
	std::stringstream disasm_buffer;
	
public:
	std::shared_ptr <MIPSLoggerSettings> cur_settings;
	MIPSLogger();
	~MIPSLogger();
	bool isLogging();
	bool Log(u32 pc);

	void stopLogger();
	bool flush_to_file();
};
