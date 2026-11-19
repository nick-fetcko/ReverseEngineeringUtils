#include "Memory.hpp"

#include <charconv>
#include <sstream>

#include "Utils/Utils.hpp"

namespace Fetcko {
Address Memory::SignatureScan(
	const std::string_view &signature, // String of hexadecimal bytes, separated by spaces
	const UINT_PTR baseAddress, // Entry point of .exe + start of .code page
	const std::size_t size, // Size of .code page
	const std::size_t pos // Position inside the found pattern to return
) {
	LoggableClass loggableClass("Memory::SignatureScan");

	const auto split = Utils::Split(signature, ' ');

	std::size_t offset = 0;
	uint8_t byte = 0;

	while (true) {
		bool found = true;
		for (const auto &[i, token] : Utils::Enumerate(split)) {
			// ANY invalid argument (including "??", obviously) is treated as a wildcard
			if (std::from_chars(token.c_str(), token.c_str() + token.size(), byte, 16).ec != std::errc::invalid_argument) {
				if (byte != *reinterpret_cast<const uint8_t *const>(baseAddress + offset + i)) {
					found = false;
					break;
				}
			}
		}

		if (found) {
			std::stringstream stream;
			stream << "Found pattern at offset " << offset << ": ";
			for (std::size_t i = 0; i < split.size() + 4; ++i) {
				stream
					<< std::hex
					<< std::setw(2)
					<< std::setfill('0')
					<< static_cast<int>(*reinterpret_cast<const uint8_t *const>(baseAddress + offset + i))
					<< " ";
			}
			loggableClass.LogDebug(stream.str());

			return Address(baseAddress, offset + pos);
		}

		if (++offset + split.size() >= size)
			break;
	}

	return Address(baseAddress, 0);
}
}
