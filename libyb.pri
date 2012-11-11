INCLUDEPATH += $$PWD
SOURCES += \
    $$PWD/libyb/descriptor.cpp \
    $$PWD/libyb/stream_parser.cpp \
    $$PWD/libyb/tunnel.cpp \
    $$PWD/libyb/async/async_runner.cpp \
    $$PWD/libyb/async/cancellation_token.cpp \
    $$PWD/libyb/async/descriptor_reader.cpp \
    $$PWD/libyb/async/device.cpp \
    $$PWD/libyb/async/mock_stream.cpp \
    $$PWD/libyb/async/null_stream.cpp \
    $$PWD/libyb/async/serial_port.cpp \
    $$PWD/libyb/async/stream.cpp \
    $$PWD/libyb/async/stream_device.cpp \
    $$PWD/libyb/async/timer.cpp \
    $$PWD/libyb/async/detail/parallel_composition_task.cpp \
    $$PWD/libyb/async/detail/task_impl.cpp \
    $$PWD/libyb/async/detail/win32_wait_context.cpp \
    $$PWD/libyb/shupito/escape_sequence.cpp \
    $$PWD/libyb/shupito/flip2.cpp \
    $$PWD/libyb/usb/detail/usb_request_context.cpp \
    $$PWD/libyb/usb/bulk_stream.cpp \
    $$PWD/libyb/usb/interface_guard.cpp \
    $$PWD/libyb/usb/usb_context.cpp \
    $$PWD/libyb/usb/usb_descriptors.cpp \
    $$PWD/libyb/usb/usb_device.cpp \
    $$PWD/libyb/utils/ihex_file.cpp \
    $$PWD/libyb/utils/sparse_buffer.cpp \
    $$PWD/libyb/utils/utf.cpp \
    $$PWD/libyb/utils/detail/win32_file_operation.cpp \
    $$PWD/libyb/utils/detail/win32_overlapped.cpp
