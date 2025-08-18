#pragma once
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>
#include "utils.hpp"

class ThresholdSpinDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	ThresholdSpinDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
	{
		ASSERT_THREAD(OBS_TASK_UI);

		QDoubleSpinBox *e = new QDoubleSpinBox(parent);
		e->setRange(-50.0, 0.0);
		e->setDecimals(1);
		return e;
	}

	void setEditorData(QWidget *editor, const QModelIndex &index) const override
	{
		ASSERT_THREAD(OBS_TASK_UI);

		double value = index.model()->data(index, Qt::EditRole).toDouble();
		QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(editor);
		if (spinBox)
			spinBox->setValue(value);
	}

	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
	{
		ASSERT_THREAD(OBS_TASK_UI);

		QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(editor);
		if (spinBox)
			model->setData(index, spinBox->value(), Qt::EditRole);
	}

	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
				  const QModelIndex &) const override
	{
		ASSERT_THREAD(OBS_TASK_UI);

		editor->setGeometry(option.rect);
	}
};
