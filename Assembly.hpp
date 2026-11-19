#pragma once

#include <Windows.h>

#include "Utils/Logger.hpp"

#include "Address.hpp"
#include "BaseAddress.hpp"

typedef struct _Disasm DISASM;

namespace Fetcko {
class Assembly : public LoggableClass {
private:
	typedef unsigned int (*fasm_Assemble_t)(const char *szSource, BYTE *lpMemory, DWORD nSize, int nPassesLimit);

public:
	Assembly(const BaseAddress &baseAddress, const Address &address);

	bool Assemble(const std::string &assembly);

	void EnableCodeCave(bool enabled);

	void Allocate(std::size_t size);
	void Deallocate();

	template<typename T>
	void Allocate(const std::string &name) {
		auto bytes = AllocateNearAddress(
			reinterpret_cast<LPVOID>(baseAddress.address + baseAddress.baseOfCode + baseAddress.sizeOfCode),
			sizeof(T),
			0x7FFFFFFF
		);

		if (bytes)
			objects.emplace(std::make_pair(name, bytes));
		else
			LogError("Could not allocate variable \"", name, "\" on allocation stack!");
	}

	template<typename T>
	void Allocate(const std::string &name, T value) {
		Allocate<T>(name);

		*reinterpret_cast<T *>(objects[name]) = value;

		LogDebug("Setting value \"", name, "\" to ", *reinterpret_cast<T *>(objects[name]));
	}

	template<typename T>
	void Update(const std::string &name, T value) {
		if (auto iter = objects.find(name); iter != objects.end()) {
			*reinterpret_cast<T *>(iter->second) = value;
		}
	}

	void Monitor(bool enabled);

	explicit operator bool() const {
		return DLL && _Assemble;
	}

private:
	static fasm_Assemble_t Init();

	static HMODULE DLL;
	static fasm_Assemble_t _Assemble;

	void *AllocateNearAddress(void *targetAddress, std::size_t size, std::size_t maxDistance);

	template <typename T>
	void Encode64Bit(std::stringstream &stream, T value) {
		stream << std::hex << std::setw(16) << std::setfill('0') << value;
	}

	std::string GetOperand(const DISASM *disasm, const std::string &token);

	const BaseAddress &baseAddress;
	const Address &address;

	int relativeAddress = 0;

	uint8_t *buf = nullptr;

	std::map<std::string, void *> objects;
	std::map<std::string, int> objectPlaceholders;

	uint8_t oldBytes[5] = { 0 };

	std::thread monitorThread;

	// https://www.youtube.com/watch?v=kbNdx0yqbZE
	std::atomic<bool> monitoring = false;
};
}