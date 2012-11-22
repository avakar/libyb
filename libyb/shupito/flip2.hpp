#ifndef LIBYB_SHUPITO_FLIP2_HPP
#define LIBYB_SHUPITO_FLIP2_HPP

#include "../async/task.hpp"
#include "../usb/usb_device.hpp"

namespace yb {

class flip2
{
public:
	typedef size_t memory_id_t;
	typedef uint32_t offset_t;

	flip2();

	void open(usb_device const & dev);
	void close();

	bool is_open() const;

	task<void> clear_errors();

	task<void> read_memory(memory_id_t mem, offset_t offset, uint8_t * buffer, size_t size);
	task<bool> blank_check(memory_id_t mem, offset_t first, offset_t size);

	task<void> chip_erase();
	task<void> write_memory(memory_id_t mem, offset_t offset, uint8_t const * buffer, size_t size);

	task<void> start_application();

private:
	usb_device m_device;
	uint8_t m_interface_number;
	size_t m_packet_size;

	memory_id_t m_current_mem_id;
	uint16_t m_current_page;
	bool m_mem_page_selected;
};

} // namespace yb

#endif // LIBYB_SHUPITO_FLIP2_HPP
