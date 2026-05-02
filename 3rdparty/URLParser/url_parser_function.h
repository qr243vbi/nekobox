#ifdef _WIN32
#include <winsock2.h>
#endif
#pragma once

#include <string>

class URLParserFunction
{
public:
	static bool FindKeyword(const std::string& input_url, size_t& st, size_t& before, const std::string& delim, std::string& result)
	{
		size_t temp_st = st;

		st = input_url.find(delim, before);
		if (st == std::string::npos)
		{
			st = temp_st;
			return false;
		}

		result = input_url.substr(before, st - before);
		before = st + delim.length();

		if (result.empty())
			return false;

		return true;
	};

	static bool SplitQueryString(const std::string& str, const std::string& delim, std::string& key, std::string& value)
	{
		size_t st = str.find(delim);

		if (st == std::string::npos)
		{
			key = str;
			value = "";
			return false;
		}

		key = str.substr(0, st);
		value = str.substr(st + delim.length());

		return true;
	};
};
