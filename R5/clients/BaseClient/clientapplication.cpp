
#include "clientapplication.h"

ClientApplication::ClientApplication(int& argc, char** argv)
    : QApplication(argc, argv, QApplication::GuiServer)
    , m_programName("record")
{
    applyDefaultSettings();

    QStringList args = arguments();
    QFileInfo program(args.at(0));

    if (program.exists())
        m_programName = program.baseName();

    m_window = new ToolWindow();
}

void ClientApplication::loadWebsite(QString url)
{
    m_window->load(url);
}

void ClientApplication::applyDefaultSettings()
{
    QWebSettings::setMaximumPagesInCache(4);
    QWebSettings::setObjectCacheCapacities((16*1024*1024) / 8, (16*1024*1024) / 8, 16*1024*1024);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
}
