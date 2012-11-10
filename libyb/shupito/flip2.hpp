#ifndef LIBYB_SHUPITO_FLIP2_HPP
#define LIBYB_SHUPITO_FLIP2_HPP

#include "../async/task.hpp"
#include "../usb/usb.h"

namespace yb {

class flip2
{
public:
	typedef size_t memory_id_t;
	typedef uint32_t offset_t;

	flip2();

	void open(usb_device & dev);
	void close();

	task<size_t> read_memory(memory_id_t mem, offset_t offset, uint8_t * buffer, size_t size);
	task<bool> blank_check(memory_id_t mem, offset_t first, offset_t size);

private:
	usb_device m_device;
	uint8_t m_interface_number;
	memory_id_t m_current_mem_id;
	uint16_t m_current_page;
	bool m_mem_page_selected;
};

} // namespace yb

#endif // LIBYB_SHUPITO_FLIP2_HPP
