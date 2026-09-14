#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
// #include <QAbstractButton>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void initLayout();
    void initNavigation();

private:
    Ui::MainWindow *ui;
    QButtonGroup *m_navigationGroup;
    QMap<QAbstractButton*, QWidget*> m_pageMap;
};
#endif // MAINWINDOW_H
