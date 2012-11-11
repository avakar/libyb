#ifndef LIBYB_UTILS_SPARSE_BUFFER_HPP
#define LIBYB_UTILS_SPARSE_BUFFER_HPP

#include "../vector_ref.hpp"
#include <map>
#include <vector>
#include <stdint.h>
#include <stddef.h>

namespace yb {

class sparse_buffer
{
public:
	typedef std::map<size_t, std::vector<uint8_t>>::const_iterator region_iterator;

	void clear();

	region_iterator region_begin() const;
	region_iterator region_end() const;
	size_t top_address() const;

	bool intersects(size_t address, size_t length) const;

	void read_region(size_t address, uint8_t * buffer, size_t length) const;
	void read_region(size_t address, uint8_t * buffer, size_t length, uint8_t fill) const;
	void write_region(size_t address, buffer_ref const & buffer);

private:
	typedef std::map<size_t, std::vector<uint8_t>> region_map_t;
	region_map_t m_regions;
};

} // namespace yb

#endif // LIBYB_UTILS_SPARSE_BUFFER_HPP
