TEMPLATE = app
CONFIG -= qt
CONFIG += c++14
CONFIG += strict_c++
CONFIG += exceptions_off
CONFIG += rtti_off

# FOR CLANG
#QMAKE_CXXFLAGS += -stdlib=libc++
#QMAKE_LFLAGS += -stdlib=libc++
QMAKE_CXXFLAGS += -fconstexpr-depth=256
#QMAKE_CXXFLAGS += -fconstexpr-steps=900000000

# debug flags
QMAKE_CXXFLAGS_DEBUG += -O0 -g3

# release flags
QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_CXXFLAGS_RELEASE += -fno-threadsafe-statics
QMAKE_CXXFLAGS_RELEASE += -fno-asynchronous-unwind-tables
#QMAKE_CXXFLAGS_RELEASE += -fstack-protector-all
QMAKE_CXXFLAGS_RELEASE += -fstack-protector-strong
QMAKE_CXXFLAGS_RELEASE += -fdata-sections
QMAKE_CXXFLAGS_RELEASE += -ffunction-sections
QMAKE_LFLAGS_RELEASE += -Wl,--gc-sections

# libraries
LIBS += -lrt

#DEFINES += DISABLE_INTERRUPTED_WRAPPER

#LIBS += -lpthread
experimental {
#QMAKE_CXXFLAGS += -stdlib=libc++
QMAKE_CXXFLAGS += -nostdinc
INCLUDEPATH += /usr/include/x86_64-linux-musl
INCLUDEPATH += /usr/include/c++/v1
INCLUDEPATH += /usr/include
INCLUDEPATH += /usr/include/x86_64-linux-gnu
QMAKE_LFLAGS += -L/usr/lib/x86_64-linux-musl -dynamic-linker /lib/ld-musl-x86_64.so.1
LIBS += -lc++
}

PUT = ../put
INCLUDEPATH += $$PUT

SOURCES = executor.cpp 

HEADERS += \
    $$PUT/cxxutils/posix_helpers.h \
    $$PUT/cxxutils/misc_helpers.h \
    $$PUT/cxxutils/socket_helpers.h \
    $$PUT/cxxutils/error_helpers.h \
    $$PUT/cxxutils/hashing.h \
    $$PUT/specialized/capabilities.h
