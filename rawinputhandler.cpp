#include "rawinputhandler.h"
#include <linux/input.h>

#include <QSocketNotifier>
#include <fcntl.h>   // open
#include <unistd.h>  // daemon, close

#define KEY_RELEASE 0
#define KEY_PRESS 1
#define KEY_TABLETMODE 4

#define TABLETMODE_TABLET 204
#define TABLETMODE_NOTEBOOK 205

RawInputHandler::RawInputHandler(QObject *parent) : QObject(parent)
{
    m_sn = NULL;
    m_fp = -1;
    m_currentMode = OrientationEnums::MODE_INVALID;

}

RawInputHandler::~RawInputHandler()
{
    if (m_sn) {
        m_sn->setEnabled(false);
        delete(m_sn);
    }
    if (m_fp > -1) {
        close(m_fp);
    }
}

bool RawInputHandler::initialize(const QString &path, QString *toError)
{

    /* Attempt to open non blocking to access dev */
    QByteArray buffer_access = path.toUtf8();
    m_fp = open(buffer_access.constData(), O_RDONLY | O_NONBLOCK);

    if (m_fp == -1) { /* If it isn't there make the node */
        *toError = QString(tr("Failed to open %1 : %2 - %3")).arg(buffer_access.constData()).arg(strerror(errno)).arg(errno);
        return false;
    } else {
        m_sn= new QSocketNotifier(m_fp, QSocketNotifier::Read);
        m_sn->setEnabled(true);
        connect(m_sn, SIGNAL(activated(int)), this,SLOT(m_sn_activated(int)));
    }
    m_currentMode = OrientationEnums::MODE_NOTEBOOK;
    return true;
}


void RawInputHandler::m_sn_activated(int)
{
    input_event event;

    if (read(m_fp, &event, sizeof(input_event)) == sizeof(input_event)) {
        if (event.type == KEY_TABLETMODE) {
           if (event.value == TABLETMODE_TABLET) {
                 setCurrentMode(OrientationEnums::MODE_TABLET);
           } else if (event.value == TABLETMODE_NOTEBOOK) {
               setCurrentMode(OrientationEnums::MODE_NOTEBOOK);
           }
        }
    }

}

