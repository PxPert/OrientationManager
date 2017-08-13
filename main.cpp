#include <QApplication>
#include "orientationmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setApplicationName("OrientationManager");
    QApplication::setOrganizationName("PxPert");

    new OrientationManager(NULL);

    return a.exec();
}
