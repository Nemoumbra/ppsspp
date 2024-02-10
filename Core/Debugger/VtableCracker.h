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

#pragma once

#include "Core/MIPS/MIPSTables.h"

#include <map>
#include <unordered_set>
#include <fstream>


using BranchesMapping = std::map<uint32_t, std::unordered_set<uint32_t>>;

class VtableCracker {
private:
	static BranchesMapping jr_mapping;
	static BranchesMapping jalr_mapping;
	std::string current_path;

	void flush_branches(std::ofstream& ouput, const BranchesMapping& mapping);
public:
	static void HandleJr(MIPSOpcode op);
	static void HandleJalr(MIPSOpcode op);

	void SetFlushPath(const std::string& path);
	std::string GetFlushPath() const;
	void Enable();
	void Disable();

	bool Flush();
	void Reset();
};

extern VtableCracker vtableCracker;
