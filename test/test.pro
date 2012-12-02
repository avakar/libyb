TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += main.cpp test.cpp memmock.cpp shupito_flash.cpp

include(../libyb.pri)
