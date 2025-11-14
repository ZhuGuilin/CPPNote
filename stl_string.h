#pragma once

#include "Observer.h"

#include <string>
#include <print>
#include <format>
#include <cctype>

class STL_String : public Observer
{
public:

	//	预定义两位数字的ASCII字符数组 用于高效数字转字符串
	static constexpr char TWO_ASCII_DIGITS[100][2] = 
	{
	  {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'},
	  {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
	  {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'},
	  {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
	  {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'},
	  {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
	  {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'},
	  {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
	  {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'},
	  {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
	  {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'},
	  {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
	  {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'},
	  {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
	  {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'},
	  {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
	  {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'},
	  {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
	  {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'},
	  {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
	};

	//	高效无符号整数转字符串函数 返回字符串结束位置指针 copy from folly itoa.h
	char* UInt32ToBufferLeft(std::uint32_t u, char* buffer) 
	{
		int digits;
		const char* ASCII_digits = nullptr;
		// The idea of this implementation is to trim the number of divides to as few
		// as possible by using multiplication and subtraction rather than mod (%),
		// and by outputting two digits at a time rather than one.
		// The huge-number case is first, in the hopes that the compiler will output
		// that case in one branch-free block of code, and only output conditional
		// branches into it from below.
		if (u >= 1000000000) {  // >= 1,000,000,000
			digits = u / 100000000;  // 100,000,000
			ASCII_digits = TWO_ASCII_DIGITS[digits];
			buffer[0] = ASCII_digits[0];
			buffer[1] = ASCII_digits[1];
			buffer += 2;
		sublt100_000_000:
			u -= digits * 100000000;  // 100,000,000
		lt100_000_000:
			digits = u / 1000000;  // 1,000,000
			ASCII_digits = TWO_ASCII_DIGITS[digits];
			buffer[0] = ASCII_digits[0];
			buffer[1] = ASCII_digits[1];
			buffer += 2;
		sublt1_000_000:
			u -= digits * 1000000;  // 1,000,000
		lt1_000_000:
			digits = u / 10000;  // 10,000
			ASCII_digits = TWO_ASCII_DIGITS[digits];
			buffer[0] = ASCII_digits[0];
			buffer[1] = ASCII_digits[1];
			buffer += 2;
		sublt10_000:
			u -= digits * 10000;  // 10,000
		lt10_000:
			digits = u / 100;
			ASCII_digits = TWO_ASCII_DIGITS[digits];
			buffer[0] = ASCII_digits[0];
			buffer[1] = ASCII_digits[1];
			buffer += 2;
		sublt100:
			u -= digits * 100;
		lt100:
			digits = u;
			ASCII_digits = TWO_ASCII_DIGITS[digits];
			buffer[0] = ASCII_digits[0];
			buffer[1] = ASCII_digits[1];
			buffer += 2;
		done:
			*buffer = 0;
			return buffer;
		}

		if (u < 100) {
			digits = u;
			if (u >= 10) goto lt100;
			*buffer++ = '0' + digits;
			goto done;
		}
		if (u < 10000) {   // 10,000
			if (u >= 1000) goto lt10_000;
			digits = u / 100;
			*buffer++ = '0' + digits;
			goto sublt100;
		}
		if (u < 1000000) {   // 1,000,000
			if (u >= 100000) goto lt1_000_000;
			digits = u / 10000;  //    10,000
			*buffer++ = '0' + digits;
			goto sublt10_000;
		}
		if (u < 100000000) {   // 100,000,000
			if (u >= 10000000) goto lt100_000_000;
			digits = u / 1000000;  //   1,000,000
			*buffer++ = '0' + digits;
			goto sublt1_000_000;
		}
		// we already know that u < 1,000,000,000
		digits = u / 100000000;   // 100,000,000
		*buffer++ = '0' + digits;
		goto sublt100_000_000;
	}

	//	高效整数转字符串函数 返回字符串起始位置指针 copy from folly itoa.h
	static constexpr int Int32ToBufferOffset = 11;
	static char* Int32ToBuffer(std::int32_t i, char* buffer) {
		// We could collapse the positive and negative sections, but that
		// would be slightly slower for positive numbers...
		// 12 bytes is enough to store -2**32, -4294967296.
		char* p = buffer + Int32ToBufferOffset;
		*p-- = '\0';
		if (i >= 0) {
			do {
				*p-- = '0' + i % 10;
				i /= 10;
			} while (i > 0);
			return p + 1;
		}
		else {
			// On different platforms, % and / have different behaviors for
			// negative numbers, so we need to jump through hoops to make sure
			// we don't divide negative numbers.
			if (i > -10) {
				i = -i;
				*p-- = '0' + i;
				*p = '-';
				return p;
			}
			else {
				// Make sure we aren't at MIN_INT, in which case we can't say i = -i
				i = i + 10;
				i = -i;
				*p-- = '0' + i % 10;
				// Undo what we did a moment ago
				i = i / 10 + 1;
				do {
					*p-- = '0' + i % 10;
					i /= 10;
				} while (i > 0);
				*p = '-';
				return p;
			}
		}
	}

	struct PrintObject
	{
		std::string ip;
		std::uint16_t port;
		std::string name;

		std::string ToString() const
		{
			return std::format("IP: {}, Port: {}, Name: {}", ip, port, name);
		}
	};

	//	字符串分割函数
	static void split1(std::vector<std::string>& tokens, std::string_view&& str, const char* c)
	{
		tokens.clear();
		std::string::size_type b = 0, e = 0;
		for (;;)
		{
			b = str.find_first_not_of(c, e);
			if (std::string::npos == b)
				break;

			e = str.find_first_of(c, b);
			tokens.emplace_back(str.substr(b, e - b));

			if (std::string::npos == e)
				break;
		}
	}

	//	比split1更高效 快一倍左右
	static void split2(std::vector<std::string>& tokens, std::string_view&& str, char delimiter = ' ') 
	{
		size_t start = 0;
		size_t end = str.find(delimiter);
		tokens.clear();

		while (end != std::string::npos) 
		{
			tokens.emplace_back(str.substr(start, end - start));
			start = end + 1;
			end = str.find(delimiter, start);
		}

		tokens.emplace_back(str.substr(start));
	}

	//	字符串替换函数
	void replace(const std::string& s, const std::string& oldsub,
		const std::string& newsub, bool replace_all,
		std::string* res) 
	{
		if (oldsub.empty()) 
		{
			res->append(s);  // if empty, append the given string.
			return;
		}

		res->clear();
		std::string::size_type start_pos = 0;
		std::string::size_type pos;
		do {
			pos = s.find(oldsub, start_pos);
			if (pos == std::string::npos) 
				break;
			
			res->append(s, start_pos, pos - start_pos);
			res->append(newsub);
			start_pos = pos + oldsub.size();  // start searching again after the "old"
		} while (replace_all);

		res->append(s, start_pos, s.length() - start_pos);
	}

	void Test() override
	{
		std::print(" ===== STL_String Begin =====\n");

		//	String view 用于固定字符串，避免不必要的字符串拷贝
		std::string_view sv = "This is a string_view example.";
		std::print("String View: {}\n", sv);

		std::string str1 = "Hello, ";
		std::string str2 = "World!";
		std::string str3 = str1 + str2;
		std::print("Concatenated String: {}\n", str3);

		//	Formatted string using std::format
		auto str = std::format("Forma+tted S+tring: {}, +number: {}\n", "must 4==7s+ 88jk en+d 00hd++", 23564);
		std::print("{}", str);

		//	String splitting performance test
		constexpr auto TEST_COUNT = 10;
		auto start = std::chrono::high_resolution_clock::now();
		std::vector<std::string> result;
		for (size_t i = 0; i < TEST_COUNT; ++i)
		{
			auto s = str;
			split1(result, s, " ");
		}
		auto end = std::chrono::high_resolution_clock::now();
		std::print("split1 use: {} microseconds\n",
			std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

		start = std::chrono::high_resolution_clock::now();
		for (size_t i = 0; i < TEST_COUNT; ++i)
		{
			auto s = str;
			split2(result, s, ' ');
		}
		end = std::chrono::high_resolution_clock::now();
		std::print("split2 use: {} microseconds\n",
			std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

		//	String replacement and case conversion
		auto replace_str = str;
		std::string replaced;
		replace(replace_str, "+", "-", true, &replaced);
		std::print("Replaced String + -> -: {}\n", replaced);

		replace(replace_str, "+", "", true, &replaced);
		std::print("Replaced String del + : {}\n", replaced);

		std::transform(replace_str.begin(), replace_str.end(), replace_str.begin(),
			[](unsigned char c) {
				return std::tolower(c); 
			});
		std::print("tolower replace_str: {}\n", replace_str);

		std::transform(replace_str.begin(), replace_str.end(), replace_str.begin(),
			[](unsigned char c) {
				return std::toupper(c);
			});
		std::print("toupper replace_str: {}\n", replace_str);
		
		//	Custom object formatting
		PrintObject obj{ "192.168.1.2", 8080, "TestServer" };
		std::print("Custom Object: {}\n", obj.ToString());

		//	Various format specifiers
		std::print("Pi approx: {:.5f}\n", 3.1415926535);
		std::print("Hex number: {:#x}\n", 255);
		std::print("Binary number: {:#b}\n", 10);

		char buffer[Int32ToBufferOffset + 1];
		std::print("Integer to string conversion: {}\n", Int32ToBuffer(-845014722, buffer));

		//	结果是 \0开头，有问题!
		std::print("Unsigned Integer to string conversion: {}\n", UInt32ToBufferLeft(3456789123u, buffer));

		std::print(" ===== STL_String End =====\n");
	}
};
