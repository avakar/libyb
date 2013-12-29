cmake_minimum_required(VERSION 2.8)
project(yb)

set(SOURCES
	libyb/descriptor.cpp
	libyb/stream_parser.cpp
	libyb/tunnel.cpp
	libyb/async/cancellation_token.cpp
	libyb/async/descriptor_reader.cpp
	libyb/async/device.cpp
	libyb/async/mock_stream.cpp
	libyb/async/null_stream.cpp
	libyb/async/stream.cpp
	libyb/async/stream_device.cpp
	libyb/async/timer.cpp
	libyb/async/detail/exit_guard_task.cpp
	libyb/async/detail/parallel_composition_task.cpp
	libyb/async/detail/task_impl.cpp
	libyb/net/net.cpp
	libyb/shupito/escape_sequence.cpp
	libyb/shupito/flip2.cpp
	libyb/usb/bulk_stream.cpp
	libyb/usb/interface_guard.cpp
	libyb/usb/usb_descriptors.cpp
	libyb/usb/usb_device.cpp
	libyb/utils/ihex_file.cpp
	libyb/utils/sparse_buffer.cpp
	libyb/utils/svf_file.cpp
	libyb/utils/utf.cpp
	)

if (WIN32)
set(SOURCES ${SOURCES}
	libyb/async/detail/win32_affinity_task.cpp
	libyb/async/detail/win32_async_channel.cpp
	libyb/async/detail/win32_async_runner.cpp
	libyb/async/detail/win32_handle_task.cpp
	libyb/async/detail/win32_prepared_task.cpp
	libyb/async/detail/win32_resumable.cpp
	libyb/async/detail/win32_serial_port.cpp
	libyb/async/detail/win32_sync_runner.cpp
	libyb/async/detail/win32_timer.cpp
	libyb/async/detail/win32_wait_context.cpp
	libyb/usb/detail/usb_request_context.cpp
	libyb/usb/detail/win32_usb_context.cpp
	libyb/usb/detail/win32_usb_device.cpp
	libyb/utils/detail/win32_file_operation.cpp
	libyb/utils/detail/win32_mutex.cpp
	libyb/utils/detail/win32_overlapped.cpp
	)
else (WIN32)
set(SOURCES ${SOURCES}
	libyb/async/detail/linux_async_channel.cpp
	libyb/async/detail/linux_async_runner.cpp
	libyb/async/detail/linux_serial_port.cpp
	libyb/async/detail/linux_sync_runner.cpp
	libyb/async/detail/linux_timer.cpp
	libyb/async/detail/linux_wait_context.cpp
	libyb/usb/detail/linux_usb_context.cpp
	libyb/usb/detail/linux_usb_device.cpp
	libyb/utils/detail/pthread_mutex.cpp
	)
endif (WIN32)

add_library(yb STATIC ${SOURCES})

install(TARGETS yb ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(DIRECTORY libyb DESTINATION ${CMAKE_INSTALL_PREFIX}/include FILES_MATCHING PATTERN "*.hpp")
