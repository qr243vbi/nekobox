#include "nekobox/ui/setting/dialog_basic_settings.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "nekobox/configs/proxy/Preset.hpp"
#include "nekobox/ui/setting/ThemeManager.hpp"
#include "nekobox/ui/setting/Icon.hpp"
#include "nekobox/global/GuiUtils.hpp"
#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/global/HTTPRequestHelper.hpp"
#include "nekobox/global/DeviceDetailsHelper.hpp"
#include "nekobox/dataStore/ResourceEntity.hpp"

#include <QStyleFactory>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <qfontdatabase.h>
#include "nekobox/sys/Settings.h"
#include "nekobox/dataStore/ResourceEntity.hpp"
#include <QString>

#include "nekobox/ui/mainwindow_interface.h"



QList<QString> locales = {
    "",
    "C",
    "he_IL",
    "zh_CN",
    "fa_IR",
    "ru_RU"
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#define STATE_CHANGED &QCheckBox::checkStateChanged
#else
#define STATE_CHANGED &QCheckBox::stateChanged
#endif
#include <QDir>

#define settings Configs::windowSettings

DialogBasicSettings::DialogBasicSettings(MainWindow *parent)
    : QDialog(parent), ui(new Ui::DialogBasicSettings) {
    CHECK_SETTINGS_ACCESS
    ui->setupUi(this);
    ADD_ASTERISK(this);
    this->parent = parent;

    // Auto-testing
    D_LOAD_BOOL(auto_test_enable)
    ui->auto_test_interval_seconds->setValue(Configs::dataStore->auto_test_interval_seconds);
    ui->auto_test_proxy_count->setValue(Configs::dataStore->auto_test_proxy_count);
    ui->auto_test_working_pool_size->setValue(Configs::dataStore->auto_test_working_pool_size);
    ui->auto_test_latency_threshold_ms->setValue(Configs::dataStore->auto_test_latency_threshold_ms);
    ui->auto_test_failure_retry_count->setValue(Configs::dataStore->auto_test_failure_retry_count);
    D_LOAD_STRING(auto_test_target_url)
    D_LOAD_BOOL(auto_test_tun_failover)

    // Common
    ui->inbound_socks_port_l->setText(ui->inbound_socks_port_l->text().replace("Socks", "Mixed (SOCKS+HTTP)"));
    ui->log_level->addItems(QString("trace debug info warn error fatal panic").split(" "));
    ui->mux_protocol->addItems({"h2mux", "smux", "yamux"});
    ui->disable_stats->setChecked(Configs::dataStore->disable_traffic_stats);
    ui->proxy_scheme->setCurrentText(Configs::dataStore->proxy_scheme);

    #define UPDATE_ICON CACHE.updateIcon = true
    #define UPDATE_FONT {                   \
        CACHE.updateFont = true;            \
        updateEmojiFont();                  \
        CACHE.needRestart = true;           \
        adjustSize();                       \
    }                   
    
    LINK_RESOURCE_MANAGER("icon.png", icon, UPDATE_ICON);
    LINK_RESOURCE_MANAGER("emoji.ttf", emoji, UPDATE_FONT);

    D_LOAD_STRING(inbound_address)
    D_LOAD_COMBO_STRING(log_level)
    CACHE.custom_inbound = Configs::dataStore->custom_inbound;
    D_LOAD_INT(inbound_socks_port)
    D_LOAD_BOOL(random_inbound_port);
    D_LOAD_INT(test_concurrent)
    D_LOAD_STRING(test_latency_url)
    D_LOAD_BOOL(disable_tray)
    S_LOAD_BOOL(auto_hide)
    ui->set_text_under_menu_icons->setChecked(settings->text_under_buttons);
    connect(ui->set_text_under_menu_icons, STATE_CHANGED, this, [=,this]
    {
        CACHE.updateMenuIcon = true;
    });
    D_LOAD_INT(url_test_timeout_ms)
    ui->speedtest_mode->setCurrentIndex(Configs::dataStore->speed_test_mode);
    D_LOAD_INT(speed_test_timeout_ms);
    D_LOAD_STRING(simple_dl_url)
    ui->force_json_configs->setChecked(Configs::ForceJsonConfigs);

    connect(ui->custom_inbound_edit, &QPushButton::clicked, this, [=,this] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_inbound)
    });
    connect(ui->disable_tray, STATE_CHANGED, this, [=,this](const bool &) {
        CACHE.updateDisableTray = true;
    });
    connect(ui->random_inbound_port, STATE_CHANGED, this, [=,this](const bool &state)
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
    D_LOAD_BOOL(connection_statistics)

#ifndef Q_OS_WIN
    ui->show_system_dns->hide();
#else
    D_LOAD_BOOL(show_system_dns)
    connect(ui->show_system_dns, STATE_CHANGED, this, [=,this]
    {
        CACHE.updateSystemDns = true;
    });
#endif
    //
    D_LOAD_BOOL(start_minimal)
    S_LOAD_INT(max_log_line)
    //
    auto language = ui->language;
    language->setCurrentIndex(locales.indexOf(getLocale()));
    connect(language, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        CACHE.needRestart = true;
    });
    connect(ui->font, &QComboBox::currentTextChanged, this, [=,this](const QString &fontName) {
        auto font = qApp->font();
        font.setFamily(fontName);
        qApp->setFont(font);
        settings->font_family =  fontName;
        settings->Save();
        adjustSize();
    });
    for (int i=7;i<=26;i++) {
        ui->font_size->addItem(QString::number(i));
    }
    ui->font_size->setCurrentText(QString::number(qApp->font().pointSize()));
    connect(ui->font_size, &QComboBox::currentTextChanged, this, [=,this](const QString &sizeStr) {
        auto font = qApp->font();
        int font_size = sizeStr.toInt();
        font.setPointSize(font_size);
        qApp->setFont(font);
        settings->font_size = font_size;
        settings->Save();
        adjustSize();
    });
    //
    ui->theme->addItems(QStyleFactory::keys());
    ui->theme->addItem("QDarkStyle");
    //
