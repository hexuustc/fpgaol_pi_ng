
TARGET = fpgaol_pi_ng
TEMPLATE = app
QT = core network websockets serialport
CONFIG += console
INCLUDEPATH += include/

HEADERS += \
           include/global.h \
           include/gpio.h \
           include/requestmapper.h \
           include/controller/fileuploadcontroller.h \
           include/serial.h \
           include/util.h

SOURCES += src/main.cpp \
           src/global.cpp \
           src/requestmapper.cpp \
           src/controller/fileuploadcontroller.cpp

OTHER_FILES += etc/* etc/docroot/* etc/templates/* etc/ssl/* logs/* ../readme.txt

#---------------------------------------------------------------------------------------
# The following lines include the sources of the QtWebAppLib library
#---------------------------------------------------------------------------------------

include(../QtWebApp/logging/logging.pri)
include(../QtWebApp/httpserver/httpserver.pri)
# Not used: include(../QtWebApp/qtservice/qtservice.pri)
