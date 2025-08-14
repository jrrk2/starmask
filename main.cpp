#include <QApplication>
#include "PCLMockAPI.h"
#include <pcl/api/APIInterface.h>
#include "MainWindow.h"

extern "C" {
#include "astrometry/log.h"
};

void loginit()
{
    log_init(LOG_MSG);
}

void logverbose()
{
    log_init(LOG_VERB);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    pcl_mock::InitializeMockAPI();
    API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
    
    w.show();
    return app.exec();
}
