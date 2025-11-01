#include "include/ui/setting/dialog_basic_settings.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/configs/proxy/Preset.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/DeviceDetailsHelper.hpp"

#include <QStyleFactory>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <qfontdatabase.h>
#include <QSettings>
#include <QString>

#include "include/ui/mainwindow.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#define STATE_CHANGED &QCheckBox::checkStateChanged
#else
#define STATE_CHANGED &QCheckBox::stateChanged
#endif
#include <QDir>
#define CONFIG_INI_PATH  QDir::current().absolutePath() + "/window.ini"

DialogBasicSettings::DialogBasicSettings(MainWindow *parent)
    : QDialog(parent), ui(new Ui::DialogBasicSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
    this->parent = parent;

    // Common
    ui->inbound_socks_port_l->setText(ui->inbound_socks_port_l->text().replace("Socks", "Mixed (SOCKS+HTTP)"));
    ui->log_level->addItems(QString("trace debug info warn error fatal panic").split(" "));
    ui->mux_protocol->addItems({"h2mux", "smux", "yamux"});
    ui->disable_stats->setChecked(Configs::dataStore->disable_traffic_stats);
    ui->proxy_scheme->setCurrentText(Configs::dataStore->proxy_scheme);

    D_LOAD_STRING(inbound_address)
    D_LOAD_COMBO_STRING(log_level)
    CACHE.custom_inbound = Configs::dataStore->custom_inbound;
    D_LOAD_INT(inbound_socks_port)
    ui->random_listen_port->setChecked(Configs::dataStore->random_inbound_port);
    D_LOAD_INT(test_concurrent)
    D_LOAD_STRING(test_latency_url)
    D_LOAD_BOOL(disable_tray)
    ui->url_timeout->setText(Int2String(Configs::dataStore->url_test_timeout_ms));
    ui->speedtest_mode->setCurrentIndex(Configs::dataStore->speed_test_mode);
    ui->test_timeout->setText(Int2String(Configs::dataStore->speed_test_timeout_ms));
    ui->simple_down_url->setText(Configs::dataStore->simple_dl_url);
    ui->allow_beta->setChecked(Configs::dataStore->allow_beta_update);

    connect(ui->custom_inbound_edit, &QPushButton::clicked, this, [=,this] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_inbound)
    });
    connect(ui->disable_tray, STATE_CHANGED, this, [=,this](const bool &) {
        CACHE.updateDisableTray = true;
    });
    connect(ui->random_listen_port, STATE_CHANGED, this, [=,this](const bool &state)
    {
        if (state)
        {
            ui->inbound_socks_port->setDisabled(true);
        } else
        {
            ui->inbound_socks_port->setDisabled(false);
        }
    });

#ifndef Q_OS_WIN
    ui->proxy_scheme_l->hide();
    ui->proxy_scheme->hide();
    ui->windows_no_admin->hide();
#endif

    // Style
    ui->connection_statistics->setChecked(Configs::dataStore->enable_stats);
    ui->show_sys_dns->setChecked(Configs::dataStore->show_system_dns);
    connect(ui->show_sys_dns, STATE_CHANGED, this, [=,this]
    {
        CACHE.updateSystemDns = true;
    });
#ifndef Q_OS_WIN
    ui->show_sys_dns->hide();
