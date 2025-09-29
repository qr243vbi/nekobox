#pragma once

#include <string>
#include <cstring>

class URLParserFunction
{
public:
	static bool FindKeyword(const std::string& input_url, size_t& st, size_t& before, const std::string& delim, std::string& result)
	{
		char temp[1024] = { 0, };
		size_t temp_st = st;
		memcpy(&temp_st, &st, sizeof(temp_st));

		st = input_url.find(delim, before);
		if (st == std::string::npos)
		{
			st = temp_st;
			return false;
		}

		memcpy(&temp[0], &input_url[before], st - before);
		before = st + delim.length();

		result = std::string(temp);
		if (result.empty())
			return false;

		return true;
	};

	static bool SplitQueryString(const std::string& str, const std::string& delim, std::string& key, std::string& value)
	{
		char first[1024] = { 0, };
		char second[1024] = { 0, };

		size_t st = str.find(delim, 0);

		memcpy(first, &str[0], st);
		memcpy(second, &str[st + 1], str.length() - st);

		key = std::string(first);
		value = std::string(second);

		return true;
	};
};
