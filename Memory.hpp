#pragma once

#include "Address.hpp"

namespace Fetcko {
class Memory {
public:
	static Address SignatureScan(
		const std::string_view &signature, // String of hexadecimal bytes, separated by spaces
		const UINT_PTR baseAddress, // Entry point of .exe + start of .code page
		const std::size_t size, // Size of .code page
		const std::size_t pos // Position inside the found pattern to return
	);
};
}