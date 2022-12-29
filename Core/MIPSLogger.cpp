#include "CommonTypes.h"
#include "Core/MIPSLogger.h"
#include "Core/Core.h"
#include "Core/Mips/MIPSDebugInterface.h"
#include <vector>

// #include <map>

MIPSLoggerSettings::MIPSLoggerSettings(int max_count_) :
	max_count(max_count_),
	forbidden_ranges(),
	additional_info(),
	flush_when_full(false)
{}

MIPSLoggerSettings::MIPSLoggerSettings() :
	max_count(100000),
	forbidden_ranges(),
	additional_info(),
	flush_when_full(false)
{} // random number

u32 MIPSLoggerSettings::getMaxCount() const {
	return max_count;
}

bool MIPSLoggerSettings::getFlushWhenFull() const {
	return flush_when_full;
}

bool MIPSLoggerSettings::log_address(u32 address) const {
	auto lower = forbidden_ranges.lower_bound(address);
	return lower == forbidden_ranges.end() || address >= lower->first + lower->second;
}

bool MIPSLoggerSettings::forbide_range(u32 start, u32 size) {
	if (!forbidden_ranges.size()) {
		forbidden_ranges.insert({ start, size });
		// forbidden_ranges.emplace(start, size);
		return true;
	}

	auto lower = forbidden_ranges.lower_bound(start);
	if (lower == forbidden_ranges.end()) {
		// does the given range intersect the first one?
		if (forbidden_ranges.lower_bound(start + size - 1) != forbidden_ranges.end()) {
			return false;
		}
		forbidden_ranges.insert({ start, size });
		// forbidden_ranges.emplace(start, size);
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
	// forbidden_ranges.emplace(start, size);
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

void MIPSLoggerSettings::update_additional_log(u32 address, std::string log_info) {
	additional_info[address] = log_info;
}

bool MIPSLoggerSettings::remove_additional_log(u32 address) {
	auto iter = additional_info.find(address);
	if (iter == additional_info.end()) {
		return false;
	}
	additional_info.erase(iter);
	return true;
}

bool MIPSLoggerSettings::get_additional_log(u32 address, std::string& log_info) const {
	auto iter = additional_info.find(address);
	if (iter == additional_info.end()) {
		return false;
	}
	log_info = iter->second;
	return true;
}

bool MIPSLoggerSettings::instance_made = false;
std::shared_ptr<MIPSLoggerSettings> MIPSLoggerSettings::_only_instance;

std::shared_ptr<MIPSLoggerSettings> MIPSLoggerSettings::getInstance() {
	if (!instance_made) {
		instance_made = true;
		_only_instance = std::make_shared<MIPSLoggerSettings>();
	}
	return _only_instance;
}

MIPSLogger::MIPSLogger() {
	disasm.setCpu(currentDebugMIPS);
}

MIPSLogger::~MIPSLogger() {
	disasm.clear();
}

bool MIPSLogger::isLogging() {
	return logging_on;
}

bool MIPSLogger::Log(u32 pc) {
	if (!logging_on || !cur_settings || !cur_settings->log_address(pc)) return false;

	disasm.getLine(pc, false, disasm_line, currentDebugMIPS);
	disasm_buffer << "PC = " << std::hex << pc << std::dec << " ";
	disasm_buffer << disasm_line.name << " " << disasm_line.params;
	std::string additional;
	if (cur_settings->get_additional_log(pc, additional)) {
		disasm_buffer << " // " << additional;
	}
	logs_storage.push_back(disasm_buffer.str());
	disasm_buffer.str(std::string());

	if (logs_storage.size() == cur_settings->getMaxCount()) {
		// we are done, let's start stepping
		Core_EnableStepping(true, "mipslogger.overflow");
		if (cur_settings->getFlushWhenFull()) {
			flush_to_file();
		}
	}
	return true;
}

//bool MIPSLogger::selectLogStream(std::ofstream& output_stream) {
//	if (!output_stream) return false;
//	//output = output_stream;
//	output = std::make_shared<std::ofstream>(output_stream);
//	return true;
//}
//
//bool MIPSLogger::selectLogStream(std::shared_ptr<std::ofstream> output_stream) {
//	if (!*output_stream) return false;
//	output = output_stream;
//	return true;
//}

bool MIPSLogger::selectLogPath(const std::string& output_path) {
	// let's open the file
	if (output) {
		output.close();
	}
	output.open(output_path, std::ios_base::app);
	if (!output) return false;
	return true;
}

void MIPSLogger::stopLogger() {
	logging_on = false;
}

bool MIPSLogger::flush_to_file() {
	if (logging_on || !output) {
		return false;
	}
	for (const auto& log : logs_storage) {
		output << log << "\n";
	}
	// Not catching exceptions here.

	//*output << disasm_buffer.rdbuf();
	//disasm_buffer.clear();
	output.flush();
	return true;
}

bool MIPSLogger::startLogger() {
	if (!output || !cur_settings) return false;
	logging_on = true;
	return true;
}

MIPSLogger mipsLogger;

// std::shared_ptr<MIPSLoggerSettings> default_MIPSLogger_settings;
