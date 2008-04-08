#ifndef BODYPROPERTYEDITOR_H
#define BODYPROPERTYEDITOR_H

#include <QWidget>
#include "dynamiceditor.h"
#include "ui_bodypropertyeditor.h"

class BodyPropertyEditor : public QDialog
{
  Q_OBJECT

signals:
  void BodyMaterialComboChanged(BodyPropertyEditor *, QString);

public:
  BodyPropertyEditor(QWidget *parent = 0);
  ~BodyPropertyEditor();

  Ui::bodyPropertyDialog ui;
  DynamicEditor *material;

  bool touched;

public slots:
  void materialComboChanged(QString);

private slots:
  void applySlot();
  void discardSlot();

private:

};

#endif // BODYPROPERTYEDITOR_H