#endif
    //
    D_LOAD_BOOL(start_minimal)
    D_LOAD_INT(max_log_line)
    //
    ui->language->setCurrentIndex(Configs::dataStore->language);
    connect(ui->language, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        CACHE.needRestart = true;
    });
    connect(ui->font, &QComboBox::currentTextChanged, this, [=,this](const QString &fontName) {
        auto font = qApp->font();
        font.setFamily(fontName);
        qApp->setFont(font);
        Configs::dataStore->font = fontName;
        Configs::dataStore->Save();
        adjustSize();
    });
    for (int i=7;i<=26;i++) {
        ui->font_size->addItem(Int2String(i));
    }
    ui->font_size->setCurrentText(Int2String(qApp->font().pointSize()));
    connect(ui->font_size, &QComboBox::currentTextChanged, this, [=,this](const QString &sizeStr) {
        auto font = qApp->font();
        font.setPointSize(sizeStr.toInt());
        qApp->setFont(font);
        Configs::dataStore->font_size = sizeStr.toInt();
        Configs::dataStore->Save();
        adjustSize();
    });
    //
    ui->theme->addItems(QStyleFactory::keys());
    ui->theme->addItem("QDarkStyle");
    //
    bool ok;
    auto themeId = Configs::dataStore->theme.toInt(&ok);
    if (ok) {
        ui->theme->setCurrentIndex(themeId);
    } else {
        ui->theme->setCurrentText(Configs::dataStore->theme);
    }
    //
    connect(ui->theme, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=,this](int index) {
        themeManager->ApplyTheme(ui->theme->currentText());
        Configs::dataStore->theme = ui->theme->currentText();
        Configs::dataStore->Save();
    });

    // Subscription

    ui->user_agent->setText(Configs::dataStore->user_agent);
    ui->user_agent->setPlaceholderText(Configs::dataStore->GetUserAgent(true));
    D_LOAD_BOOL(net_use_proxy)
    D_LOAD_BOOL(sub_clear)
    D_LOAD_BOOL(net_insecure)
    D_LOAD_BOOL(sub_send_hwid)
    D_LOAD_INT_ENABLE(sub_auto_update, sub_auto_update_enable)
    auto details = GetDeviceDetails();
	ui->sub_send_hwid->setToolTip(
        ui->sub_send_hwid->toolTip()
            .arg(details.hwid.isEmpty() ? "N/A" : details.hwid,
                details.os.isEmpty() ? "N/A" : details.os,
                details.osVersion.isEmpty() ? "N/A" : details.osVersion,
                details.model.isEmpty() ? "N/A" : details.model));

    // Core
    ui->groupBox_core->setTitle(software_core_name);

    // Mux
    D_LOAD_INT(mux_concurrency)
    D_LOAD_COMBO_STRING(mux_protocol)
    D_LOAD_BOOL(mux_padding)
    D_LOAD_BOOL(mux_default_on)

    // NTP
    ui->ntp_enable->setChecked(Configs::dataStore->enable_ntp);
    ui->ntp_server->setEnabled(Configs::dataStore->enable_ntp);
    ui->ntp_port->setEnabled(Configs::dataStore->enable_ntp);
    ui->ntp_interval->setEnabled(Configs::dataStore->enable_ntp);
    ui->ntp_server->setText(Configs::dataStore->ntp_server_address);
    ui->ntp_port->setText(Int2String(Configs::dataStore->ntp_server_port));
    ui->ntp_interval->setCurrentText(Configs::dataStore->ntp_interval);
    connect(ui->ntp_enable, STATE_CHANGED, this, [=,this](const bool &state) {
        ui->ntp_server->setEnabled(state);
        ui->ntp_port->setEnabled(state);
        ui->ntp_interval->setEnabled(state);
    });

    // Security

    ui->utlsFingerprint->addItems(Preset::SingBox::UtlsFingerPrint);
    ui->disable_priv_req->setChecked(Configs::dataStore->disable_privilege_req);
    ui->windows_no_admin->setChecked(Configs::dataStore->disable_run_admin);
    ui->mozilla_cert->setChecked(Configs::dataStore->use_mozilla_certs);

    D_LOAD_BOOL(skip_cert)
    ui->utlsFingerprint->setCurrentText(Configs::dataStore->utlsFingerprint);

    // Startup
    connect(ui->select_core, &QPushButton::clicked, this, [=,this]{
        QString fileName = QFileDialog::getOpenFileName(this, QObject::tr("Select"), QDir::currentPath(),
                                                        "", nullptr, QFileDialog::Option::ReadOnly);
        ui->core_path->setText(fileName);
    });
    connect(ui->select_icons, &QPushButton::clicked, this, [=,this]{
        QString folderName = QFileDialog::getExistingDirectory(this, QObject::tr("Select a Folder"), "");
        ui->icons_path->setText(folderName);
    });

    connect(this, &DialogBasicSettings::size_changed, parent, &MainWindow::size_changed);
    connect(this, &DialogBasicSettings::point_changed, parent, &MainWindow::point_changed);
    connect(ui->apply_now, &QPushButton::clicked, this, [=,this]{
        emit size_changed(ui->window_width->text().toInt(), ui->window_height->text().toInt());
        emit point_changed(ui->window_X->text().toInt(), ui->window_Y->text().toInt());
    });

    QSettings settings(CONFIG_INI_PATH, QSettings::IniFormat);
    auto validator = new QIntValidator(0, 0xfffffff, this);
    ui->save_geometry->setChecked(settings.value("save_geometry", true).toBool());
    ui->window_width->setText(QString::number(settings.value("width", 0).toInt()));
    ui->window_height->setText(QString::number(settings.value("height", 0).toInt()));
    ui->window_X->setText(QString::number(settings.value("X", 0).toInt()));
    ui->window_Y->setText(QString::number(settings.value("Y", 0).toInt()));
    ui->window_width->setValidator(validator);
    ui->window_height->setValidator(validator);
    ui->window_X->setValidator(validator);
    ui->window_Y->setValidator(validator);

    QString core_path = settings.value("core_path", "").toString();
    QString icons_path = settings.value("icons_path", "").toString();
    ui->core_path->setText(core_path);
    ui->icons_path->setText(icons_path);
    connect(ui->default_core_path, STATE_CHANGED, this, [=, this](int state){
        bool disabled = (state == Qt::Checked);
        ui->core_path->setDisabled(disabled);
        ui->select_core->setDisabled(disabled);
    });
    connect(ui->default_icons_path, STATE_CHANGED, this, [=, this](int state){
        bool disabled = (state == Qt::Checked);
        ui->icons_path->setDisabled(disabled);
        ui->select_icons->setDisabled(disabled);
    });
    ui->default_core_path->setChecked(core_path == "");
    ui->default_icons_path->setChecked(icons_path == "");
}

