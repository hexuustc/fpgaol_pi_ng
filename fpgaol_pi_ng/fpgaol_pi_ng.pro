
TARGET = fpgaol_pi_ng
TEMPLATE = app
QT = core network websockets serialport
CONFIG += console
INCLUDEPATH += include/

HEADERS += \
           include/gpio.h \
           include/httpserver.h \
           include/serial.h \
           include/util.h \
           include/wsserver.h

SOURCES += src/main.cpp \
           src/httpserver.cpp \
           src/wsserver.cpp


#---------------------------------------------------------------------------------------
# The following lines include the sources of the QtWebAppLib library
#---------------------------------------------------------------------------------------

include(../QtWebApp/httpserver/httpserver.pri)
# Not used: include(../QtWebApp/qtservice/qtservice.pri)
