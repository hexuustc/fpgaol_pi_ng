
TARGET = fpgaol_pi_ng
TEMPLATE = app
QT = core network websockets serialport
CONFIG += console debug
INCLUDEPATH += include/

LIBS += -lpigpio

HEADERS += \
           include/httpserver.h \
           include/io_defs_hal.h \
           include/fpga.h \
           include/wsserver.h \
           include/periph.h \
           include/peripherals.h

SOURCES += src/main.cpp \
           src/io_defs_hal.cpp \
           src/httpserver.cpp \
           src/fpga.cpp \
           src/wsserver.cpp \
           src/periph.cpp \
           src/peripherals.cpp


#---------------------------------------------------------------------------------------
# The following lines include the sources of the QtWebAppLib library
#---------------------------------------------------------------------------------------

include(../QtWebApp/httpserver/httpserver.pri)
# Not used: include(../QtWebApp/qtservice/qtservice.pri)