DialogBasicSettings::~DialogBasicSettings() {
    delete ui;
}

void DialogBasicSettings::accept() {
    QSettings settings(CONFIG_INI_PATH, QSettings::IniFormat);
    // Common
    bool needChoosePort = false;

    D_SAVE_STRING(inbound_address)
    D_SAVE_COMBO_STRING(log_level)
    Configs::dataStore->custom_inbound = CACHE.custom_inbound;
    D_SAVE_INT(inbound_socks_port)
    if (!Configs::dataStore->random_inbound_port && ui->random_listen_port->isChecked())
    {
        needChoosePort = true;
    }
    Configs::dataStore->random_inbound_port = ui->random_listen_port->isChecked();
    D_SAVE_INT(test_concurrent)
    D_SAVE_STRING(test_latency_url)
    D_SAVE_BOOL(disable_tray)
    Configs::dataStore->proxy_scheme = ui->proxy_scheme->currentText().toLower();
    Configs::dataStore->speed_test_mode = ui->speedtest_mode->currentIndex();
    Configs::dataStore->simple_dl_url = ui->simple_down_url->text();
    Configs::dataStore->url_test_timeout_ms = ui->url_timeout->text().toInt();
    Configs::dataStore->speed_test_timeout_ms = ui->test_timeout->text().toInt();
    Configs::dataStore->allow_beta_update = ui->allow_beta->isChecked();

    // Style

    Configs::dataStore->enable_stats = ui->connection_statistics->isChecked();
    Configs::dataStore->language = ui->language->currentIndex();
    D_SAVE_BOOL(start_minimal)
    D_SAVE_INT(max_log_line)
    Configs::dataStore->show_system_dns = ui->show_sys_dns->isChecked();

    if (Configs::dataStore->max_log_line <= 0) {
        Configs::dataStore->max_log_line = 200;
    }

    // Subscription

    if (ui->sub_auto_update_enable->isChecked()) {
        TM_auto_update_subsctiption_Reset_Minute(ui->sub_auto_update->text().toInt());
    } else {
        TM_auto_update_subsctiption_Reset_Minute(0);
    }

    Configs::dataStore->user_agent = ui->user_agent->text();
    D_SAVE_BOOL(net_use_proxy)
    D_SAVE_BOOL(sub_clear)
    D_SAVE_BOOL(net_insecure)
    D_SAVE_BOOL(sub_send_hwid)
    D_SAVE_INT_ENABLE(sub_auto_update, sub_auto_update_enable)

    // Core
    Configs::dataStore->disable_traffic_stats = ui->disable_stats->isChecked();

    // Mux
    D_SAVE_INT(mux_concurrency)
    D_SAVE_COMBO_STRING(mux_protocol)
    D_SAVE_BOOL(mux_padding)
    D_SAVE_BOOL(mux_default_on)

    // NTP
    Configs::dataStore->enable_ntp = ui->ntp_enable->isChecked();
    Configs::dataStore->ntp_server_address = ui->ntp_server->text();
    Configs::dataStore->ntp_server_port = ui->ntp_port->text().toInt();
    Configs::dataStore->ntp_interval = ui->ntp_interval->currentText();

    // Security

    D_SAVE_BOOL(skip_cert)
    Configs::dataStore->utlsFingerprint = ui->utlsFingerprint->currentText();
    Configs::dataStore->disable_privilege_req = ui->disable_priv_req->isChecked();
    Configs::dataStore->disable_run_admin = ui->windows_no_admin->isChecked();
    Configs::dataStore->use_mozilla_certs = ui->mozilla_cert->isChecked();

    QStringList str{"UpdateDataStore"};
    if (CACHE.needRestart) str << "NeedRestart";
    if (CACHE.updateDisableTray) str << "UpdateDisableTray";
    if (CACHE.updateSystemDns) str << "UpdateSystemDns";
    if (needChoosePort) str << "NeedChoosePort";
    MW_dialog_message(Dialog_DialogBasicSettings, str.join(","));
    QDialog::accept();

    int width, height, X, Y;
    // Startup
    settings.setValue("save_geometry", ui->save_geometry->isChecked());
    settings.setValue("width", width = ui->window_width->text().toInt());
    settings.setValue("height", height = ui->window_height->text().toInt());
    settings.setValue("X", X = ui->window_X->text().toInt());
    settings.setValue("Y", Y = ui->window_Y->text().toInt());
    if (ui->default_core_path->isChecked()){
        settings.remove("core_path");
    } else {
        settings.setValue("core_path", ui->core_path->text());
    }
    if (ui->default_icons_path->isChecked()){
        settings.remove("icons_path");
    } else {
        settings.setValue("icons_path", ui->icons_path->text());
    }
    settings.sync();
}

