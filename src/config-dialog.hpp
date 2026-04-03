#pragma once

#include <QDialog>
#include <QColor>
#include "config.hpp"

class ConfigDialog : public QDialog {
	Q_OBJECT

public:
	ConfigDialog(const loudness_dock_config_s &cfg, QWidget *parent = nullptr);

	const loudness_dock_config_s &getConfig() const { return config; }

	void reject() override;

signals:
	void changed();

public slots:
	void setTabTableCell(int row, int column);
	void setColorTableCell(int row, int column);

private:
	void on_abbrev_label_changed(bool checked);
	void on_tab_table_changed(int row, int column);
	void on_tab_table_add();
	void on_tab_table_remove();
	void on_color_table_changed(int row, int column);
	void on_color_table_add();
	void on_color_table_remove();

	void TabTableAdd(int ix, const struct loudness_dock_config_s::tab_config &tab);
	void ColorTableAdd(int ix, float threshold, uint32_t color_fg, uint32_t color_bg);

private:
	class QCheckBox *abbrevLabelCheck;
	class QTableWidget *tabTable;
	class QTableWidget *colorTable;

	loudness_dock_config_s config;
	loudness_dock_config_s config_orig;
};
