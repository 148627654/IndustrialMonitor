#include "logindialog.h"
#include "ui_logindialog.h"
#include <QLineEdit>
#include <QSettings>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog),m_isPasswordHidden(false),m_toggleAction(nullptr)
{
    ui->setupUi(this);
    initUI();
    loadConfig();                   //导入ini文件
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::onTogglePasswordEcho()
{
    m_isPasswordHidden=!m_isPasswordHidden;
    ui->line_Password->setEchoMode(m_isPasswordHidden?QLineEdit::Normal:QLineEdit::Password);
    m_toggleAction->setIcon(m_isPasswordHidden?QIcon(":/icons/eye.png"):QIcon(":/icons/eye_close.png"));
}

void LoginDialog::onLoginClicked()
{
    QString username = ui->line_Username->text().trimmed();         //trimmed 移除首项尾项的所有的空白字符
    QString password = ui->line_Password->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->lab_ErrorHint->setText("⚠ 用户名或密码不可为空！");
        return;
    }
    if(username=="admin"&&password=="123456")
    {
        ui->lab_ErrorHint->setText("");
        saveConfig();
        accept(); // 核心：关闭弹窗并向 main.cpp 返回 Accepted
    }
    else
    {
        ui->lab_ErrorHint->setText("⚠ 账号或密码校验失败，请重试！");
        ui->line_Password->selectAll();
        ui->line_Password->setFocus();
    }
}

void LoginDialog::loadConfig() {
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    bool remember = settings.value("Login/RememberMe", false).toBool();
    ui->chkRemember->setChecked(remember);

    if (remember) {
        ui->line_Username->setText(settings.value("Login/Username").toString());
        // 简易反混淆：实际生产中建议结合简易 Base64 或 XOR 加密
        QByteArray passData = QByteArray::fromBase64(settings.value("Login/Password").toByteArray());
        ui->line_Password->setText(QString::fromUtf8(passData));
    }
}

void LoginDialog::saveConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    bool remember = ui->chkRemember->isChecked();
    settings.setValue("Login/RememberMe", remember);

    if(remember)
    {
        settings.setValue("Login/Username", ui->line_Username->text());
        // 密码明文转utf8字节，再base64编码保存
        QByteArray passBytes = ui->line_Password->text().toUtf8();
        settings.setValue("Login/Password", passBytes.toBase64());
    }
    else
    {
        // 不记住密码：清除旧账号密码
        settings.remove("Login/Username");
        settings.remove("Login/Password");
    }
    settings.sync(); // 写入磁盘，刷新文件
}

void LoginDialog::initUI()
{
    //工业标准
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(400, 320);
    setWindowTitle("系统身份认证");

    // 默认隐藏错误提示
    ui->lab_ErrorHint->setStyleSheet("color: #FF5252; font-weight: bold;");
    ui->lab_ErrorHint->setText("");

    //密码内嵌操作
    m_toggleAction=ui->line_Password->addAction(QIcon(":/icons/eye_close.png"),QLineEdit::TrailingPosition);           //尾部插入
    ui->line_Password->setEchoMode(QLineEdit::Password);
    connect(m_toggleAction, &QAction::triggered, this, &LoginDialog::onTogglePasswordEcho);

    connect(ui->btn_Login,&QPushButton::clicked,this,&LoginDialog::onLoginClicked);
    //用户名输入完成后,回车跳到密码输入
    connect(ui->line_Username,&QLineEdit::returnPressed,[this](){
        ui->line_Password->setFocus();
    });
    //密码输入完成后，回车自动登录
    connect(ui->line_Password,&QLineEdit::returnPressed,this,&LoginDialog::onLoginClicked);

}
