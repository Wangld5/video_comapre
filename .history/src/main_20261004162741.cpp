#include <QApplication>
#include <QIcon>
#include "app/MainWindow.h"
#include "styles/FluentStyle.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Application metadata
    QApplication::setApplicationName("ImageCompare");
    QApplication::setApplicationVersion("1.7.14");
    QApplication::setOrganizationName("ImageCompare");

    // Application icon
    const QIcon applicationIcon(QStringLiteral(":/icons/play-media-sign-technology-icon.jpg"));
    app.setWindowIcon(applicationIcon);

    // Apply Fluent 2 design system
    FluentStyle::applyGlobalStyle(&app);

    MainWindow mainWindow;
    mainWindow.setWindowIcon(applicationIcon);
    mainWindow.show();

    return app.exec();
}
