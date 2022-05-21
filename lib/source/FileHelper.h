#pragma once
#include <vector>
#include <stddef.h>

namespace IBLLib
{
	bool readFile(const char* _path, std::vector<char>& _outBuffer);

	bool writeFile(const char* _path, const char* _data, size_t _bytes);

	template <class T>
	bool writeFile(const char* _path, const std::vector<T>& _outBuffer)
	{
		return writeFile(_path, reinterpret_cast<const char*>(_outBuffer.data()), _outBuffer.size() * sizeof(T));
	}
} // !IBLLIb