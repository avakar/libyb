#ifndef LIBYB_DESCRIPTOR_HPP
#define LIBYB_DESCRIPTOR_HPP

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

namespace yb {

class device_descriptor
{
public:
	struct config
	{
		std::string guid;
		uint8_t flags;
		uint8_t cmd;
		uint8_t cmd_count;
		std::vector<uint8_t> actseq;
		std::vector<uint8_t> data;

		bool empty() const { return guid.empty(); }
		bool always_active() const { return (flags & 1) != 0; }
		bool default_active() const { return (flags & 2) != 0; }
	};

	bool empty() const { return m_dev_guid.empty(); }
	std::string device_guid() const { return m_dev_guid; }
	config const * get_config(std::string const & str) const;
	static device_descriptor parse(uint8_t const * first, uint8_t const * last);

private:
	std::string m_dev_guid;
	std::map<std::string, config> m_interface_map;
};

typedef device_descriptor::config device_config;

} // namespace yb

#endif // LIBYB_DESCRIPTOR_HPP
