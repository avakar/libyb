#include "ihex_file.hpp"
#include <sstream>
#include <cassert>
using namespace yb;

static bool from_hex(uint8_t & res, char ch)
{
	if ('0' <= ch && ch <= '9')
		res = ch - '0';
	else if ('a' <= ch && ch <= 'f')
		res = ch - 'a' + 10;
	else if ('A' <= ch && ch <= 'F')
		res = ch - 'A' + 10;
	else
		return false;

	return true;
}

static bool from_base_16(uint8_t * out, string_ref b16)
{
	assert(b16.size() % 2 == 0);

	for (; !b16.empty(); b16 += 2)
	{
		uint8_t b1, b2;
		if (!from_hex(b1, b16[0]) || !from_hex(b2, b16[1]))
			return false;

		*out++ = (b1 << 4) | b2;
	}

	return true;
}

ihex_parse_error::ihex_parse_error(kind_t kind, int line)
	: m_kind(kind), m_line(line)
{
}

const char * ihex_parse_error::what() const throw()
{
	return "ihex parsing failed";
}

ihex_parse_error::kind_t ihex_parse_error::kind() const
{
	return m_kind;
}

int ihex_parse_error::line() const
{
	return m_line;
}

yb::sparse_buffer yb::parse_ihex(string_ref const & data)
{
	std::istringstream ss(data);
	return parse_ihex(ss);
}


yb::sparse_buffer yb::parse_ihex(std::istream & hexfile)
{
	sparse_buffer res;

	size_t base = 0;
	std::vector<uint8_t> rec_nums;
	for (int lineno = 1;; ++lineno)
	{
		std::string line;
		if (!std::getline(hexfile, line))
			break;

		if (!line.empty() && line.end()[-1] == '\r')
			line.erase(line.end() - 1);

		if (line.empty())
			continue;

		if (line[0] != ':' || line.size() % 2 != 1)
			throw ihex_parse_error(ihex_parse_error::k_missing_leading_colon, lineno);

		rec_nums.resize(line.size() / 2);
		if (!from_base_16(rec_nums.data(), string_ref(line) + 1))
			throw ihex_parse_error(ihex_parse_error::k_unexpected_character, lineno);

		uint8_t length = rec_nums[0];
		uint16_t address = (rec_nums[1] << 8) | rec_nums[2];
		uint8_t rectype = rec_nums[3];

		if (length != rec_nums.size() - 5)
			throw ihex_parse_error(ihex_parse_error::k_mismatched_record_length, lineno);

		if (rectype == 4)
		{
			if (length != 2)
				throw ihex_parse_error(ihex_parse_error::k_mismatched_record_length, lineno);
			base = (rec_nums[4] * 0x100 + rec_nums[5]) << 16;
			continue;
		}

		if (rectype == 3)
		{
			continue;
		}

		if (rectype == 2)
		{
			if (length != 2)
				throw ihex_parse_error(ihex_parse_error::k_mismatched_record_length, lineno);
			base = (rec_nums[4] * 0x100 + rec_nums[5]) * 16;
			continue;
		}

		if (rectype == 1)
			break;

		if (rectype != 0)
			throw ihex_parse_error(ihex_parse_error::k_unknown_record_type, lineno);

		if (res.intersects(base + address, rec_nums.size() - 5))
			throw ihex_parse_error(ihex_parse_error::k_overlapping_regions, lineno);

		res.write_region(base + address, buffer_ref(rec_nums.data() + 4, rec_nums.size() - 5));
	}

	return res;
}

