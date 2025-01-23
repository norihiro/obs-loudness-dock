#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include "dock-compat.hpp"

#include "plugin-macros.generated.h"

#define QT_UTF8(str) QString::fromUtf8(str, -1)

OBSDock::OBSDock(QMainWindow *main) : QDockWidget(main) {}

#if LIBOBS_API_VER <= MAKE_SEMANTIC_VERSION(29, 1, 3)
extern "C" bool obs_frontend_add_dock_by_id_compat(const char *id, const char *title, void *widget)
{
	auto *main = (QMainWindow *)obs_frontend_get_main_window();
	auto *dock = new OBSDock(main);

	dock->setWidget((QWidget *)widget);
	dock->setWindowTitle(QT_UTF8(title));
	dock->setObjectName(QT_UTF8(id));
	obs_frontend_add_dock(dock);
	dock->setFloating(true);
	dock->setVisible(false);

	return true;
}
#endif
