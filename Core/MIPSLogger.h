#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include "Core/MIPS/MIPS.h"
#include "Core/Debugger/DisassemblyManager.h"

enum class LoggingMode {
	Normal,
	LogLastNLines
};

constexpr const char* to_string(LoggingMode mode) {
	switch (mode) {
		case LoggingMode::Normal: {
			return "Normal";
		}
		case LoggingMode::LogLastNLines: {
			return "LogLastNLines";
		}
		default: {
			return "Not implemented!";
		}
	}
}


class MIPSLoggerSettings {
private:
	LoggingMode mode;
	std::map<u32, u32> forbidden_ranges;
	u32 max_count; // The size of the logs storage can only increase
	std::map<u32, std::string> additional_info;
	bool flush_when_full;
	bool ignore_forbidden_when_recording;

	static std::shared_ptr<MIPSLoggerSettings> _only_instance;
public:
	MIPSLoggerSettings(int max_count_);
	MIPSLoggerSettings();

	LoggingMode getLoggingMode() const;
	u32 getMaxCount() const;
	bool getFlushWhenFull() const;
	bool getIgnoreForbiddenWhenRecording() const;
	const std::map<u32, u32>& getForbiddenRanges() const;
	const std::map<u32, std::string>& getAdditionalInfo() const;


	void setLoggingMode(LoggingMode new_mode);
	void setMaxCount(u32 new_value);
	void setFlushWhenFull(bool new_value);
	void setIgnoreForbiddenWhenRecording(bool new_value);

	bool log_address(u32 address) const;
	bool forbid_range(u32 start, u32 size);
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
