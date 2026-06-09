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
		std::string userinfo;
		std::string host;
		std::string port;
		std::vector<std::string> path;
		std::string query_string;
		std::unordered_map<std::string, std::string> query;
		std::string fragment;
	};

public:
	static HTTP_URL Parse(const std::string& input_url)
	{
		HTTP_URL http_url;

		if (input_url.empty())
			return http_url;

		size_t st = 0;
		size_t before = 0;

		// scheme 파싱 (예: "http", "https")
		// has_authority: authority(host/userinfo/port) 파싱 여부를 결정하는 플래그
		// - "://" 있음 → scheme 있는 절대 URL
		// - "//"로 시작 → scheme-relative URL
		// - 그 외(상대 경로 등) → authority 없음, path 파싱만 수행
		bool has_authority = URLParserFunction::FindKeyword(input_url, st, before, "://", http_url.scheme);
		if (!has_authority && input_url.size() >= 2 && input_url[0] == '/' && input_url[1] == '/')
		{
			has_authority = true;
			before = 2;
		}

		if (has_authority)
		{
			// userinfo 파싱 — "@" 앞의 "user:pass" 부분 분리
			// authority 영역(다음 "/" 또는 "?" 또는 "#"까지)에서만 "@"를 탐색
			size_t authority_end = input_url.find_first_of("/?#", before);
			size_t at_pos = input_url.find('@', before);
			if (at_pos != std::string::npos &&
				(authority_end == std::string::npos || at_pos < authority_end))
			{
				http_url.userinfo = input_url.substr(before, at_pos - before);
				before = at_pos + 1;
			}

			// host 파싱 — "/" 또는 "?" 또는 "#"가 나오기 전까지가 host
			// path가 없는 URL(예: http://example.com)도 처리
			size_t host_end = input_url.find_first_of("/?#", before);
			if (host_end == std::string::npos)
			{
				http_url.host = input_url.substr(before);
				before = input_url.length();
			}
			else
			{
				http_url.host = input_url.substr(before, host_end - before);
				before = host_end;
			}

			// host에서 port 분리 (IPv6 bracketed notation 지원)
			if (!http_url.host.empty() && http_url.host.front() == '[')
			{
				// IPv6: [2001:db8::1] 또는 [2001:db8::1]:8080
				size_t bracket_close = http_url.host.find(']');
				if (bracket_close != std::string::npos)
				{
					// bracket 뒤에 ":port"가 있는지 확인
					if (bracket_close + 1 < http_url.host.length() && http_url.host[bracket_close + 1] == ':')
					{
						http_url.port = http_url.host.substr(bracket_close + 2);
					}
					// bracket 안의 주소만 추출 ([ ] 제거)
					http_url.host = http_url.host.substr(1, bracket_close - 1);
				}
			}
			else
			{
				size_t colon_pos = http_url.host.find(':');
				if (colon_pos != std::string::npos)
				{
					http_url.port = http_url.host.substr(colon_pos + 1);
					http_url.host = http_url.host.substr(0, colon_pos);
				}
			}
		}

		// port 유효성 검증 — 숫자만 허용, 범위 0-65535
		if (!http_url.port.empty())
		{
			bool valid_port = !http_url.port.empty();
			for (size_t i = 0; valid_port && i < http_url.port.length(); ++i)
			{
				if (http_url.port[i] < '0' || http_url.port[i] > '9')
					valid_port = false;
			}
			if (valid_port && http_url.port.length() <= 5)
			{
				unsigned long port_num = 0;
				for (size_t i = 0; i < http_url.port.length(); ++i)
					port_num = port_num * 10 + static_cast<unsigned long>(http_url.port[i] - '0');
				if (port_num > 65535)
					valid_port = false;
			}
			else if (http_url.port.length() > 5)
			{
				valid_port = false;
			}

			if (!valid_port)
				http_url.port.clear();
		}

		// fragment(#) 분리 — 이후 path/query 파싱 범위를 제한
		size_t frag_pos = input_url.find('#', before);
		size_t effective_end = (frag_pos != std::string::npos) ? frag_pos : input_url.length();

		if (frag_pos != std::string::npos && frag_pos + 1 < input_url.length())
		{
			http_url.fragment = input_url.substr(frag_pos + 1);
		}

		// path 파싱 — "?" 또는 "#" 이전까지의 "/" 구분 세그먼트
		size_t query_pos = input_url.find('?', before);
		if (query_pos != std::string::npos && query_pos >= effective_end)
			query_pos = std::string::npos; // "#" 뒤의 "?"는 무시

		size_t path_end = effective_end;
		if (query_pos != std::string::npos)
			path_end = query_pos;

		std::string path_str = input_url.substr(before, path_end - before);

		if (!path_str.empty())
		{
			size_t p_st = 0;
			size_t p_before = 0;

			while (true)
			{
				p_st = path_str.find('/', p_before);
				if (p_st == std::string::npos)
				{
					std::string seg = path_str.substr(p_before);
					if (!seg.empty())
						http_url.path.push_back(seg);
					break;
				}

				std::string seg = path_str.substr(p_before, p_st - p_before);
				if (!seg.empty())
					http_url.path.push_back(seg);

				p_before = p_st + 1;
			}
		}

		// query string 파싱 — "#" 이전까지만
		if (query_pos != std::string::npos && query_pos + 1 < effective_end)
		{
			http_url.query_string = input_url.substr(query_pos + 1, effective_end - query_pos - 1);

			size_t q_before = 0;
			while (q_before < http_url.query_string.length())
			{
				size_t amp_pos = http_url.query_string.find('&', q_before);
				std::string pair;

				if (amp_pos == std::string::npos)
				{
					pair = http_url.query_string.substr(q_before);
					q_before = http_url.query_string.length();
				}
				else
				{
					pair = http_url.query_string.substr(q_before, amp_pos - q_before);
					q_before = amp_pos + 1;
				}

				if (!pair.empty())
				{
					std::string key, value;
					URLParserFunction::SplitQueryString(pair, "=", key, value);
					if (!key.empty())
						http_url.query.insert(std::unordered_map<std::string, std::string>::value_type(key, value));
				}
			}
		}

		return http_url;
	};
};
