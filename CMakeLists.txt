cmake_minimum_required(VERSION 2.8)
project(yb)

set(SOURCES
	libyb/buffer.hpp
	libyb/buffer.cpp
	libyb/descriptor.cpp
	libyb/stream_parser.cpp
	libyb/tunnel.cpp
	libyb/vector_ref.hpp
	libyb/async/descriptor_reader.cpp
	libyb/async/descriptor_reader.hpp
	libyb/async/device.cpp
	libyb/async/group.hpp
	libyb/async/mock_stream.cpp
	libyb/async/null_stream.cpp
	libyb/async/stream.cpp
	libyb/async/stream_device.cpp
	libyb/async/timer.cpp
	libyb/async/async_channel.hpp
	libyb/async/async_runner.hpp
	libyb/async/cancel_exception.hpp
	libyb/async/cancel_level.hpp
	libyb/async/console_stream.hpp
	libyb/async/detail/cancel_level_upgrade_task.hpp
	libyb/async/detail/canceller_task.hpp
	libyb/async/detail/circular_buffer.hpp
	libyb/async/detail/double_buffer_task.hpp
	libyb/async/detail/exit_guard_task.hpp
	libyb/async/detail/channel_detail.hpp
	libyb/async/detail/loop_task.hpp
	libyb/async/detail/parallel_composition_task.hpp
	libyb/async/detail/parallel_compositor.cpp
	libyb/async/detail/parallel_compositor.hpp
	libyb/async/detail/parallel_compositor_impl.hpp
	libyb/async/detail/prepared_task.hpp
	libyb/async/detail/prepared_task_impl.hpp
	libyb/async/detail/promise_task.hpp
	libyb/async/detail/runner_impl.hpp
	libyb/async/detail/sequential_composition_task.hpp
	libyb/async/detail/task_fwd.hpp
	libyb/async/detail/task_impl.hpp
	libyb/async/detail/task_result.hpp
	libyb/async/detail/wait_context.hpp
	libyb/async/detail/exit_guard_task.cpp
	libyb/async/detail/parallel_composition_task.cpp
	libyb/async/detail/task_impl.cpp
	libyb/async/device.hpp
	libyb/async/double_buffer.hpp
	libyb/async/channel.hpp
	libyb/async/mock_stream.hpp
	libyb/async/null_stream.hpp
	libyb/async/promise.hpp
	libyb/async/resumable.hpp
	libyb/async/runner.hpp
	libyb/async/serial_port.hpp
	libyb/async/stream.hpp
	libyb/async/stream_device.hpp
	libyb/async/sync_runner.hpp
	libyb/async/task.hpp
	libyb/async/task_base.hpp
	libyb/async/timer.hpp
	libyb/net/net.cpp
	libyb/net/net.hpp
	libyb/descriptor.hpp
	libyb/packet.hpp
	libyb/packet_handler.hpp
#	libyb/shupito/escape_sequence.hpp
	libyb/shupito/flip2.hpp
	libyb/stream_parser.hpp
	libyb/tunnel.hpp
#	libyb/shupito/escape_sequence.cpp
	libyb/shupito/flip2.cpp
	libyb/usb/bulk_stream.cpp
	libyb/usb/interface_guard.cpp
	libyb/usb/usb_descriptors.cpp
	libyb/usb/usb_device.cpp
	libyb/usb/bulk_stream.hpp
	libyb/usb/detail/usb_device_core_fwd.hpp
	libyb/usb/detail/usb_request_context.hpp
	libyb/usb/interface_guard.hpp
	libyb/usb/usb_context.hpp
	libyb/usb/usb_descriptors.hpp
	libyb/usb/usb_device.hpp
	libyb/utils/ihex_file.cpp
	libyb/utils/sparse_buffer.cpp
	libyb/utils/svf_file.cpp
	libyb/utils/utf.cpp
	libyb/utils/except.hpp
	libyb/utils/ihex_file.hpp
	libyb/utils/noncopyable.hpp
	libyb/utils/signal.hpp
	libyb/utils/sparse_buffer.hpp
	libyb/utils/svf_file.hpp
	libyb/utils/tuple_less.hpp
	libyb/utils/utf.hpp
	)

if (WIN32)
set(SOURCES ${SOURCES}
	libyb/async/detail/win32_group.cpp
	libyb/async/detail/win32_affinity_task.hpp
	libyb/async/detail/win32_handle_task.hpp
	libyb/async/detail/win32_wait_context.hpp
	libyb/async/detail/win32_affinity_task.cpp
	libyb/async/detail/win32_async_channel.cpp
	libyb/async/detail/win32_async_runner.cpp
	libyb/async/detail/win32_console_stream.cpp
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
	libyb/usb/detail/win32_usb_device_core.hpp
	libyb/utils/detail/win32_file_operation.cpp
	libyb/utils/detail/win32_mutex.cpp
	libyb/utils/detail/win32_overlapped.cpp
	libyb/utils/detail/scoped_win32_handle.hpp
	libyb/utils/detail/win32_file_operation.hpp
	libyb/utils/detail/win32_mutex.hpp
	libyb/utils/detail/win32_overlapped.hpp
	)
else (WIN32)
set(SOURCES ${SOURCES}
	libyb/async/detail/linux_fdpoll_task.hpp
	libyb/async/detail/linux_wait_context.hpp
	libyb/async/detail/unix_wait_context.hpp
	libyb/async/detail/linux_async_channel.cpp
	libyb/async/detail/linux_async_runner.cpp
	libyb/async/detail/linux_serial_port.cpp
	libyb/async/detail/linux_sync_runner.cpp
	libyb/async/detail/linux_timer.cpp
	libyb/async/detail/linux_wait_context.cpp
	libyb/usb/detail/linux_usb_context.cpp
	libyb/usb/detail/linux_usb_device.cpp
	libyb/usb/detail/linux_usb_device_core.hpp
	libyb/usb/detail/udev.hpp
	libyb/usb/detail/unix_usb_device_core.hpp
	libyb/utils/detail/pthread_mutex.cpp
	libyb/utils/detail/pthread_mutex.hpp
	libyb/utils/detail/scoped_unix_fd.hpp
	)
endif (WIN32)

add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_library(yb STATIC ${SOURCES})

install(TARGETS yb ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(DIRECTORY libyb DESTINATION ${CMAKE_INSTALL_PREFIX}/include FILES_MATCHING PATTERN "*.hpp")
