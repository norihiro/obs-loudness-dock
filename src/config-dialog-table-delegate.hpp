#pragma once
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>

class ThresholdSpinDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	ThresholdSpinDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
	{
		QDoubleSpinBox *e = new QDoubleSpinBox(parent);
		e->setRange(-50.0, 0.0);
		e->setDecimals(1);
		return e;
	}

	void setEditorData(QWidget *editor, const QModelIndex &index) const override
	{
		double value = index.model()->data(index, Qt::EditRole).toDouble();
		QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(editor);
		if (spinBox)
			spinBox->setValue(value);
	}

	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
	{
		QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(editor);
		if (spinBox)
			model->setData(index, spinBox->value(), Qt::EditRole);
	}

	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
				  const QModelIndex &) const override
	{
		editor->setGeometry(option.rect);
	}
};
