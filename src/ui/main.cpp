// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include <QApplication>
#include <QTimer>
int main(int argc,char**argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("Chosuta");
    QCoreApplication::setApplicationName("Chosuta");
    QCoreApplication::setApplicationVersion(CHOSUTA_VERSION);
    QTemporaryDir settings;
    if(app.arguments().contains("--smoke-test"))QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    chosuta::Window window;
    window.show();
    auto args=app.arguments();
    if(args.contains("--smoke-test")) {
        QTimer::singleShot(100,&app,[&] {
            QTemporaryDir temp;
            bool success=window.runSmokeWorkflow(temp.path());
            int index=args.indexOf("--screenshot");
            QTimer::singleShot(100,&app,[&,success,index] {
                if(index>=0&&index+1<args.size())window.grab().save(args[index+1]);
                app.exit(success?0:1);
            });
        });
    }
    else if(args.size()>1)window.openPath(args[1]);
    return app.exec();
}
