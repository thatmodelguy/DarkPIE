#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFileInfo>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DarkPIE"));
    app.setOrganizationName(QStringLiteral("DarkPIE"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/darkpie.png")));
    applyDarkPieTheme(app);

    MainWindow w;
    w.setWindowIcon(QIcon(QStringLiteral(":/icons/darkpie.png")));
    w.show();
    if (argc > 1) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        if (QFileInfo::exists(path)) w.openPath(path);
    }
    return app.exec();
}
