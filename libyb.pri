INCLUDEPATH += $$PWD
SOURCES += \
    $$PWD/libyb/descriptor.cpp \
    $$PWD/libyb/stream_parser.cpp \
    $$PWD/libyb/tunnel.cpp \
    $$PWD/libyb/async/cancellation_token.cpp \
    $$PWD/libyb/async/descriptor_reader.cpp \
    $$PWD/libyb/async/device.cpp \
    $$PWD/libyb/async/mock_stream.cpp \
    $$PWD/libyb/async/null_stream.cpp \
    $$PWD/libyb/async/stream.cpp \
    $$PWD/libyb/async/stream_device.cpp \
    $$PWD/libyb/async/timer.cpp \
    $$PWD/libyb/async/detail/parallel_composition_task.cpp \
    $$PWD/libyb/async/detail/task_impl.cpp \
    $$PWD/libyb/shupito/escape_sequence.cpp \
    $$PWD/libyb/shupito/flip2.cpp \
    $$PWD/libyb/usb/bulk_stream.cpp \
    $$PWD/libyb/usb/interface_guard.cpp \
    $$PWD/libyb/usb/usb_descriptors.cpp \
    $$PWD/libyb/usb/usb_device.cpp \
    $$PWD/libyb/utils/ihex_file.cpp \
    $$PWD/libyb/utils/sparse_buffer.cpp \
    $$PWD/libyb/utils/svf_file.cpp \
    $$PWD/libyb/utils/utf.cpp

win32 {
    *g++* {
        QMAKE_CXXFLAGS += -std=c++0x -Wno-missing-field-initializers -include cmath
    }

    SOURCES += \
        $$PWD/libyb/async/detail/win32_affinity_task.cpp \
        $$PWD/libyb/async/detail/win32_async_channel.cpp \
        $$PWD/libyb/async/detail/win32_async_runner.cpp \
        $$PWD/libyb/async/detail/win32_handle_task.cpp \
        $$PWD/libyb/async/detail/win32_serial_port.cpp \
        $$PWD/libyb/async/detail/win32_sync_runner.cpp \
        $$PWD/libyb/async/detail/win32_timer.cpp \
        $$PWD/libyb/async/detail/win32_wait_context.cpp \
        $$PWD/libyb/usb/detail/usb_request_context.cpp \
        $$PWD/libyb/usb/detail/win32_usb_context.cpp \
        $$PWD/libyb/usb/detail/win32_usb_device.cpp \
        $$PWD/libyb/utils/detail/win32_file_operation.cpp \
        $$PWD/libyb/utils/detail/win32_overlapped.cpp
}

unix:!macx:!symbian {
    QMAKE_CXXFLAGS += -std=c++0x -Wno-missing-field-initializers
    SOURCES += \
        $$PWD/libyb/async/detail/linux_async_channel.cpp \
        $$PWD/libyb/async/detail/linux_async_runner.cpp \
        $$PWD/libyb/async/detail/linux_serial_port.cpp \
        $$PWD/libyb/async/detail/linux_sync_runner.cpp \
        $$PWD/libyb/async/detail/linux_timer.cpp \
        $$PWD/libyb/async/detail/linux_wait_context.cpp \
        $$PWD/libyb/usb/detail/linux_usb_context.cpp \
        $$PWD/libyb/usb/detail/linux_usb_device.cpp \
        $$PWD/libyb/utils/detail/pthread_mutex.cpp
    LIBS += -ludev
}
