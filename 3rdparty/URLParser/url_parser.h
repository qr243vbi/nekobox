//
// URL Parser for C++
// Created Oct 22, 2017.
// Website : https://github.com/dongbum/URLParser
// Usage : Just include this header file.
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "url_parser_function.h"

class URLParser
{
public:
	struct HTTP_URL
	{
		std::string scheme;
		std::string host;
		std::string port;
		std::vector<std::string> path;
		std::string query_string;
		std::unordered_map<std::string, std::string> query;
	};

public:
	static HTTP_URL Parse(const std::string& input_url)
	{
		HTTP_URL http_url;

		size_t st = 0;
		size_t before = 0;

		URLParserFunction::FindKeyword(input_url, st, before, "://", http_url.scheme);
		URLParserFunction::FindKeyword(input_url, st, before, "/", http_url.host);

		size_t temp_st = 0;
		size_t temp_before = 0;
		std::string temp_ip;
		std::string temp_port;

		if (true == URLParserFunction::FindKeyword(http_url.host, temp_st, temp_before, ":", temp_ip))
		{
			http_url.port = std::string(&http_url.host[temp_before]);
			http_url.host = temp_ip;
		}

		while (true)
		{
			std::string path;
			if (false == URLParserFunction::FindKeyword(input_url, st, before, "/", path))
				break;

			http_url.path.push_back(path);
		}

		std::string path;
		if (false == URLParserFunction::FindKeyword(input_url, st, before, "?", path))
		{
			path = std::string(&input_url[st + 1]);
			http_url.path.push_back(path);
			return http_url;
		}

		if (st < input_url.length())
		{
			http_url.query_string = std::string(&input_url[st + 1]);
			if (false == http_url.query_string.empty())
			{
				std::string query;
				st = 0;
				before = 0;

				while (true)
				{
					std::string key, value;

					if (false == URLParserFunction::FindKeyword(http_url.query_string, st, before, "&", query))
					{
						URLParserFunction::SplitQueryString(std::string(&http_url.query_string[before]), "=", key, value);
						http_url.query.insert(std::unordered_map<std::string, std::string>::value_type(key, value));
						break;
					}

					URLParserFunction::SplitQueryString(query, "=", key, value);
					http_url.query.insert(std::unordered_map<std::string, std::string>::value_type(key, value));
				}
			}
		}

		return http_url;
	};
};
