#pragma once

#include <iostream>

#include <Windows.h>

#include "Utils/Logger.hpp"

namespace Fetcko {
class BaseAddress : public LoggableClass {
public:
	UINT_PTR address = 0;
	DWORD baseOfCode = 0;
	DWORD sizeOfCode = 0;

	static BaseAddress Get() {
		BaseAddress ret;

		ret.address = reinterpret_cast<UINT_PTR>(GetModuleHandle(nullptr));

		PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(ret.address);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
			ret.LogError("Could not find valid DOS signature!");
			return ret;
		}

		PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(ret.address + dosHeader->e_lfanew);

		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
			ret.LogError("Could not find valid PE signature!");
			return ret;
		}

		ret.address += ntHeaders->OptionalHeader.BaseOfCode;

		ret.baseOfCode = ntHeaders->OptionalHeader.BaseOfCode;
		ret.sizeOfCode = ntHeaders->OptionalHeader.SizeOfCode;

		return ret;
	}
};
}