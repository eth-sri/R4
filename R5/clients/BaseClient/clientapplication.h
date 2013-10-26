#ifndef CLIENTAPPLICATION_H
#define CLIENTAPPLICATION_H

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QString>

#include "toolwindow.h"

class ClientApplication : public QApplication {
    Q_OBJECT

public:
    ClientApplication(int& argc, char** argv);

protected:
    void loadWebsite(QString url);

private:
    void applyDefaultSettings();

protected:
    ToolWindow* m_window;
    QString m_programName;
};

#endif // CLIENTAPPLICATION_H
