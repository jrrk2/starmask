#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("XISF Test Creator");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("StellinaProcessor");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}