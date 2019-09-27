#include "FileHelper.h"
#include <stdio.h>

bool IBLLib::readFile(const char* _path, std::vector<char>& _outBuffer)
{
	FILE* file = fopen(_path, "rb");

	if (file == nullptr)
	{
		printf("Failed to open file %s\n", _path);
		return false;
	}

	// obtain file size:
	fseek(file, 0, SEEK_END);
	auto size = ftell(file);
	rewind(file);

	_outBuffer.resize(size);

	// read the file
	auto sizeRead = fread(_outBuffer.data(), sizeof(char), size, file);
	fclose(file);

	return sizeRead > 0u;
}

bool IBLLib::writeFile(const char* _path, const char* _data, size_t _bytes)
{
	FILE* file = fopen(_path, "wb");

	if (file == nullptr)
	{
		printf("Failed to open file %s\n", _path);
		return false;
	}

	// write the file
	auto sizeWritten = fwrite(_data, sizeof(char), _bytes, file);
	fclose(file);

	return sizeWritten > 0u;
}
