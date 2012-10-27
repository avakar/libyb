#ifndef LIBYB_ASYNC_DESCRIPTOR_READER_HPP
#define LIBYB_ASYNC_DESCRIPTOR_READER_HPP

#include "../descriptor.hpp"
#include "device.hpp"
#include "task.hpp"

namespace yb {

task<device_descriptor> read_device_descriptor(device & d);

}

#endif // LIBYB_ASYNC_DESCRIPTOR_READER_HPP
