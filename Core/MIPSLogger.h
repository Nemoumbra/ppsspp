#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include "Core/MIPS/MIPS.h"
#include "Core/Debugger/DisassemblyManager.h"


class MIPSLoggerSettings {
private:
	std::map <u32, u32> forbidden_ranges;
	u32 max_count;
	std::map<u32, std::string> additional_info;
	bool flush_when_full;

	static std::shared_ptr<MIPSLoggerSettings> _only_instance;
public:
	MIPSLoggerSettings(int max_count_);
	MIPSLoggerSettings();

	u32 getMaxCount() const;
	bool getFlushWhenFull() const;
	bool log_address(u32 address) const;
	bool forbide_range(u32 start, u32 size);
	bool allow_range(u32 start, u32 size);
	void update_additional_log(u32 address, std::string log_info);
	bool remove_additional_log(u32 address);
	bool get_additional_log(u32 address, std::string& log_info) const;

	static std::shared_ptr<MIPSLoggerSettings> getInstance();
	static bool instance_made;
};

class MIPSLogger {
private:
	std::vector <std::string> logs_storage;
	bool logging_on;
	DisassemblyManager disasm;
	DisassemblyLineInfo disasm_line;
	std::stringstream disasm_buffer;
	//std::shared_ptr<std::ofstream> output;
	std::ofstream output;
	
public:
	std::shared_ptr <MIPSLoggerSettings> cur_settings;
	MIPSLogger();
	~MIPSLogger();
	bool isLogging();
	bool Log(u32 pc);

	// bool selectLogStream(std::ofstream& output_stream);
	// bool selectLogStream(std::shared_ptr<std::ofstream> output_stream);
	bool selectLogPath(const std::string& output_path);
	void stopLogger();
	bool flush_to_file();

	bool startLogger();
};


extern MIPSLogger mipsLogger;
// extern std::shared_ptr<MIPSLoggerSettings> default_MIPSLogger_settings;
