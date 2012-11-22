#include "descriptor.hpp"
#include <stdexcept>

static std::string make_guid(uint8_t const * p)
{
	std::string str;
	str.reserve(36);
	for (size_t i = 0; i < 16; ++i)
	{
		static char const digits[] = "0123456789abcdef";
		str.append(1, digits[p[i] >> 4]);
		str.append(1, digits[p[i] & 0xf]);
	}
	str.insert(20, 1, '-');
	str.insert(16, 1, '-');
	str.insert(12, 1, '-');
	str.insert(8, 1, '-');
	return str;
}

static void parse_config(
	uint8_t const *& first, uint8_t const * last,
	uint8_t & base_cmd, std::vector<uint8_t> & actseq,
	std::map<std::string, yb::device_descriptor::config> & intf_map)
{
	if ((last - first) < 19)
		throw std::runtime_error("Invalid descriptor.");

	yb::device_descriptor::config cfg;
	cfg.flags = *first++;
	cfg.guid = make_guid(first);
	first += 16;

	cfg.cmd = *first++;
	cfg.cmd_count = *first++;
	base_cmd += cfg.cmd_count;
	cfg.actseq = actseq;

	uint8_t data_len = *first++;
	if (last - first < data_len)
		throw std::runtime_error("Invalid descriptor.");

	cfg.data.assign(first, first + data_len);
	first += data_len;

	intf_map[cfg.guid] = std::move(cfg);
}

static void parse_group_config(
	uint8_t const *& first, uint8_t const * last,
	uint8_t & base_cmd, std::vector<uint8_t> & actseq,
	std::map<std::string, yb::device_descriptor::config> & intf_map)
{
	if (first == last)
		throw std::runtime_error("Invalid descriptor.");

	if (*first == 0)
	{
		++first;
		parse_config(first, last, base_cmd, actseq, intf_map);
		return;
	}

	uint8_t count = *first & 0x7f;

	if (*first & 0x80)
	{
		// Or
		++first;

		uint8_t next_base_cmd = base_cmd;
		for (uint8_t i = 0; i < count; ++i)
		{
			uint8_t or_base_cmd = base_cmd;
			actseq.push_back(i);
			parse_group_config(first, last, or_base_cmd, actseq, intf_map);
			actseq.pop_back();
			next_base_cmd = (std::max)(next_base_cmd, or_base_cmd);
		}
		base_cmd = next_base_cmd;
	}
	else
	{
		// And
		++first;
		for (uint8_t i = 0; i < count; ++i)
		{
			actseq.push_back(i);
			parse_group_config(first, last, base_cmd, actseq, intf_map);
			actseq.pop_back();
		}
	}
}

yb::device_descriptor yb::device_descriptor::parse(uint8_t const * first, uint8_t const * last)
{
	if ((last - first) < 17 || *first++ != 1)
		throw std::runtime_error("Invalid descriptor.");

	yb::device_descriptor res;
	res.m_dev_guid = make_guid(first);
	first += 16;

	uint8_t base_cmd = 1;
	std::vector<uint8_t> activation_seq;
	parse_group_config(first, last, base_cmd, activation_seq, res.m_interface_map);
	return res;
}

yb::device_descriptor::config const * yb::device_descriptor::get_config(std::string const & str) const
{
	std::map<std::string, config>::const_iterator it = m_interface_map.find(str);
	if (it == m_interface_map.end())
		return 0;
	return &it->second;
}
