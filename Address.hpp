#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>

#include <Windows.h>

#include "Utils/Logger.hpp"

namespace Fetcko {
class Address : public LoggableClass {
public:
	explicit Address(UINT_PTR baseAddress, std::size_t offset) :
		baseAddress(baseAddress), offset(offset) {

	}

	const std::size_t GetAddress() const { return baseAddress + offset; }

	void JumpToOffsetAtByte(const std::size_t offset) {
		uint32_t jump = *reinterpret_cast<uint32_t *>(GetAddress() + offset);
		LogDebug("jump = ", std::hex, jump);
		this->offset += jump + 8;
	}

	float GetFloat() const {
		return *reinterpret_cast<float *>(GetAddress());
	}

	template<typename T>
	void Set(T value) {
		DWORD oldProtect;
		VirtualProtect(reinterpret_cast<LPVOID>(GetAddress()), sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect);

		memcpy(reinterpret_cast<void *>(GetAddress()), &value, sizeof(T));

		DWORD tempProtect;
		VirtualProtect(reinterpret_cast<LPVOID>(GetAddress()), sizeof(T), oldProtect, &tempProtect);
	}

private:
	UINT_PTR baseAddress;
	std::size_t offset;
};
}