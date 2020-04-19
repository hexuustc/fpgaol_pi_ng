#include <QCoreApplication>
#include <QDir>
#include <cstdlib>

#ifndef UTIL_H
#define UTIL_H

inline int programFPGA(QString filename)
{
    return system(("djtgcfg init -d Nexys4DDR && djtgcfg prog -d Nexys4DDR -i 0 -f " + filename).toStdString().data());
}

#endif // UTIL_H
