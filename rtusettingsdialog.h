#ifndef RTUSETTINGSDIALOG_H
#define RTUSETTINGSDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include "discover.h"

class RtuSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RtuSettingsDialog(QWidget *parent = nullptr);

private slots:
    void addProfileRow(const discover::ModbusRtuSettings &cfg = discover::ModbusRtuSettings());
    void removeSelectedRow();
    void saveAndAccept();

private:
    QTableWidget *m_table = nullptr;
};

#endif // RTUSETTINGSDIALOG_H