//    bool ok;
    ui->theme->setCurrentText(settings->theme);
    //
    connect(ui->theme, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=,this](int index) {
        QString ui_theme_text = ui->theme->currentText();
        themeManager->ApplyTheme(ui_theme_text);
        settings->theme = ui_theme_text;
        settings->Save();
    });

    // Subscription
    connect(ui->sub_url_test, STATE_CHANGED, this, [=,this](const bool &c) {
        ui->sub_rm_unavailable->setEnabled(c);
    });
    D_LOAD_STRING(user_agent)
    ui->user_agent->setPlaceholderText(Configs::dataStore->GetUserAgent(true));
    D_LOAD_BOOL(net_use_proxy);
    D_LOAD_BOOL(sub_clear)
    D_LOAD_BOOL(net_insecure)
    D_LOAD_BOOL(sub_send_hwid)
    D_LOAD_STRING(sub_custom_hwid_params)
    D_LOAD_BOOL(sub_rm_invalid)
    D_LOAD_BOOL(sub_url_test)
    D_LOAD_BOOL(sub_rm_duplicates)
    D_LOAD_BOOL(sub_rm_unavailable)
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
    ui->ntp_port->setText(QString::number(Configs::dataStore->ntp_server_port));
    ui->ntp_interval->setCurrentText(Configs::dataStore->ntp_interval);
    connect(ui->ntp_enable, STATE_CHANGED, this, [=,this](const bool &state) {
        ui->ntp_server->setEnabled(state);
        ui->ntp_port->setEnabled(state);
        ui->ntp_interval->setEnabled(state);
    });

    // Security

    ui->utlsFingerprint->addItems(Preset::SingBox::UtlsFingerPrint);
    D_LOAD_BOOL(disable_privilege_req)
    D_LOAD_BOOL(windows_no_admin)
    D_LOAD_BOOL(use_mozilla_certs)

    D_LOAD_BOOL(skip_cert)
    ui->utlsFingerprint->setCurrentText(Configs::dataStore->utlsFingerprint);
    // Startup
    // #TODO remote_control, resource_manager
    ui->remote_control->setHidden(true);
    ui->resource_manager->setHidden(true);

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
        emit size_changed(ui->width->text().toInt(), ui->height->text().toInt());
        emit point_changed(ui->X->text().toInt(), ui->Y->text().toInt());
    });

    auto validator = new QIntValidator(0, 0xfffffff, this);
    S_LOAD_BOOL(save_geometry)
    S_LOAD_BOOL(save_position)
    S_LOAD_BOOL(test_after_start)
    if (settings->startup_update != 4){
        S_LOAD_BOOL(startup_update)
    } else {
        ui->startup_update->setHidden(true);
        ui->startup_update->setDisabled(true);
    }
    S_LOAD_INT(width)
    S_LOAD_INT(height)
    S_LOAD_INT(X)
    S_LOAD_INT(Y)
    

    QString core_path = Configs::resourceManager->core_path;
    QString icons_path = Configs::resourceManager->resources_path;
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
    // Auto-testing
    D_SAVE_BOOL(auto_test_enable)
    Configs::dataStore->auto_test_interval_seconds = ui->auto_test_interval_seconds->value();
    Configs::dataStore->auto_test_proxy_count = ui->auto_test_proxy_count->value();
    Configs::dataStore->auto_test_working_pool_size = ui->auto_test_working_pool_size->value();
    Configs::dataStore->auto_test_latency_threshold_ms = ui->auto_test_latency_threshold_ms->value();
    Configs::dataStore->auto_test_failure_retry_count = ui->auto_test_failure_retry_count->value();
    D_SAVE_STRING(auto_test_target_url)
    D_SAVE_BOOL(auto_test_tun_failover)

    // Common
    bool needChoosePort = false;

    D_SAVE_STRING(inbound_address)
    D_SAVE_COMBO_STRING(log_level)
    Configs::dataStore->custom_inbound = CACHE.custom_inbound;
    D_SAVE_INT(inbound_socks_port)
    if (!Configs::dataStore->random_inbound_port && ui->random_inbound_port->isChecked())
    {
        needChoosePort = true;
    }
    D_SAVE_BOOL(random_inbound_port);
    D_SAVE_INT(test_concurrent)
    D_SAVE_STRING(test_latency_url)
    D_SAVE_BOOL(disable_tray)
    Configs::dataStore->proxy_scheme = ui->proxy_scheme->currentText().toLower();
    Configs::dataStore->speed_test_mode = ui->speedtest_mode->currentIndex();
    D_SAVE_STRING(simple_dl_url)
    Configs::ForceJsonConfigs = ui->force_json_configs->isChecked();
    D_SAVE_INT(url_test_timeout_ms)
    D_SAVE_INT(speed_test_timeout_ms)

    // Style

    D_SAVE_BOOL(connection_statistics);
    QString locale = "";
    int locale_index = ui->language->currentIndex();
    if (locale_index >= 0){
        locale = (locales[locale_index]);
    }
    D_SAVE_BOOL(start_minimal)
    S_SAVE_INT(max_log_line)
    #ifdef Q_OS_WIN
    D_SAVE_BOOL(show_system_dns);
    #endif

    if (settings->max_log_line <= 0) {
        settings->max_log_line = 200;
    }

    // Subscription

    if (ui->sub_auto_update_enable->isChecked()) {
        TM_auto_update_subsctiption_Reset_Minute(ui->sub_auto_update->text().toInt());
    } else {
        TM_auto_update_subsctiption_Reset_Minute(0);
    }

    D_SAVE_STRING(user_agent)
    D_SAVE_BOOL(net_use_proxy)

    S_SAVE_BOOL(test_after_start)
    if (!ui->startup_update->isHidden()){
        S_SAVE_BOOL(startup_update)
    }
    D_SAVE_BOOL(sub_clear)
    D_SAVE_BOOL(net_insecure)
    S_SAVE_BOOL(auto_hide)
    D_SAVE_BOOL(sub_send_hwid)
    D_SAVE_STRING(sub_custom_hwid_params)
    D_SAVE_BOOL(sub_rm_invalid)
    D_SAVE_BOOL(sub_url_test)

    D_SAVE_BOOL(sub_rm_duplicates)
    D_SAVE_BOOL(sub_rm_unavailable)
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

 //   int width, height, X, Y;
    // Startup
    settings->language = locale;
    S_SAVE_BOOL(save_geometry);
    S_SAVE_BOOL(save_position);
    S_SAVE_INT(width)
    S_SAVE_INT(height)
    S_SAVE_INT(X)
    S_SAVE_INT(Y)
    QString core_path_text, resources_path;
    bool need_save_manager = false;
    bool need_core_restart = false;
    if (ui->default_core_path->isChecked()){
        core_path_text = "";
    } else {
        core_path_text = ui->core_path->text();
    }
    if (Configs::resourceManager->core_path != core_path_text){
        need_save_manager = true;
        need_core_restart = true;
        Configs::resourceManager->core_path = core_path_text;
    }
    if (ui->default_icons_path->isChecked()){
        resources_path = "";
    } else {
        resources_path = (ui->icons_path->text());
    }
    if (Configs::resourceManager->resources_path != resources_path){
        Configs::resourceManager->resources_path = resources_path;
        need_save_manager = true;
    }
    if (need_save_manager){
        Configs::resourceManager->Save();
    }
    // Security

    D_SAVE_BOOL(skip_cert)
    Configs::dataStore->utlsFingerprint = ui->utlsFingerprint->currentText();
    D_SAVE_BOOL(disable_privilege_req);
    D_SAVE_BOOL(windows_no_admin);
    Configs::dataStore->use_mozilla_certs = ui->use_mozilla_certs->isChecked();

    QStringList str{"UpdateDataStore"};
    if (CACHE.needRestart) str << "NeedRestart";
    if (CACHE.updateDisableTray) str << "UpdateDisableTray";
    if (CACHE.updateSystemDns) str << "UpdateSystemDns";
    if (needChoosePort) str << "NeedChoosePort";
    if (CACHE.updateMenuIcon){
        settings->text_under_buttons = (ui->set_text_under_menu_icons->isChecked());
    }
    if (CACHE.updateMenuIcon || CACHE.updateIcon || CACHE.updateFont) str << "UpdateIcon";
    if (need_core_restart) str << "UpdateCorePath";
    MW_dialog_message(Dialog_DialogBasicSettings, str.join(","));
    settings->Save();
    QDialog::accept();
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
    core_box_clash_api->setText(Configs::dataStore->core_box_clash_api > 0 ? QString::number(Configs::dataStore->core_box_clash_api) : "");
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
