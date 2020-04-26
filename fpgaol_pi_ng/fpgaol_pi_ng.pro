
TARGET = fpgaol_pi_ng
TEMPLATE = app
QT = core network websockets serialport
CONFIG += console
INCLUDEPATH += include/

LIBS += -lpigpio

HEADERS += \
           include/httpserver.h \
           include/gpio_defs.h \
           include/fpga.h \
           include/wsserver.h

SOURCES += src/main.cpp \
           src/httpserver.cpp \
           src/fpga.cpp \
           src/wsserver.cpp


#---------------------------------------------------------------------------------------
# The following lines include the sources of the QtWebAppLib library
#---------------------------------------------------------------------------------------

include(../QtWebApp/httpserver/httpserver.pri)
# Not used: include(../QtWebApp/qtservice/qtservice.pri)
