#include "CommonTypes.h"
#include "Core/MIPSLogger.h"
#include "Core/Core.h"
#include "Core/Mips/MIPSDebugInterface.h"
#include "Core/Debugger/Breakpoints.h"
#include <vector>

// #include <map>

void MIPSLogger::LastNLines::store_line(const std::string & line, u32 lineCount) {
	
	if (lines.size() < lineCount) {
		lines.push_back(line);
		return;
	}
	// else lines.size() == N
	lines[cur_index] = line;
	++cur_index;
	if (cur_index == lineCount) {
		cur_index = 0;
	}
}

bool MIPSLogger::LastNLines::flush_to_file(const std::string & filename, u32 lineCount) {
	std::ofstream output(filename);
	if (!output) return false;
	if (lines.size() < lineCount) {
		for (const auto& line : lines) {
			output << line << "\n";
		}
		return true;
	}

	for (u32 i = cur_index; i < lineCount; ++i) {
		output << lines[i] << "\n";
	}
	for (u32 i = 0; i < cur_index; ++i) {
		output << lines[i] << "\n";
	}
	return true;
}

MIPSLoggerSettings::MIPSLoggerSettings(int max_count_) :
	mode(LoggingMode::Normal),
	max_count(max_count_),
	forbidden_ranges(),
	additional_info(),
	flush_when_full(false),
	ignore_forbidden_when_recording(false),
	lineCount(100)
{}

MIPSLoggerSettings::MIPSLoggerSettings() :
	mode(LoggingMode::Normal),
	max_count(100000),
	forbidden_ranges(),
	additional_info(),
	flush_when_full(false),
	ignore_forbidden_when_recording(false),
	lineCount(100)
{}

LoggingMode MIPSLoggerSettings::getLoggingMode() const {
	return mode;
}

u32 MIPSLoggerSettings::getMaxCount() const {
	return max_count;
}

bool MIPSLoggerSettings::getFlushWhenFull() const {
	return flush_when_full;
}

bool MIPSLoggerSettings::getIgnoreForbiddenWhenRecording() const {
	return ignore_forbidden_when_recording;
}

u32 MIPSLoggerSettings::getLineCount() const {
	return lineCount;
}

bool MIPSLoggerSettings::log_address(u32 address) const {
	auto first_gt = forbidden_ranges.upper_bound(address);
	// first greater than
	if (first_gt == forbidden_ranges.begin()) {
		return true;
	}
	--first_gt;
	return address >= first_gt->first + first_gt->second;
}

bool MIPSLoggerSettings::forbid_range(u32 start, u32 size) {
	if (!forbidden_ranges.size()) {
		forbidden_ranges.emplace(start, size);
		return true;
	}

	auto first_gt = forbidden_ranges.upper_bound(start);

	if (first_gt == forbidden_ranges.begin()) {
		if (start + size - 1 < first_gt->first) {
			forbidden_ranges.emplace(start, size);
			return true;
		}
		return false;
	}
	if (first_gt == forbidden_ranges.end()) {
		--first_gt;
		if (start > first_gt->first + first_gt->second - 1) {
			forbidden_ranges.emplace(start, size);
			return true;
		}
		return false;
	}
	--first_gt;
	if (start <= first_gt->first + first_gt->second - 1) {
		return false;
	}
	++first_gt;
	if (first_gt->first <= start + size - 1) {
		return false;
	}
	forbidden_ranges.emplace(start, size);
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

const std::map<u32, u32>& MIPSLoggerSettings::getForbiddenRanges() const {
	return forbidden_ranges;
}

const std::map<u32, std::string>& MIPSLoggerSettings::getAdditionalInfo() const {
	return additional_info;
}

void MIPSLoggerSettings::setLoggingMode(LoggingMode new_mode) {
	mode = new_mode;
}

void MIPSLoggerSettings::setMaxCount(u32 new_value) {
	max_count = new_value;
}

void MIPSLoggerSettings::setFlushWhenFull(bool new_value) {
	flush_when_full = new_value;
}

void MIPSLoggerSettings::setIgnoreForbiddenWhenRecording(bool new_value) {
	ignore_forbidden_when_recording = new_value;
}

void MIPSLoggerSettings::setLineCount(u32 new_value) {
	lineCount = new_value;
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



std::string MIPSLogger::compute_line(u32 pc) {
	disasm.getLine(pc, false, disasm_line, currentDebugMIPS);
	disasm_buffer << "PC = " << std::hex << pc << std::dec << " ";
	disasm_buffer << disasm_line.name << " " << disasm_line.params;
	std::string format;
	if (cur_settings->get_additional_log(pc, format)) {
		disasm_buffer << " // ";
		std::string additional;
		if (CBreakPoints::EvaluateLogFormat(currentDebugMIPS, format, additional)) {
			disasm_buffer << additional;
		}
		else {
			disasm_buffer << format;
		}
	}
	std::string res = disasm_buffer.str();
	disasm_buffer.str(std::string());
	return res;
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
	if (!logging_on || !cur_settings) return false;

	auto mode = cur_settings->getLoggingMode();
	if (mode == LoggingMode::Normal) {
		if (!cur_settings->log_address(pc)) return false;
		if (logs_storage.size() == cur_settings->getMaxCount()) return false;
	}
	else {
		// we can either ignore or not
		if (!cur_settings->getIgnoreForbiddenWhenRecording() && !cur_settings->log_address(pc)) return false;
	}
	
	auto line = compute_line(pc);

	if (mode == LoggingMode::LogLastNLines) {
		auto lastLinesCount = cur_settings->getLineCount();
		cyclic_buffer.store_line(line, lastLinesCount);
		return true;
	}
	if (mode == LoggingMode::Normal) {
		logs_storage.push_back(line);
		if (logs_storage.size() == cur_settings->getMaxCount()) {
			// we are done, let's start stepping
			Core_EnableStepping(true, "mipslogger.overflow");
			if (cur_settings->getFlushWhenFull()) {
				flush_to_file();
			}
		}
	}
	
	return true;
}

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
	/*if (output) {
		output.close();
	}*/
	logging_on = false;
}

bool MIPSLogger::flush_to_file(const std::string& filename) {
	// filename is only used in the LogLastNLines mode

	if (logging_on) {
		return false;
	}
	auto mode = cur_settings->getLoggingMode();
	if (mode == LoggingMode::Normal) {
		if (!output) return false;
		for (const auto& log : logs_storage) {
			output << log << "\n";
		}
		// Not catching exceptions here.
		output.flush();
		return true;
	}
	auto lastLinesCount = cur_settings->getLineCount();
	return cyclic_buffer.flush_to_file(filename, lastLinesCount);
}

bool MIPSLogger::startLogger() {
	if (!cur_settings) return false;
	if ((cur_settings->getLoggingMode() == LoggingMode::Normal) && !output) return false;
	logging_on = true;
	return true;
}

MIPSLogger mipsLogger;

// std::shared_ptr<MIPSLoggerSettings> default_MIPSLogger_settings;
