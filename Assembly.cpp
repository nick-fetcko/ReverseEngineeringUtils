#include "Assembly.hpp"

#include <thread>
#include <chrono>
#include <charconv>

#include "Utils/Utils.hpp"

#define BEA_ENGINE_STATIC
#include "beaengine/BeaEngine.h"

using namespace std::chrono_literals;

namespace Fetcko {
Assembly::fasm_Assemble_t Assembly::Init() {
	DLL = LoadLibraryA("FASMX64.dll");

	if (DLL) {
		if (auto ret = reinterpret_cast<fasm_Assemble_t>(GetProcAddress(DLL, "fasm_Assemble")))
			return ret;
	}

	return nullptr;
}

HMODULE Assembly::DLL = nullptr;
Assembly::fasm_Assemble_t Assembly::_Assemble = Init();

Assembly::Assembly(const BaseAddress &baseAddress, const Address &address) : baseAddress(baseAddress), address(address) {
	if (!DLL)
		LogError("Could not find FASMX64.dll!");
	else if (!_Assemble)
		LogError("Could not find fasm_Assemble in FASMX64.dll!");
}

std::string Assembly::GetOperand(const DISASM *disasm, const std::string &token) {
	uint8_t opIndex = 0;

	if (auto result = std::from_chars(token.data() + 2, token.data() + token.size(), opIndex); result.ec == std::errc()) {
		const auto *op = &(disasm->Operand1) + (opIndex - 1);

		return op->OpMnemonic;
	}

	return "";
}

bool Assembly::Assemble(const std::string &assembly) {
	// TODO: Allow for dynamic selection of temp register
	std::string finalAssembly = "use64\npush rcx\n";

	DISASM disasm = { 0 };
	disasm.EIP = address.GetAddress();

	const auto originalCodeLength = Disasm(&disasm);

	if (originalCodeLength < 0) {
		LogError("Could not disassemble! Error code ", originalCodeLength);
		return false;
	}

	bool inToken = false;
	bool inOperand = false;

	std::string token;
	for (const auto &line : Utils::Split(assembly, '\n')) {
		auto trimmedLine = line;
		Utils::ltrim(trimmedLine);

		std::string finalLine;
		for (const auto &c : trimmedLine) {
			if (c == '[') {
				inToken = true;
				token.clear();
			} else if (c == ']') {
				inToken = false;

				if (auto iter = objects.find(token); iter != objects.end()) {
					std::stringstream stream;

					stream << "mov rcx, 0x";
					Encode64Bit(stream, iter->second);
					finalAssembly += stream.str() + "\n";

					finalLine += "[rcx]";
				} else finalLine += c;
			} else if (c == '$' && !inOperand) {
				inOperand = true;
				token.clear();
				token += c;
			} else if (!inToken && !inOperand) {
				finalLine += c;
			} else {
				token += c;

				// Is token an operand?
				if (token.size() == 3 && token[0] == '$' && token[1] == '$') {
					if (inToken) finalLine += '[';
					inOperand = false;
					if (const auto op = GetOperand(&disasm, token); !op.empty()) {
						finalLine += op;
					}
				}
			}
		}

		finalAssembly += finalLine + "\n";
	}

	finalAssembly += "pop rcx";

	// Remove any trailing whitespace
	Utils::rtrim(finalAssembly);

	// FASM's data pointer is only 32-bit, so the target buffer must be in 32-bit address space
	unsigned char *localBuf = reinterpret_cast<unsigned char *>(AllocateNearAddress(0, 4096, 0x7FFFFFFF));

	if (!localBuf) {
		LogError("Could not allocate any memory in the 32-bit address space for assembly!");
		return false;
	}

	LogDebug("Final assembly: ", finalAssembly);

	if (auto ret = _Assemble(finalAssembly.data(), localBuf, 4096, 100); ret != 0) {
		LogError("Could not compile assembly! ret = ", ret, " error code = ", reinterpret_cast<int32_t *>(localBuf)[1]);

		VirtualFree(localBuf, 0, MEM_RELEASE);

		return false;
	} else {
		const int32_t length = reinterpret_cast<int32_t *>(localBuf)[1];

		Allocate(length + originalCodeLength + 5 /* size of return jmp */);

		relativeAddress = static_cast<int>(
			reinterpret_cast<uintptr_t>(buf) - (address.GetAddress() + 5)
			);

		if (buf) {
			LogDebug("Assembly compiled! Output length = ", length, " output data = ", reinterpret_cast<int32_t *>(localBuf)[2]);

			// We've already ensured this is in the 32-bit address space,
			// so no need to warn us about void* being larger than int32_t
#pragma warning(push)
#pragma warning(disable: 4312)
			memcpy(buf, reinterpret_cast<void *>(reinterpret_cast<int32_t *>(localBuf)[2]), length);
#pragma warning(pop)
			memcpy(&buf[length], reinterpret_cast<void *>(address.GetAddress()), originalCodeLength);

			// Return jmp
			const auto returnJmp = static_cast<int>(relativeAddress) * -1 - (length + originalCodeLength);

			*reinterpret_cast<uint8_t *>(&buf[length + originalCodeLength]) = 0xE9;
			*reinterpret_cast<int *>(&buf[length + originalCodeLength + 1]) = returnJmp;

		} else LogError("Could not allocate memory for code cave!");

		VirtualFree(localBuf, 0, MEM_RELEASE);

		return true;
	}
}

void Assembly::EnableCodeCave(bool enabled) {
	DWORD oldProtect;
	DWORD tempProtect;

	if (enabled) {
		VirtualProtect(reinterpret_cast<LPVOID>(address.GetAddress()), 5, PAGE_EXECUTE_READWRITE, &oldProtect);

		memcpy(oldBytes, reinterpret_cast<void *>(address.GetAddress()), 5);

		*reinterpret_cast<uint8_t *>(address.GetAddress()) = 0xE9;
		*reinterpret_cast<int *>(address.GetAddress() + 1) = relativeAddress;

		VirtualProtect(reinterpret_cast<LPVOID>(address.GetAddress()), 5, oldProtect, &tempProtect);

		LogDebug("Code cave enabled!");
	} else if (!enabled && oldBytes[0]) {
		VirtualProtect(reinterpret_cast<LPVOID>(address.GetAddress()), 5, PAGE_EXECUTE_READWRITE, &oldProtect);

		memcpy(reinterpret_cast<void *>(address.GetAddress()), oldBytes, 5);

		VirtualProtect(reinterpret_cast<LPVOID>(address.GetAddress()), 5, oldProtect, &tempProtect);

		LogDebug("Code cave disabled!");
	}
}

void Assembly::Allocate(std::size_t size) {
	buf = reinterpret_cast<unsigned char *>(
		AllocateNearAddress(
			reinterpret_cast<LPVOID>(baseAddress.address + baseAddress.baseOfCode + baseAddress.sizeOfCode), 4096, 0x7FFFFFFF
		)
	);
}

void Assembly::Deallocate() {
	if (buf) {
		VirtualFree(buf, 0, MEM_RELEASE);
		buf = nullptr;
	}

	for (auto &[name, ptr] : objects)
		VirtualFree(ptr, 0, MEM_RELEASE);

	objects.clear();

	Monitor(false);
}

void *Assembly::AllocateNearAddress(void *targetAddress, std::size_t size, std::size_t maxDistance) {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	const std::size_t granularity = sysInfo.dwAllocationGranularity;

	const auto target = reinterpret_cast<uintptr_t>(targetAddress);

	// Align to granularity
	const auto startAttempt = target & ~(granularity - 1);

	for (std::size_t offset = 0; offset < maxDistance; offset += granularity) {
		// Below target
		if (target > offset) {
			auto attemptLoc = startAttempt - offset;
			if (void *result = VirtualAlloc(reinterpret_cast<void *>(attemptLoc), size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
				return result;
		}

		// Above target
		auto attemptLoc = startAttempt + offset;
		if (void *result = VirtualAlloc(reinterpret_cast<void *>(attemptLoc), size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
			return result;
	}

	return nullptr;
}

void Assembly::Monitor(bool enabled) {
	monitoring = false;

	if (monitorThread.joinable())
		monitorThread.join();

	if (enabled) {
		monitoring = true;

		monitorThread = std::thread([this] {
			while (monitoring) {
				std::this_thread::sleep_for(1s);
				LogDebug("Allocated objects: ");
				for (const auto &object : objects) {
					LogDebug("\t", object.first, ": ", *reinterpret_cast<float *>(object.second));
				}
			}
		});
	}
}
}
