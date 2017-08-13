#ifndef ORIENTATIONENUMS_H
#define ORIENTATIONENUMS_H
#include <QObject>


class OrientationEnums : public QObject {
    Q_OBJECT
    public:
    enum OrientationPositions {
        ORIENTATION_INVALID = -1,
        ORIENTATION_FLAT = 0,
        ORIENTATION_TOP,
        ORIENTATION_BOTTOM,
        ORIENTATION_RIGHT,
        ORIENTATION_LEFT
    } ; /* various orientations */

    enum TabletModes {
        MODE_INVALID = -1,
        MODE_NOTEBOOK = 0,
        MODE_TABLET
    };
    Q_ENUM(OrientationPositions);
    Q_ENUM(TabletModes);
//    OrientationEnums( QObject *parent  = 0);

    // Do not forget to declare your class to the QML system.
    // static void declareQML();
};

#endif // ORIENTATIONENUMS_H
