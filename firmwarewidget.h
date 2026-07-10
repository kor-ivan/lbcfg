#ifndef FIRMWAREWIDGET_H
#define FIRMWAREWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QPushButton>

class FirmwareWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FirmwareWidget(QWidget *parent = nullptr);
    virtual ~FirmwareWidget();

    void showStatus();
    void setProgress(int value);
    void resetAndHide();

signals:
    void stopButtonPressed();

private:
    QProgressBar *progressBar = nullptr;
    QPushButton *stopButton = nullptr;
};

#endif // FIRMWAREWIDGET_H
