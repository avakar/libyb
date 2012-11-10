#include "flip2.hpp"
#include <cassert>
using namespace yb;

static usb_control_code_t const usbcc_download = { 0x21, 0x01 };
static usb_control_code_t const usbcc_upload = { 0xa1, 0x02 };
static usb_control_code_t const usbcc_get_status = { 0xa1, 0x03 };
static usb_control_code_t const usbcc_clear_status = { 0x21, 0x04 };

struct flip_command_code_t
{
	uint8_t group;
	uint8_t command;
};

static flip_command_code_t const flipcc_select_memory = { 0x06, 0x03 };
static flip_command_code_t const flipcc_read_memory   = { 0x03, 0x00 };
static flip_command_code_t const flipcc_blank_check   = { 0x03, 0x01 };

enum flip_status_t
{
	status_ok            = 0x0002,
	status_stall         = 0x0f0a,
	status_mem_unknown   = 0x030a,
	status_mem_protected = 0x0302,
	status_out_of_range  = 0x080a,
	status_blank_fail    = 0x0502,
	status_erase_ongoing = 0x0904
};

flip2::flip2()
	: m_interface_number(0), m_mem_page_selected(false)
{
}

void flip2::open(usb_device & dev)
{
	m_device = dev;
	m_mem_page_selected = false;
}

void flip2::close()
{
	m_device.clear();
}

static task<void> select_memory_space(usb_device & dev, flip2::memory_id_t mem)
{
	assert(!dev.empty());
	std::shared_ptr<std::vector<uint8_t>> ctx = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>(4));
	(*ctx)[0] = flipcc_select_memory.group;
	(*ctx)[1] = flipcc_select_memory.command;
	(*ctx)[2] = 0;
	(*ctx)[3] = (uint8_t)mem;
	return dev.control_write(usbcc_download, 0, 0, ctx->data(), ctx->size()).follow_with([ctx]{});
}

static task<void> select_memory_page(usb_device & dev, uint16_t page)
{
	assert(!dev.empty());
	std::shared_ptr<std::vector<uint8_t>> ctx = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>(6));
	(*ctx)[0] = flipcc_select_memory.group;
	(*ctx)[1] = flipcc_select_memory.command;
	(*ctx)[2] = 1;
	(*ctx)[3] = (uint8_t)(page >> 8);
	(*ctx)[4] = (uint8_t)page;
	(*ctx)[5] = 0;
	return dev.control_write(usbcc_download, 0, 0, ctx->data(), ctx->size()).follow_with([ctx]{});
}

static task<size_t> read_memory_range(usb_device & dev, flip2::offset_t offset, uint8_t * buffer, size_t size)
{
	assert(!dev.empty());
	assert(size != 0);

	flip2::offset_t last = offset + size - 1;

	std::shared_ptr<std::vector<uint8_t>> ctx = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>(6));
	(*ctx)[0] = flipcc_read_memory.group;
	(*ctx)[1] = flipcc_read_memory.command;
	(*ctx)[2] = (uint8_t)(offset >> 8);
	(*ctx)[3] = (uint8_t)offset;
	(*ctx)[4] = (uint8_t)(last >> 8);
	(*ctx)[5] = (uint8_t)last;
	return dev.control_write(usbcc_download, 0, 0, ctx->data(), ctx->size()).then([ctx, &dev, buffer, size] {
		return dev.control_read(usbcc_upload, 0, 0, buffer, size);
	});
}

static task<flip_status_t> get_status(usb_device & dev)
{
	std::shared_ptr<std::vector<uint8_t>> ctx = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>(6));
	return dev.control_read(usbcc_get_status, 0, 0, ctx->data(), ctx->size()).then([&dev, ctx](size_t r) -> task<flip_status_t> {
		if (r != 6)
			return async::raise<flip_status_t>(std::runtime_error("partial status read"));
		return async::value(flip_status_t(((*ctx)[0] << 8) | (*ctx)[4]));
	});
}

static task<bool> blank_check_range(usb_device & dev, flip2::offset_t offset, flip2::offset_t size)
{
	assert(!dev.empty());
	assert(size != 0);

	flip2::offset_t last = offset + size - 1;

	std::shared_ptr<std::vector<uint8_t>> ctx = std::make_shared<std::vector<uint8_t>>(std::vector<uint8_t>(6));
	(*ctx)[0] = flipcc_blank_check.group;
	(*ctx)[1] = flipcc_blank_check.command;
	(*ctx)[2] = (uint8_t)(offset >> 8);
	(*ctx)[3] = (uint8_t)offset;
	(*ctx)[4] = (uint8_t)(last >> 8);
	(*ctx)[5] = (uint8_t)last;
	return dev.control_write(usbcc_download, 0, 0, ctx->data(), ctx->size()).then([ctx, &dev] {
		return get_status(dev);
	}).then([](flip_status_t st) -> task<bool> {
		if (st == status_ok)
			return async::value(true);
		if (st == status_blank_fail)
			return async::value(false);
		return async::raise<bool>(std::runtime_error("blank check error"));
	});
}

task<size_t> flip2::read_memory(memory_id_t mem, offset_t offset, uint8_t * buffer, size_t size)
{
	assert(!m_device.empty());

	if (size > 1024)
		size = 1024;

	return (!m_mem_page_selected || m_current_mem_id != mem? select_memory_space(m_device, mem): async::value()).then([this, offset, mem]() -> task<void> {
		m_current_mem_id = mem;
		return !m_mem_page_selected || m_current_page != (uint16_t)(offset >> 16)? select_memory_page(m_device, (uint16_t)(offset >> 16)): async::value();
	}).then([this, offset, buffer, size]() -> task<size_t> {
		m_current_page = (uint16_t)(offset >> 16);
		m_mem_page_selected = true;
		return read_memory_range(m_device, offset, buffer, size);
	});
}

task<bool> flip2::blank_check(memory_id_t mem, offset_t first, offset_t size)
{
	assert(!m_device.empty());

	return (!m_mem_page_selected || m_current_mem_id != mem? select_memory_space(m_device, mem): async::value()).then([this, first, mem]() -> task<void> {
		m_current_mem_id = mem;
		return !m_mem_page_selected || m_current_page != (uint16_t)(first >> 16)? select_memory_page(m_device, (uint16_t)(first >> 16)): async::value();
	}).then([this, first, size]() -> task<bool> {
		m_current_page = (uint16_t)(first >> 16);
		m_mem_page_selected = true;
		return blank_check_range(m_device, first, size);
	});
}
