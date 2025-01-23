#pragma once

#include <QMainWindow>
#include <QDockWidget>

// To apply the style for `OBSDock QFrame`, define the class `OBSDock`.
class OBSDock : public QDockWidget {
	Q_OBJECT
public:
	OBSDock(QMainWindow *main);
};