void DialogBasicSettings::on_core_settings_clicked() {
    auto w = new QDialog(this);
    w->setWindowTitle(software_core_name + " Core Options");
    auto layout = new QGridLayout;
    w->setLayout(layout);
    //
    auto line = -1;
    MyLineEdit *core_box_clash_api;
    MyLineEdit *core_box_clash_api_secret;
    MyLineEdit *core_box_clash_listen_addr;
    //
    auto core_box_clash_listen_addr_l = new QLabel("Clash Api Listen Address");
    core_box_clash_listen_addr = new MyLineEdit;
    core_box_clash_listen_addr->setText(Configs::dataStore->core_box_clash_listen_addr);
    layout->addWidget(core_box_clash_listen_addr_l, ++line, 0);
    layout->addWidget(core_box_clash_listen_addr, line, 1);
    //
    auto core_box_clash_api_l = new QLabel("Clash API Listen Port");
    core_box_clash_api = new MyLineEdit;
    core_box_clash_api->setText(Configs::dataStore->core_box_clash_api > 0 ? Int2String(Configs::dataStore->core_box_clash_api) : "");
    layout->addWidget(core_box_clash_api_l, ++line, 0);
    layout->addWidget(core_box_clash_api, line, 1);
    //
    auto core_box_clash_api_secret_l = new QLabel("Clash API Secret");
    core_box_clash_api_secret = new MyLineEdit;
    core_box_clash_api_secret->setText(Configs::dataStore->core_box_clash_api_secret);
    layout->addWidget(core_box_clash_api_secret_l, ++line, 0);
    layout->addWidget(core_box_clash_api_secret, line, 1);
    //
    auto box = new QDialogButtonBox;
    box->setOrientation(Qt::Horizontal);
    box->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    connect(box, &QDialogButtonBox::accepted, w, [=,this] {
        Configs::dataStore->core_box_clash_api = core_box_clash_api->text().toInt();
        Configs::dataStore->core_box_clash_listen_addr = core_box_clash_listen_addr->text();
        Configs::dataStore->core_box_clash_api_secret = core_box_clash_api_secret->text();
        MW_dialog_message(Dialog_DialogBasicSettings, "UpdateDataStore");
        w->accept();
    });
    connect(box, &QDialogButtonBox::rejected, w, &QDialog::reject);
    layout->addWidget(box, ++line, 1);
    //
    ADD_ASTERISK(w)
    w->exec();
    w->deleteLater();
}
