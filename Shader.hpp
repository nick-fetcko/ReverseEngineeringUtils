#pragma once

#include <filesystem>
#include <stdint.h>
#include <windef.h>
#include <string>

#include "Utils/Utils.hpp"

#include "third_party/DXBCChecksum/DXBCChecksum.h"

namespace Fetcko {
class Shader : public LoggableClass {
public:
	static const std::set<Shader *> &GetTargetShaders() { return TargetShaders; }

	explicit Shader(const std::string &name, std::vector<DWORD> targetHash = { 0, 0, 0, 0 }) : name(name) {
		memcpy(this->targetHash, targetHash.data(), sizeof(DWORD) * 4);
		TargetShaders.emplace(this);
	}

	virtual ~Shader() {
		delete[] bytecode;

		TargetShaders.erase(this);
	}

	const std::string &GetName() const {
		return name;
	}

	void SetBytecode(const uint8_t *bytecode, std::size_t size) {
		if (this->bytecode)
			delete[] this->bytecode;

		this->bytecode = new uint8_t[size];
		memcpy(this->bytecode, bytecode, size);

		bytecodeSize = size;
	}

	void SetBytecode(const std::filesystem::path &path) {
		const auto string = Utils::GetStringFromFile(path);

		LogDebug("bytecode string = ", string);

		if (bytecode)
			delete[] bytecode;

		bytecodeSize = string.size();

		bytecode = new uint8_t[bytecodeSize];
		memcpy(bytecode, string.data(), bytecodeSize);
	}

	const uint8_t *GetBytecode() const { return bytecode; }
	std::size_t GetBytecodeSize() const { return bytecodeSize; }

	bool IsMatch(const void *pShaderBytecode, SIZE_T BytecodeLength) const {
		DWORD hash[4];
		memcpy(hash, reinterpret_cast<const void *>(&(reinterpret_cast<const uint8_t *>(pShaderBytecode)[4])), sizeof(hash));

		return hash[0] == targetHash[0] && hash[1] == targetHash[1] && hash[2] == targetHash[2] && hash[3] == targetHash[3];
	}

	template<typename T>
	void PatchIndices(std::vector<std::size_t> indices, T oldValue, T newValue) {
		if (!bytecode) return;

		std::size_t patched = 0;
		for (const auto &index : indices) {
			const auto ptr = reinterpret_cast<float *>(&(reinterpret_cast<uint8_t *>(bytecode)[index]));

			if (*ptr == oldValue) {
				*ptr = newValue;
				++patched;
			}
		}

		if (patched) {
			LogDebug("Patched ", name, " shader with value ", newValue, " ", patched, " times");
			UpdateHash();
		}
	}

	template<typename T>
	void PatchLiteral(T oldValue, T newValue) {
		if (!bytecode) return;

		bool patched = false;
		for (auto byte = bytecode; byte != bytecode + bytecodeSize; ++byte) {
			if (*reinterpret_cast<T *>(byte) == oldValue) {
				*reinterpret_cast<T *>(byte) = newValue;

				patched = true;
			}
		}

		if (patched) {
			LogDebug("Patched ", name, " shader with value ", newValue);
			UpdateHash();
		}
	}

	void ClearBytecode() {
		delete[] bytecode;
		bytecode = nullptr;
		bytecodeSize = 0;

		TargetShaders.erase(this);
	}

private:
	inline static std::set<Shader *> TargetShaders;

	void UpdateHash() {
		DWORD hash[4];

		CalculateDXBCChecksum(reinterpret_cast<BYTE *>(bytecode), bytecodeSize, hash);

		memcpy(reinterpret_cast<void *>(&(reinterpret_cast<uint8_t *>(bytecode)[4])), hash, sizeof(hash));
	}

	uint8_t *bytecode = nullptr;
	std::size_t bytecodeSize = 0;

	std::string name;
	DWORD targetHash[4];
};
}