#include "sparse_buffer.hpp"
#include <utility>
using namespace yb;

void sparse_buffer::clear()
{
	m_regions.clear();
}

sparse_buffer::region_iterator sparse_buffer::region_begin() const
{
	return m_regions.begin();
}

sparse_buffer::region_iterator sparse_buffer::region_end() const
{
	return m_regions.end();
}

bool sparse_buffer::intersects(size_t address, size_t length) const
{
	region_map_t::const_iterator it = m_regions.upper_bound(address);
	return (it != m_regions.begin() && std::prev(it)->first + std::prev(it)->second.size() > address)
		|| (it != m_regions.end() && it->first < address + length);
}

void sparse_buffer::read_region(size_t address, uint8_t * buffer, size_t length) const
{
	region_map_t::const_iterator it = m_regions.upper_bound(address);
	if (it != m_regions.begin())
		--it;

	for (; it != m_regions.end() && it->first < address + length; ++it)
	{
		std::size_t start = (std::max)(address, it->first);
		std::size_t stop = (std::min)(address + length, it->first + it->second.size());

		if (start < stop)
		{
			std::copy(
				it->second.data() + (start - it->first),
				it->second.data() + (stop - it->first),
				buffer + (start - address));
		}
	}
}

void sparse_buffer::read_region(size_t address, uint8_t * buffer, size_t length, uint8_t fill) const
{
	std::fill(buffer, buffer + length, fill);
	this->read_region(address, buffer, length);
}

size_t sparse_buffer::top_address() const
{
	if (m_regions.empty())
		return 0;

	region_map_t::const_iterator last_region = std::prev(m_regions.end());
	return last_region->first + last_region->second.size();
}

void sparse_buffer::write_region(size_t address, buffer_ref const & buffer)
{
	size_t regaddr;
	std::vector<uint8_t> * reg = 0;

	region_map_t::iterator regit = m_regions.upper_bound(address);

	{
		region_map_t::iterator p;
		if (regit != m_regions.begin())
			p = std::prev(regit);

		if (regit == m_regions.begin() || p->first + p->second.size() < address)
		{
			regaddr = address;
			reg = &m_regions[address];
			reg->assign(buffer.begin(), buffer.end());
		}
		else
		{
			regaddr = p->first;
			p->second.resize(address + buffer.size() - p->first);
			std::copy(buffer.begin(), buffer.end(), p->second.begin() + (address - p->first));
		}
	}

	while (regit != m_regions.end() && regit->first < address + buffer.size())
	{
		if (regit->first + regit->second.size() > regaddr + reg->size())
			reg->insert(reg->end(), regit->second.begin() + (regaddr + reg->size() - regit->first), regit->second.end());

		region_map_t::iterator next = std::next(regit);
		m_regions.erase(regit);
		regit = next;
	}
}
