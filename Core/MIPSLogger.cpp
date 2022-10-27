#include "CommonTypes.h"
#include "Core/MIPSLogger.h"
#include "Core/Core.h"
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

bool MIPSLoggerSettings::get_additional_log(u32 address, std::string & log_info) {
	auto iter = additional_info.find(address);
	if (iter == additional_info.end()) {
		return false;
	}
	log_info = iter->second;
	return true;
}

MIPSLogger::MIPSLogger() {
	// disasm.setCpu(currentDebugMIPS);
}

MIPSLogger::~MIPSLogger() {
	disasm.clear();
}

bool MIPSLogger::isLogging() {
	return logging_on;
}

bool MIPSLogger::Log(u32 pc) {
	if (!logging_on || !cur_settings->log_address(pc)) return false;

	// disasm.getLine(pc, false, disasm_line, currentDebugMIPS);
	disasm_buffer << "PC = " << std::hex << std::to_string(pc) << " ";
	std::string additional;
	if (cur_settings->get_additional_log(pc, additional)) {
		disasm_buffer << " // " << additional;
	}
	logs_storage.push_back(disasm_buffer.str());
	disasm_buffer.clear();
	if (logs_storage.size() == cur_settings->getMaxCount()) {
		// we are done, let's start stepping
		Core_EnableStepping(true, "mipslogger.overflow");
	}
	return false;
}

bool MIPSLogger::selectLogStream(std::ofstream& output_stream) {
	if (!output_stream) return false;
	//output = output_stream;
	output = std::make_shared<std::ofstream>(output_stream);
	return true;
}

bool MIPSLogger::selectLogStream(std::shared_ptr<std::ofstream> output_stream) {
	if (!*output_stream) return false;
	output = output_stream;
	return true;
}

void MIPSLogger::stopLogger() {
	logging_on = false;
}

bool MIPSLogger::flush_to_file() {
	if (logging_on || !output || !output->is_open()) {
		return false;
	}
	for (const auto& log : logs_storage) {
		*output << log << "\n";
	}
	// Not catching exceptions here.

	//*output << disasm_buffer.rdbuf();
	//disasm_buffer.clear();
}

bool MIPSLogger::startLogger() {
	if (!output || !*output) return false;
	logging_on = true;
	return true;
}
