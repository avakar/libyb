#include "utf.hpp"
#include <cassert>
using namespace yb;

static void encode_utf8(std::string & out, uint32_t cp)
{
	assert(cp < 0x110000);

	typedef char char_type;

	char_type data[4];
	size_t res = 0;

	if (cp < 0x7f)
	{
		data[res++] = (char_type)cp;
	}
	else if (cp < 0x7ff)
	{
		data[res++] = (char_type)(0xC0 | (cp >> 6));
		data[res++] = (char_type)(0x80 | (cp & 0x3f));
	}
	else if (cp < 0xffff)
	{
		data[res++] = (char_type)(0xe0 | (cp >> 12));
		data[res++] = (char_type)(0x80 | ((cp >> 6) & 0x3f));
		data[res++] = (char_type)(0x80 | (cp & 0x3f));
	}
	else
	{
		data[res++] = (char_type)(0xf0 | (cp >> 18));
		data[res++] = (char_type)(0x80 | ((cp >> 12) & 0x3f));
		data[res++] = (char_type)(0x80 | ((cp >> 6) & 0x3f));
		data[res++] = (char_type)(0x80 | (cp & 0x3f));
	}

	out.append(data, res);
}

std::string yb::utf16le_to_utf8(buffer_ref const & u16)
{
	if (u16.size() % 2 != 0)
		throw std::runtime_error("odd number of bytes in a utf-16le string");

	std::string u8;

	uint16_t high_surrogate = 0;
	for (size_t i = 0; i < u16.size(); i += 2)
	{
		uint16_t cu = u16[i] | (u16[i+1] << 8);
		if (cu >= 0xDC00 && cu < 0xE000)
		{
			// low surrogate

			if (!high_surrogate)
				throw std::runtime_error("matching high surrogate not found");

			encode_utf8(u8, ((uint32_t)(high_surrogate - 0xD800) << 10) | (cu - 0xDC00));
			high_surrogate = 0;
		}
		else if (cu >= 0xD800 && cu < 0xDC00)
		{
			// high surrogate

			if (high_surrogate)
				throw std::runtime_error("adjacent high surrogates");

			high_surrogate = cu;
		}
		else
		{
			if (high_surrogate)
				throw std::runtime_error("matching low surrogate not found");
			encode_utf8(u8, cu);
		}
	}

	return u8;
}
