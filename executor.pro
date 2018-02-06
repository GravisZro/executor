TEMPLATE = app
CONFIG -= qt
CONFIG += c++14

# FOR CLANG
#QMAKE_CXXFLAGS += -stdlib=libc++
#QMAKE_LFLAGS += -stdlib=libc++

# universal arguments
QMAKE_CXXFLAGS += -std=c++14
QMAKE_CXXFLAGS += -fconstexpr-depth=256
#QMAKE_CXXFLAGS += -fconstexpr-steps=900000000
QMAKE_CXXFLAGS += -pipe -Os -fno-exceptions -fno-rtti -fno-threadsafe-statics
#QMAKE_CXXFLAGS += -pipe -Os
#QMAKE_CXXFLAGS += -fno-exceptions
#QMAKE_CXXFLAGS += -fno-rtti
#QMAKE_CXXFLAGS += -fno-threadsafe-statics
DEFINES += INTERRUPTED_WRAPPER

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

PDTK = ../pdtk
INCLUDEPATH += $$PDTK

SOURCES = executor.cpp 

HEADERS +=  \
    $$PDTK/cxxutils/posix_helpers.h \
    $$PDTK/cxxutils/misc_helpers.h \
    $$PDTK/cxxutils/socket_helpers.h \
    $$PDTK/cxxutils/error_helpers.h \
    $$PDTK/cxxutils/hashing.h \
    $$PDTK/specialized/capabilities.h
