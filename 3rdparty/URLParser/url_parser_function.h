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

		int stbef = (int) st;
		stbef -= (int)before;

		if (stbef > -1 && stbef <= 1024){
			memcpy(&temp[0], &input_url[before], stbef);
			before = st + delim.length();
		}

		result = std::string(temp);
		if (result.empty())
			return false;

		return true;
	};

	static bool SplitQueryString(const std::string& str, const std::string& delim, std::string& key, std::string& value)
	{
		char first[1024] = { 0, };
		char second[1024] = { 0, };

		int st = str.find(delim, 0);
		if (st > 1024) {
			key = str;
			value = "";
			return true;
		} else {
			memcpy(first, &str[0], st);
			int st2 =  str.length() - st;
			if (st2 > 1024) st2 = 1024;
			if (st2 < 0) st2 = 0;

			memcpy(second, &str[st + 1], st2);

			key = std::string(first);
			value = std::string(second);
		}

		return true;
	};
};
