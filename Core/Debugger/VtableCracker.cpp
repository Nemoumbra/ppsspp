// Copyright (c) 2024- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "Core/Debugger/VtableCracker.h"
#include "Core/MIPS/MIPSHooks.h"
// #include "Common/File/FileUtil.h"

#include "Common/Log.h"

#include <fstream>
#include <sstream>

#include "Core/MemMap.h"
#include "Core/Core.h"

#define _RS   ((op>>21) & 0x1F)
#define _RD   ((op>>11) & 0x1F)
#define PC (currentMIPS->pc)
#define R(i) (currentMIPS->r[i])

static inline void DelayBranchTo(u32 where)
{
	if (!Memory::IsValidAddress(where) || (where & 3) != 0) {
		Core_ExecException(where, PC, ExecExceptionType::JUMP);
	}
	PC += 4;
	mipsr4k.nextPC = where;
	mipsr4k.inDelaySlot = true;
}


// Our custom handlers

void VtableCracker::HandleJr(MIPSOpcode op) {
	if (mipsr4k.inDelaySlot) {
		// There's one of these in Star Soldier at 0881808c, which seems benign.
		ERROR_LOG(CPU, "Jump in delay slot :(");
	}
	int rs = _RS;
	u32 addr = R(rs);

	if (rs == MIPSGPReg::MIPS_REG_T9) {
		jr_mapping[PC].emplace(addr);
	}
	if (!mipsr4k.inDelaySlot)
		DelayBranchTo(addr);
}

void VtableCracker::HandleJalr(MIPSOpcode op) {
	if (mipsr4k.inDelaySlot) {
		// There's one of these in Star Soldier at 0881808c, which seems benign.
		ERROR_LOG(CPU, "Jump in delay slot :(");
	}
	int rs = _RS;
	int rd = _RD;
	u32 addr = R(rs);

	if (rs == MIPSGPReg::MIPS_REG_T9) {
		jalr_mapping[PC].emplace(addr);
	}

	if (rd != 0)
		R(rd) = PC + 8;
	// Update rd, but otherwise do not take the branch if we're branching.
	if (!mipsr4k.inDelaySlot)
		DelayBranchTo(addr);
}



void VtableCracker::Enable() {
	MIPSHooks::Hook("jr", &HandleJr);
	MIPSHooks::Hook("jalr", &HandleJalr);
}

void VtableCracker::Disable() {
	MIPSHooks::Reset();
}

void VtableCracker::SetFlushPath(const std::string& path) {
	INFO_LOG(EXTENSIONS, "Vtable cracker: new flush path is '%s'", path.c_str());
	current_path = path;
}

std::string VtableCracker::GetFlushPath() const {
	return current_path;
}


void VtableCracker::flush_branches(std::ofstream& output, const BranchesMapping& mapping) {
	for (const auto[address, branches] : mapping) {
		auto it = branches.begin();
		output << address << " -> " << *(it++);
		while (it != branches.end()) {
			output << ", " << *it;
			++it;
		}
		output << ";\n";
	}
}


bool VtableCracker::Flush() {
	if (current_path.empty()) {
		WARN_LOG(EXTENSIONS, "Vtable cracker: cannot flush (no path specified)");
		return false;
	}

	
	std::ofstream output(current_path);
	if (!output) {
		WARN_LOG(EXTENSIONS, "Vtable cracker: cannot access the file '%s'", current_path.c_str());
		return false;
	}

	INFO_LOG(EXTENSIONS, "Vtable cracker: flush to '%s'", current_path.c_str());
	output << std::hex;

	// Dump jrs
	output << "jr t9:\n";
	if (!jr_mapping.empty()) {
		flush_branches(output, jr_mapping);
	}
	else {
		output << "None\n";
	}

	// Dump jalrs
	output << "jalr t9:\n";
	if (!jalr_mapping.empty()) {
		flush_branches(output, jalr_mapping);
	}
	else {
		output << "None\n";
	}

	return true;
}

void VtableCracker::Reset() {
	jr_mapping.clear();
	jalr_mapping.clear();
}


// Define the static fields
BranchesMapping VtableCracker::jr_mapping;
BranchesMapping VtableCracker::jalr_mapping;

VtableCracker vtableCracker;
