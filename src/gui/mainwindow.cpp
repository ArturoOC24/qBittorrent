/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "mainwindow.h"

#include <QtProcessorDetection>
#include <QtSystemDetection>

#include <algorithm>
#include <chrono>

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QMimeData>
#include <QOverload>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <QCryptographicHash>
#include <QDir>
#include <QScopeGuard>
#include <QToolButton>
#include "widgetframe/windowbar.h"
#include "widgetframe/windowbutton.h"
#endif

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/path.h"
#include "base/plugins/pluginsengine.h"
#include "base/preferences.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "base/tag.h"
#include "base/torrentfilter.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/password.h"
#include "base/version.h"
#include "aboutdialog.h"
#include "autoexpandabledialog.h"
#include "collapsiblesplitter.h"
#include "cookiesdialog.h"
#include "desktopintegration.h"
#include "downloadfromurldialog.h"
#include "executionlogwidget.h"
#include "hidabletabwidget.h"
#include "interfaces/iguiapplication.h"
#include "lineedit.h"
#include "optionsdialog.h"
#include "powermanagement/powermanagement.h"
#include "properties/peerlistwidget.h"
#include "properties/propertieswidget.h"
#include "properties/proptabbar.h"
#include "rss/rsswidget.h"
#include "search/searchwidget.h"
#include "speedgraphwidget.h"
#include "speedlimitdialog.h"
#include "statsdialog.h"
#include "statusbar.h"
#include "torrentcreatordialog.h"
#include "trackerlist/trackerlistwidget.h"
#include "transferlistfilterswidget.h"
#include "transferlistmodel.h"
#include "transferlistwidget.h"
#include "ui_mainwindow.h"
#include "uithememanager.h"
#include "utils.h"
#include "utils/keysequence.h"

#ifdef ENABLE_PLUGINS
#include "plugins/pluginsdialog.h"
#endif

#ifdef Q_OS_MACOS
#include "macosdockbadge/badger.h"
#include "macosstatusitem/statusitem.h"
#include "macutilities.h"
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include "programupdater.h"
#endif

using namespace std::chrono_literals;

namespace
{
#define SETTINGS_KEY(name) u"GUI/" name
#define EXECUTIONLOG_SETTINGS_KEY(name) (SETTINGS_KEY(u"Log/"_s) name)

    const std::chrono::seconds PREVENT_SUSPEND_INTERVAL {60};

#if defined(Q_OS_WIN)
#if defined(Q_PROCESSOR_X86_64)
    const QString PYTHON_INSTALLER_URL = u"https://www.python.org/ftp/python/3.14.5/python-3.14.5-amd64.exe"_s;
    const QByteArray PYTHON_INSTALLER_SHA2_256 = QByteArrayLiteral("f9c09f5ed6f796fd1a8bc5ddfa41715a494b453c4781f0e35d5077cf9fa58f6d");
#elif defined(Q_PROCESSOR_ARM_64)
    const QString PYTHON_INSTALLER_URL = u"https://www.python.org/ftp/python/3.14.5/python-3.14.5-arm64.exe"_s;
    const QByteArray PYTHON_INSTALLER_SHA2_256 = QByteArrayLiteral("f4a7df6ab4fa375cd7296127ff6b9a14fbd1313f51864ce020185deba10144fa");
#endif
#endif // Q_OS_WIN
}

MainWindow::MainWindow(IGUIApplication *app, const WindowState initialState, const QString &titleSuffix)
    : GUIApplicationComponent(app)
    , m_ui {new Ui::MainWindow}
    , m_downloadRate {Utils::Misc::friendlyUnit(0, true)}
    , m_uploadRate {Utils::Misc::friendlyUnit(0, true)}
    , m_pwr {new PowerManagement}
    , m_preventTimer {new QTimer(this)}
    , m_storeExecutionLogEnabled {EXECUTIONLOG_SETTINGS_KEY(u"Enabled"_s)}
    , m_storeDownloadTrackerFavicon {SETTINGS_KEY(u"DownloadTrackerFavicon"_s)}
    , m_storeCompactRows {SETTINGS_KEY(u"CompactRows"_s)}
    , m_storeSpeedGraphVisible {SETTINGS_KEY(u"SpeedGraphVisible"_s)}
    , m_storeFilterBarVisible {SETTINGS_KEY(u"FilterBarVisible"_s)}
    , m_storeTabBarVisible {SETTINGS_KEY(u"TabBarVisible"_s)}
    , m_storeStatusSectionVisible {SETTINGS_KEY(u"FilterSectionStatusVisible"_s)}
    , m_storeCategoriesSectionVisible {SETTINGS_KEY(u"FilterSectionCategoriesVisible"_s)}
    , m_storeTagsSectionVisible {SETTINGS_KEY(u"FilterSectionTagsVisible"_s)}
    , m_storeTrackersSectionVisible {SETTINGS_KEY(u"FilterSectionTrackersVisible"_s)}
    , m_storeFilterCombosMode {SETTINGS_KEY(u"FilterCombosMode"_s)}
    , m_storeHiddenToolbarActions {SETTINGS_KEY(u"HiddenToolbarActions"_s)}
    , m_storeExecutionLogTypes {EXECUTIONLOG_SETTINGS_KEY(u"Types"_s), Log::MsgType::ALL}
#ifdef Q_OS_MACOS
    , m_badger {std::make_unique<MacUtils::Badger>()}
    , m_statusItem {std::make_unique<MacUtils::StatusItem>()}
#endif // Q_OS_MACOS
{
    m_ui->setupUi(this);

    Preferences *const pref = Preferences::instance();
    m_uiLocked = pref->isUILocked();
    m_displaySpeedInTitle = pref->speedInTitleBar();
#ifdef Q_OS_MACOS
    m_statusItem->setVisible(pref->isMacOSMenuBarIconEnabled());
#else
    // Setting icons
    setWindowIcon(UIThemeManager::instance()->getIcon(u"qbittorrent"_s));
#endif // Q_OS_MACOS

    setTitleSuffix(titleSuffix);

#if (defined(Q_OS_UNIX))
    m_ui->actionOptions->setText(tr("Preferences"));
#endif

    addToolbarContextMenu();

    m_ui->actionOpen->setIcon(UIThemeManager::instance()->getIcon(u"list-add"_s));
    m_ui->actionDownloadFromURL->setIcon(UIThemeManager::instance()->getIcon(u"insert-link"_s));
    m_ui->actionSetGlobalSpeedLimits->setIcon(UIThemeManager::instance()->getIcon(u"speedometer"_s));
    m_ui->actionCreateTorrent->setIcon(UIThemeManager::instance()->getIcon(u"torrent-creator"_s, u"document-edit"_s));
    m_ui->actionAbout->setIcon(UIThemeManager::instance()->getIcon(u"help-about"_s));
    m_ui->actionStatistics->setIcon(UIThemeManager::instance()->getIcon(u"view-statistics"_s));
    m_ui->actionTopQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-top"_s));
    m_ui->actionIncreaseQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-up"_s));
    m_ui->actionDecreaseQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-down"_s));
    m_ui->actionBottomQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-bottom"_s));
    m_ui->actionDelete->setIcon(UIThemeManager::instance()->getIcon(u"list-remove"_s));
    m_ui->actionDocumentation->setIcon(UIThemeManager::instance()->getIcon(u"help-contents"_s));
    m_ui->actionDonateMoney->setIcon(UIThemeManager::instance()->getIcon(u"wallet-open"_s));
    m_ui->actionExit->setIcon(UIThemeManager::instance()->getIcon(u"application-exit"_s));
    m_ui->actionLock->setIcon(UIThemeManager::instance()->getIcon(u"object-locked"_s));
    m_ui->actionOptions->setIcon(UIThemeManager::instance()->getIcon(u"configure"_s, u"preferences-system"_s));
    m_ui->actionStart->setIcon(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s));
    m_ui->actionStop->setIcon(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s));
    m_ui->actionPauseSession->setIcon(UIThemeManager::instance()->getIcon(u"pause-session"_s, u"media-playback-pause"_s));
    m_ui->actionResumeSession->setIcon(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s));
    m_ui->menuAutoShutdownOnDownloadsCompletion->setIcon(UIThemeManager::instance()->getIcon(u"task-complete"_s, u"application-exit"_s));
    m_ui->actionManageCookies->setIcon(UIThemeManager::instance()->getIcon(u"browser-cookies"_s, u"preferences-web-browser-cookies"_s));
    m_ui->actionManagePlugins->setIcon(UIThemeManager::instance()->getIcon(u"plugins"_s));
    m_ui->menuLog->setIcon(UIThemeManager::instance()->getIcon(u"help-contents"_s));
    m_ui->actionCheckForUpdates->setIcon(UIThemeManager::instance()->getIcon(u"view-refresh"_s));
    m_ui->actionOpenDestinationFolder->setIcon(UIThemeManager::instance()->getIcon(u"directory"_s));

    m_ui->actionPauseSession->setVisible(!BitTorrent::Session::instance()->isPaused());
    m_ui->actionResumeSession->setVisible(BitTorrent::Session::instance()->isPaused());
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::paused, this, [this]
    {
        m_ui->actionPauseSession->setVisible(false);
        m_ui->actionResumeSession->setVisible(true);
        refreshWindowTitle();
        refreshTrayIconTooltip();
    });
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::resumed, this, [this]
    {
        m_ui->actionPauseSession->setVisible(true);
        m_ui->actionResumeSession->setVisible(false);
        refreshWindowTitle();
        refreshTrayIconTooltip();
    });

    auto *lockMenu = new QMenu(m_ui->menuView);
    lockMenu->addAction(tr("&Set Password"), this, &MainWindow::defineUILockPassword);
    lockMenu->addAction(tr("&Clear Password"), this, &MainWindow::clearUILockPassword);
    m_ui->actionLock->setMenu(lockMenu);

    updateAltSpeedsBtn(BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled());

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::speedLimitModeChanged, this, &MainWindow::updateAltSpeedsBtn);

    qDebug("create tabWidget");
    m_tabs = new HidableTabWidget(this);
    connect(m_tabs.data(), &QTabWidget::currentChanged, this, &MainWindow::tabChanged);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    auto *hSplitter = new CollapsibleSplitter(Qt::Vertical, this);
    hSplitter->setChildrenCollapsible(false);
    hSplitter->setHandleWidth(15);
    hSplitter->setFrameShape(QFrame::NoFrame);

    // Torrent filter
    auto *columnFilterSpacer = new QWidget;
    columnFilterSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_columnFilterEdit = new LineEdit;
    m_columnFilterEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    m_columnFilterEdit->setFixedWidth(200);
    m_columnFilterEdit->setPlaceholderText(tr("Filter torrents..."));
    m_columnFilterEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_columnFilterEdit, &QWidget::customContextMenuRequested, this, &MainWindow::showFilterContextMenu);

    m_columnFilterComboBox = new QComboBox;

    QHBoxLayout *columnFilterLayout = new QHBoxLayout;
    columnFilterLayout->setContentsMargins(0, 0, 0, 0);
    columnFilterLayout->addWidget(columnFilterSpacer);
    columnFilterLayout->addWidget(m_columnFilterEdit);
    columnFilterLayout->addWidget(new QLabel(tr("Filter by:")), 0);
    columnFilterLayout->addWidget(m_columnFilterComboBox, 0);

    m_columnFilterWidget = new QWidget(this);
    m_columnFilterWidget->setLayout(columnFilterLayout);
    // The column filter lives in a top bar above the transfer list (see below),
    // not in the toolbar - the toolbar is docked vertically on the left and a
    // 200px search box there would make the whole toolbar column very wide.

    // Transfer List tab
    m_transferListWidget = new TransferListWidget(app, this);

    // Speed graph floats as an overlay in the bottom-right of the torrent list.
    m_speedGraph = new SpeedGraphWidget(m_transferListWidget);
    m_speedGraph->setVisible(m_storeSpeedGraphVisible.get(true));
    m_transferListWidget->installEventFilter(this);
    positionSpeedGraph();

    // Optional top-bar filter comboboxes (alternative to the left sidebar).
    m_statusFilterComboBox = new QComboBox(this);
    for (const auto &[label, value] : std::initializer_list<std::pair<const char *, int>> {
            {QT_TR_NOOP("All Statuses"), TorrentFilter::All},
            {QT_TR_NOOP("Downloading"), TorrentFilter::Downloading},
            {QT_TR_NOOP("Seeding"), TorrentFilter::Seeding},
            {QT_TR_NOOP("Completed"), TorrentFilter::Completed},
            {QT_TR_NOOP("Running"), TorrentFilter::Running},
            {QT_TR_NOOP("Stopped"), TorrentFilter::Stopped},
            {QT_TR_NOOP("Active"), TorrentFilter::Active},
            {QT_TR_NOOP("Inactive"), TorrentFilter::Inactive},
            {QT_TR_NOOP("Stalled"), TorrentFilter::Stalled},
            {QT_TR_NOOP("Stalled Uploading"), TorrentFilter::StalledUploading},
            {QT_TR_NOOP("Stalled Downloading"), TorrentFilter::StalledDownloading},
            {QT_TR_NOOP("Checking"), TorrentFilter::Checking},
            {QT_TR_NOOP("Moving"), TorrentFilter::Moving},
            {QT_TR_NOOP("Errored"), TorrentFilter::Errored}})
    {
        m_statusFilterComboBox->addItem(tr(label), value);
    }
    connect(m_statusFilterComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::onStatusFilterChanged);

    m_categoryFilterComboBox = new QComboBox(this);
    connect(m_categoryFilterComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::onCategoryFilterChanged);

    m_tagFilterComboBox = new QComboBox(this);
    connect(m_tagFilterComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::onTagFilterChanged);

    refreshCategoryComboBox();
    refreshTagComboBox();
    {
        const auto *btSession = BitTorrent::Session::instance();
        connect(btSession, &BitTorrent::Session::categoryAdded, this, &MainWindow::refreshCategoryComboBox);
        connect(btSession, &BitTorrent::Session::categoryRemoved, this, &MainWindow::refreshCategoryComboBox);
        connect(btSession, &BitTorrent::Session::tagAdded, this, &MainWindow::refreshTagComboBox);
        connect(btSession, &BitTorrent::Session::tagRemoved, this, &MainWindow::refreshTagComboBox);
    }

    m_propertiesWidget = new PropertiesWidget(hSplitter);
    m_propertiesWidget->setObjectName(u"propertiesPanel"_s);
    connect(m_transferListWidget, &TransferListWidget::currentTorrentChanged, m_propertiesWidget, &PropertiesWidget::loadTorrentInfos);
    hSplitter->addWidget(m_transferListWidget);
    hSplitter->addWidget(m_propertiesWidget);
    hSplitter->setCollapsible(1, true);

    // Wire the toggle pill on the splitter handle to collapse/expand the panel.
    if (auto *toggleHandle = qobject_cast<CollapsibleSplitterHandle *>(hSplitter->handle(1)))
    {
        QToolButton *pill = toggleHandle->toggleButton();
        const auto refreshPillIcon = [this, pill]
        {
            pill->setIcon(UIThemeManager::instance()->getIcon(m_propertiesWidget->isPanelReduced() ? u"sort-asc"_s : u"sort-desc"_s));
            pill->setToolTip(m_propertiesWidget->isPanelReduced() ? tr("Show details panel") : tr("Hide details panel"));
        };
        refreshPillIcon();
        connect(pill, &QToolButton::clicked, this, [this]
        {
            m_propertiesWidget->setVisibility(m_propertiesWidget->isPanelReduced());
        });
        connect(m_propertiesWidget, &PropertiesWidget::panelVisibilityChanged, this, [refreshPillIcon](bool) { refreshPillIcon(); });
    }

    m_splitter->addWidget(hSplitter);
    m_splitter->setCollapsible(0, false);

    // Top filter bar above the transfer list holds the column filter (search
    // box + "Filter by"). columnFilterSpacer (added first in columnFilterLayout)
    // pushes the controls to the right edge, above the list.
    auto *filterBar = new QWidget(this);
    m_filterBar = filterBar;
    filterBar->setObjectName(u"filterBar"_s);
    filterBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *filterBarLayout = new QHBoxLayout(filterBar);
    filterBarLayout->setContentsMargins(6, 4, 6, 4);
    filterBarLayout->setSpacing(6);
    // Filter comboboxes on the left (shown only in "filter combos" mode), the
    // column filter (search box) on the right (its own spacer right-aligns it).
    filterBarLayout->addWidget(m_statusFilterComboBox);
    filterBarLayout->addWidget(m_categoryFilterComboBox);
    filterBarLayout->addWidget(m_tagFilterComboBox);
    m_columnFilterWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    filterBarLayout->addWidget(m_columnFilterWidget);

    m_transfersTab = new QWidget(this);
    auto *transfersLayout = new QVBoxLayout(m_transfersTab);
    transfersLayout->setContentsMargins(0, 0, 0, 0);
    transfersLayout->setSpacing(0);
    transfersLayout->addWidget(filterBar);
    transfersLayout->addWidget(m_splitter);

    m_tabs->addTab(m_transfersTab,
#ifndef Q_OS_MACOS
        UIThemeManager::instance()->getIcon(u"folder-remote"_s),
#endif
        tr("Transfers"));
    // Filter types
    const QList<TransferListModel::Column> filterTypes = {
        TransferListModel::Column::TR_NAME
        , TransferListModel::Column::TR_SAVE_PATH
        , TransferListModel::Column::TR_INFOHASH_V1
        , TransferListModel::Column::TR_INFOHASH_V2};
    for (const TransferListModel::Column type : filterTypes)
    {
        const QString typeName = m_transferListWidget->getSourceModel()->headerData(type, Qt::Horizontal, Qt::DisplayRole).value<QString>();
        m_columnFilterComboBox->addItem(typeName, type);
    }
    connect(m_columnFilterComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::applyTransferListFilter);
    connect(m_columnFilterEdit, &LineEdit::textUpdated, this, &MainWindow::applyTransferListFilter);
    connect(hSplitter, &QSplitter::splitterMoved, this, &MainWindow::saveSettings);
    connect(m_splitter, &QSplitter::splitterMoved, this, &MainWindow::saveSplitterSettings);

#ifdef Q_OS_MACOS
    // Increase top spacing to avoid tab overlapping
    m_ui->centralWidgetLayout->addSpacing(8);
#endif

    m_ui->centralWidgetLayout->addWidget(m_tabs);

    m_queueSeparator = m_ui->toolBar->insertSeparator(m_ui->actionTopQueuePos);
    m_queueSeparatorMenu = m_ui->menuEdit->insertSeparator(m_ui->actionTopQueuePos);

#ifdef Q_OS_MACOS
    for (QAction *action : asConst(m_ui->toolBar->actions()))
    {
        if (action->isSeparator())
        {
            auto *line = new QWidget(this);
            line->setAutoFillBackground(true);
            line->setFixedWidth(1);

            QPalette pal = line->palette();
            pal.setColor(QPalette::Window, palette().color(QPalette::Mid));
            line->setPalette(pal);

            QAction *widgetAction = m_ui->toolBar->insertWidget(action, line);
            m_ui->toolBar->removeAction(action);
            if (action == m_queueSeparator)
                m_queueSeparator = widgetAction;
        }
    }
#endif // Q_OS_MACOS

    // Transfer list slots
    connect(m_ui->actionStart, &QAction::triggered, m_transferListWidget, &TransferListWidget::startSelectedTorrents);
    connect(m_ui->actionStop, &QAction::triggered, m_transferListWidget, &TransferListWidget::stopSelectedTorrents);
    connect(m_ui->actionPauseSession, &QAction::triggered, m_transferListWidget, &TransferListWidget::pauseSession);
    connect(m_ui->actionResumeSession, &QAction::triggered, m_transferListWidget, &TransferListWidget::resumeSession);
    connect(m_ui->actionOpenDestinationFolder, &QAction::triggered, m_transferListWidget, &TransferListWidget::openSelectedTorrentsFolder);
    connect(m_ui->actionDelete, &QAction::triggered, m_transferListWidget, &TransferListWidget::softDeleteSelectedTorrents);
    connect(m_ui->actionTopQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::topQueuePosSelectedTorrents);
    connect(m_ui->actionIncreaseQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::increaseQueuePosSelectedTorrents);
    connect(m_ui->actionDecreaseQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::decreaseQueuePosSelectedTorrents);
    connect(m_ui->actionBottomQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::bottomQueuePosSelectedTorrents);
    connect(m_ui->actionMinimize, &QAction::triggered, this, &MainWindow::minimizeWindow);
    connect(m_ui->actionUseAlternativeSpeedLimits, &QAction::triggered, this, &MainWindow::toggleAlternativeSpeeds);

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    connect(m_ui->actionCheckForUpdates, &QAction::triggered, this, [this]() { checkProgramUpdate(true); });

    // trigger an early check on startup
    if (pref->isUpdateCheckEnabled())
        checkProgramUpdate(false);
#else
    m_ui->actionCheckForUpdates->setVisible(false);
#endif

    // Certain menu items should reside at specific places on macOS.
    // Qt partially does it on its own, but updates and different languages require tuning.
    m_ui->actionExit->setMenuRole(QAction::QuitRole);
    m_ui->actionAbout->setMenuRole(QAction::AboutRole);
    m_ui->actionCheckForUpdates->setMenuRole(QAction::ApplicationSpecificRole);
    m_ui->actionOptions->setMenuRole(QAction::PreferencesRole);

#ifdef Q_OS_MACOS
    // Set up native macOS Window menu
    auto *windowMenu = new QMenu(tr("&Window"), this);
    m_ui->menubar->insertMenu(m_ui->menuHelp->menuAction(), windowMenu);
    MacUtils::setupWindowMenu(windowMenu);
#endif

    connect(m_ui->actionManageCookies, &QAction::triggered, this, &MainWindow::manageCookies);

#ifdef ENABLE_PLUGINS
    connect(m_ui->actionManagePlugins, &QAction::triggered, this, &MainWindow::managePlugins);
#else
    m_ui->menuPlugins->hide();
#endif

    // Initialise system sleep inhibition timer
    m_preventTimer->setSingleShot(true);
    connect(m_preventTimer, &QTimer::timeout, this, &MainWindow::updatePowerManagementState);
    connect(pref, &Preferences::changed, this, &MainWindow::updatePowerManagementState);
    updatePowerManagementState();

    // Configure BT session according to options
    loadPreferences();

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::statsUpdated, this, &MainWindow::loadSessionStats);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentsUpdated, this, &MainWindow::reloadTorrentStats);

    createKeyboardShortcuts();

#ifdef Q_OS_MACOS
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    // View settings
    m_ui->actionTopToolBar->setChecked(pref->isToolbarDisplayed());
    m_ui->actionShowStatusbar->setChecked(pref->isStatusbarDisplayed());
    m_ui->actionSpeedInTitleBar->setChecked(pref->speedInTitleBar());
    m_ui->actionRSSReader->setChecked(pref->isRSSWidgetEnabled());
    m_ui->actionSearchWidget->setChecked(pref->isSearchEnabled());
    m_ui->actionExecutionLogs->setChecked(isExecutionLogEnabled());

    const Log::MsgTypes flags = executionLogMsgTypes();
    m_ui->actionNormalMessages->setChecked(flags.testFlag(Log::NORMAL));
    m_ui->actionInformationMessages->setChecked(flags.testFlag(Log::INFO));
    m_ui->actionWarningMessages->setChecked(flags.testFlag(Log::WARNING));
    m_ui->actionCriticalMessages->setChecked(flags.testFlag(Log::CRITICAL));

    displayRSSTab(m_ui->actionRSSReader->isChecked());
    on_actionExecutionLogs_triggered(m_ui->actionExecutionLogs->isChecked());
    on_actionNormalMessages_triggered(m_ui->actionNormalMessages->isChecked());
    on_actionInformationMessages_triggered(m_ui->actionInformationMessages->isChecked());
    on_actionWarningMessages_triggered(m_ui->actionWarningMessages->isChecked());
    on_actionCriticalMessages_triggered(m_ui->actionCriticalMessages->isChecked());
    if (m_ui->actionSearchWidget->isChecked())
        QMetaObject::invokeMethod(this, &MainWindow::on_actionSearchWidget_triggered, Qt::QueuedConnection);
    // Auto shutdown actions
    auto *autoShutdownGroup = new QActionGroup(this);
    autoShutdownGroup->setExclusive(true);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdownDisabled);
    autoShutdownGroup->addAction(m_ui->actionAutoExit);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdown);
    autoShutdownGroup->addAction(m_ui->actionAutoSuspend);
    autoShutdownGroup->addAction(m_ui->actionAutoHibernate);
    autoShutdownGroup->addAction(m_ui->actionAutoReboot);
#if (!defined(Q_OS_UNIX) || defined(Q_OS_MACOS)) || defined(QBT_USES_DBUS)
    m_ui->actionAutoShutdown->setChecked(pref->shutdownWhenDownloadsComplete());
    m_ui->actionAutoReboot->setChecked(pref->rebootWhenDownloadsComplete());
    m_ui->actionAutoSuspend->setChecked(pref->suspendWhenDownloadsComplete());
    m_ui->actionAutoHibernate->setChecked(pref->hibernateWhenDownloadsComplete());
#ifdef Q_OS_MACOS
    // macOS doesn't support Hibernate via Apple Events API
    m_ui->actionAutoHibernate->setDisabled(true);
#endif
#else
    m_ui->actionAutoShutdown->setDisabled(true);
    m_ui->actionAutoSuspend->setDisabled(true);
    m_ui->actionAutoHibernate->setDisabled(true);
#endif
    m_ui->actionAutoExit->setChecked(pref->shutdownqBTWhenDownloadsComplete());

    if (!autoShutdownGroup->checkedAction())
        m_ui->actionAutoShutdownDisabled->setChecked(true);

    // Load Window state and sizes
    loadSettings();

    populateDesktopIntegrationMenu();
#ifndef Q_OS_MACOS
    m_ui->actionLock->setVisible(app->desktopIntegration()->isActive());
    connect(app->desktopIntegration(), &DesktopIntegration::stateChanged, this, [this, app]()
    {
        m_ui->actionLock->setVisible(app->desktopIntegration()->isActive());
    });
#endif
    connect(app->desktopIntegration(), &DesktopIntegration::notificationClicked, this, &MainWindow::desktopNotificationClicked);
    connect(app->desktopIntegration(), &DesktopIntegration::activationRequested, this, [this]()
    {
#ifdef Q_OS_MACOS
        if (!isVisible())
            activate();
#else
        toggleVisibility();
#endif
    });

#ifdef Q_OS_MACOS
    if (initialState == WindowState::Normal)
    {
        show();
        activateWindow();
        raise();
    }
    else
    {
        // Make sure the Window is visible if we don't have a tray icon
        showMinimized();
    }
#else
    if (app->desktopIntegration()->isActive())
    {
        if ((initialState == WindowState::Normal) && !m_uiLocked)
        {
            show();
            activateWindow();
            raise();
        }
        else if (initialState == WindowState::Minimized)
        {
            showMinimized();
            if (pref->minimizeToTray())
            {
                hide();
                if (!pref->minimizeToTrayNotified())
                {
                    app->desktopIntegration()->showNotification(tr("qBittorrent is minimized to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
                    pref->setMinimizeToTrayNotified(true);
                }
            }
        }
    }
    else
    {
        // Make sure the Window is visible if we don't have a tray icon
        if (initialState != WindowState::Normal)
        {
            showMinimized();
        }
        else
        {
            show();
            activateWindow();
            raise();
        }
    }
#endif

    const bool isFiltersSidebarVisible = pref->isFiltersSidebarVisible();
    m_ui->actionShowFiltersSidebar->setChecked(isFiltersSidebarVisible);
    if (isFiltersSidebarVisible)
    {
        showFiltersSidebar(true);
    }
    else
    {
        m_transferListWidget->applyStatusFilter(pref->getTransSelFilter());
        m_transferListWidget->applyCategoryFilter(QString());
        m_transferListWidget->applyTagFilter(std::nullopt);
        m_transferListWidget->applyTrackerFilter({});
    }

    // Start watching the executable for updates
    m_executableWatcher = new QFileSystemWatcher(this);
    connect(m_executableWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::notifyOfUpdate);
    m_executableWatcher->addPath(qApp->applicationFilePath());

    m_transferListWidget->setFocus();

    // Update the number of torrents (tab)
    updateNbTorrents();
    connect(m_transferListWidget->getSourceModel(), &QAbstractItemModel::rowsInserted, this, &MainWindow::updateNbTorrents);
    connect(m_transferListWidget->getSourceModel(), &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateNbTorrents);

    connect(pref, &Preferences::changed, this, &MainWindow::optionsSaved);

#ifdef ENABLE_PLUGINS
    populatePluginsMenu();
#endif

    m_ui->actionCompactRows->setChecked(m_storeCompactRows.get(false));
    on_actionCompactRows_triggered(m_ui->actionCompactRows->isChecked());

    m_ui->actionShowSpeedGraph->setChecked(m_storeSpeedGraphVisible.get(true));

    m_ui->actionShowFilterBar->setChecked(m_storeFilterBarVisible.get(false));
    if (m_filterBar)
        m_filterBar->setVisible(m_storeFilterBarVisible.get(false));

    m_ui->actionShowTabBar->setChecked(m_storeTabBarVisible.get(false));
    static_cast<HidableTabWidget *>(m_tabs.data())->setTabBarForcedHidden(!m_storeTabBarVisible.get(false));

    m_ui->actionShowStatusFilter->setChecked(m_storeStatusSectionVisible.get(true));
    m_ui->actionShowCategoriesFilter->setChecked(m_storeCategoriesSectionVisible.get(true));
    m_ui->actionShowTagsFilter->setChecked(m_storeTagsSectionVisible.get(true));
    m_ui->actionShowTrackersFilter->setChecked(m_storeTrackersSectionVisible.get(true));

    const bool filterCombosMode = m_storeFilterCombosMode.get(false);
    m_ui->actionFilterCombos->setChecked(filterCombosMode);
    setFilterCombosMode(filterCombosMode);

    applyToolbarActionVisibility();

    // Details panel: reflect the properties panel's current state and keep the
    // menu item in sync with the toggle pill on the splitter handle.
    m_ui->actionShowDetailsPanel->setChecked(!m_propertiesWidget->isPanelReduced());
    connect(m_propertiesWidget, &PropertiesWidget::panelVisibilityChanged, this, [this](const bool visible)
    {
        m_ui->actionShowDetailsPanel->setChecked(visible);
    });

#ifdef Q_OS_WIN
    installWindowAgent();
#endif

    qDebug("GUI Built");
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

#ifdef Q_OS_WIN
void MainWindow::installWindowAgent()
{
    // Collect menus before the old menu bar is removed by setMenuWidget().
    QList<QMenu *> menus;
    for (QAction *action : menuBar()->actions())
    {
        if (QMenu *m = action->menu())
            menus.append(m);
    }

    // Build the custom title bar strip.
    auto *windowBar = new QWK::WindowBar();
    windowBar->setHostWidget(this);
    windowBar->setTitleFollowWindow(true);

    auto *minBtn = new QWK::WindowButton();
    minBtn->setObjectName(u"min-button"_s);
    minBtn->setProperty("system-button", true);
    minBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *maxBtn = new QWK::WindowButton();
    maxBtn->setObjectName(u"max-button"_s);
    maxBtn->setProperty("system-button", true);
    maxBtn->setCheckable(true);
    maxBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *closeBtn = new QWK::WindowButton();
    closeBtn->setObjectName(u"close-button"_s);
    closeBtn->setProperty("system-button", true);
    closeBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    windowBar->setMinButton(minBtn);
    windowBar->setMaxButton(maxBtn);
    windowBar->setCloseButton(closeBtn);

    // App-menu button: qBittorrent icon + name, opens popup containing all menus.
    auto *appBtn = new QToolButton(windowBar);
    appBtn->setObjectName(u"app-menu-button"_s);
    appBtn->setIcon(UIThemeManager::instance()->getIcon(u"qbittorrent"_s));
    appBtn->setIconSize(QSize(22, 22));
    appBtn->setText(u"   qBittorrent"_s);
    appBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    appBtn->setPopupMode(QToolButton::InstantPopup);

    auto *appMenu = new QMenu(appBtn);
    for (QMenu *m : menus)
    {
        m->setParent(appMenu, m->windowFlags()); // preserve Qt::Popup flag
        appMenu->addMenu(m);
    }
    appBtn->setMenu(appMenu);

    m_appMenuButton = appBtn;

    // Use the icon-button slot (layout index 0). Never insert extra widgets
    // into the bar layout directly: WindowBar's accessors (maxButton() etc.)
    // look widgets up by fixed layout index and would return the wrong one.
    windowBar->setIconButton(appBtn);

    // setMenuWidget() replaces the native menu bar and schedules its deletion.
    setMenuWidget(windowBar);

    connect(windowBar, &QWK::WindowBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(windowBar, &QWK::WindowBar::maximizeRequested, this, [this, maxBtn](bool max)
    {
        if (max)
            showMaximized();
        else
            showNormal();
        QEvent e(QEvent::Leave);
        QCoreApplication::sendEvent(maxBtn, &e);
    });
    connect(windowBar, &QWK::WindowBar::closeRequested, this, &QWidget::close);

    m_windowBar = windowBar;

    // Native frameless setup: keep WS_CAPTION | WS_THICKFRAME so DWM still
    // provides the shadow, Aero Snap and minimize/restore animations; the
    // visible frame itself is removed in WM_NCCALCSIZE (see nativeEvent()).
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    // WS_SYSMENU is required for the min/max boxes to be "active" so Windows 11
    // engages the Snap Layouts flyout on the maximize button (HTMAXBUTTON).
    ::SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    // No DwmExtendFrameIntoClientArea: the resize frame kept in WM_NCCALCSIZE
    // already gives the DWM shadow + rounded corners, and extending the frame
    // interferes with the Snap Layouts caption detection in windowed mode.
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    setupJumpList();
}

void MainWindow::updateTaskbarProgress()
{
    if (!m_taskbarList)
    {
        ITaskbarList3 *taskbarList = nullptr;
        if (FAILED(::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbarList))))
            return;
        if (FAILED(taskbarList->HrInit()))
        {
            taskbarList->Release();
            return;
        }
        m_taskbarList = taskbarList;
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());

    qlonglong wanted = 0;
    qlonglong completed = 0;
    for (const BitTorrent::Torrent *torrent : asConst(BitTorrent::Session::instance()->torrents()))
    {
        if (torrent->isDownloading() && !torrent->isFinished())
        {
            wanted += torrent->wantedSize();
            completed += torrent->completedSize();
        }
    }

    if (wanted > 0)
    {
        m_taskbarList->SetProgressState(hwnd, TBPF_NORMAL);
        m_taskbarList->SetProgressValue(hwnd, static_cast<ULONGLONG>(completed), static_cast<ULONGLONG>(wanted));
    }
    else
    {
        m_taskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
    }
}

void MainWindow::setupJumpList()
{
    ICustomDestinationList *destList = nullptr;
    if (FAILED(::CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&destList))))
        return;

    UINT minSlots = 0;
    IObjectArray *removedItems = nullptr;
    if (FAILED(destList->BeginList(&minSlots, IID_PPV_ARGS(&removedItems))))
    {
        destList->Release();
        return;
    }
    removedItems->Release();

    IObjectCollection *tasks = nullptr;
    if (SUCCEEDED(::CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tasks))))
    {
        const std::wstring exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();

        // PKEY_Title == {F29F85E0-4FF9-1068-AB91-08002B27B3D9}, 2
        const PROPERTYKEY pkeyTitle = {{0xF29F85E0, 0x4FF9, 0x1068, {0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9}}, 2};

        const auto addTask = [tasks, &exePath, &pkeyTitle](const QString &title, const QString &arguments)
        {
            IShellLinkW *link = nullptr;
            if (FAILED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))))
                return;

            link->SetPath(exePath.c_str());
            link->SetArguments(arguments.toStdWString().c_str());

            IPropertyStore *propStore = nullptr;
            if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&propStore))))
            {
                PROPVARIANT titleValue {};
                titleValue.vt = VT_LPWSTR;
                if (SUCCEEDED(::SHStrDupW(title.toStdWString().c_str(), &titleValue.pwszVal)))
                {
                    propStore->SetValue(pkeyTitle, titleValue);
                    propStore->Commit();
                    ::PropVariantClear(&titleValue);
                }
                propStore->Release();
            }

            tasks->AddObject(link);
            link->Release();
        };

        addTask(tr("Add Torrent Link..."), u"@showAddTorrentLink"_s);
        addTask(tr("Resume Session"), u"@resumeSession"_s);
        addTask(tr("Pause Session"), u"@pauseSession"_s);

        IObjectArray *taskArray = nullptr;
        if (SUCCEEDED(tasks->QueryInterface(IID_PPV_ARGS(&taskArray))))
        {
            destList->AddUserTasks(taskArray);
            taskArray->Release();
        }
        tasks->Release();
    }

    destList->CommitList();
    destList->Release();
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if ((eventType == "windows_generic_MSG") && m_windowBar)
    {
        MSG *msg = static_cast<MSG *>(message);
        switch (msg->message)
        {
        case WM_NCCALCSIZE:
            {
                if (!msg->wParam)
                    break;

                // Keep the standard resize frame on the left/right/bottom (so
                // DWM still manages the caption - required for Snap Layouts and
                // the maximize-button flyout) but reclaim the top edge for our
                // custom title bar. When maximized the window overhangs the
                // screen by the frame width, so inset the top back too.
                auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                RECT &rect = params->rgrc[0];
                const int frameX = ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER);
                const int frameY = ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER);
                rect.left += frameX;
                rect.right -= frameX;
                rect.bottom -= frameY;
                if (::IsZoomed(msg->hwnd))
                    rect.top += frameY;

                *result = 0;
                return true;
            }
        case WM_NCHITTEST:
            {
                const QPoint globalCursor {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
                RECT wndRect;
                ::GetWindowRect(msg->hwnd, &wndRect);

                const bool maximized = ::IsZoomed(msg->hwnd);
                const int rb = ::GetSystemMetrics(SM_CXSIZEFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER);

                const bool nearLeft = !maximized && (globalCursor.x() < (wndRect.left + rb));
                const bool nearRight = !maximized && (globalCursor.x() >= (wndRect.right - rb));
                const bool nearTop = !maximized && (globalCursor.y() < (wndRect.top + rb));
                const bool nearBottom = !maximized && (globalCursor.y() >= (wndRect.bottom - rb));

                // Corners take priority (resize)
                if (nearTop && nearLeft) { *result = HTTOPLEFT; return true; }
                if (nearTop && nearRight) { *result = HTTOPRIGHT; return true; }
                if (nearBottom && nearLeft) { *result = HTBOTTOMLEFT; return true; }
                if (nearBottom && nearRight) { *result = HTBOTTOMRIGHT; return true; }

                // Top resize edge takes priority over the title-bar drag band:
                // the thin strip at the very top (rb px) must resize, otherwise
                // the title bar swallows it and the top edge can't be dragged.
                if (nearTop) { *result = HTTOP; return true; }

                // Title bar band: use the window bar's own geometry so the drag
                // band and the child hit-testing share one coordinate system.
                const qreal dpr = devicePixelRatioF();
                POINT clientPt {globalCursor.x(), globalCursor.y()};
                ::ScreenToClient(msg->hwnd, &clientPt);
                const QPoint logicalInWindow {qRound(clientPt.x / dpr), qRound(clientPt.y / dpr)};

                const QRect barGeo {m_windowBar->mapTo(this, QPoint(0, 0)), m_windowBar->size()};
                if (barGeo.contains(logicalInWindow))
                {
                    // Maximize button reports HTMAXBUTTON so Windows 11 shows
                    // its Snap Layouts flyout on hover.
                    if (QAbstractButton *maxButton = m_windowBar->maxButton()
                        ; maxButton && maxButton->isVisible())
                    {
                        const QRect maxRect {maxButton->mapTo(this, QPoint(0, 0)), maxButton->size()};
                        if (maxRect.contains(logicalInWindow))
                        {
                            *result = HTMAXBUTTON;
                            return true;
                        }
                    }

                    // Interactive only when directly over one of the title bar buttons.
                    bool interactive = false;
                    const QList<QAbstractButton *> buttons = m_windowBar->findChildren<QAbstractButton *>();
                    for (QAbstractButton *btn : buttons)
                    {
                        if (!btn->isVisible())
                            continue;
                        const QRect btnRect {btn->mapTo(this, QPoint(0, 0)), btn->size()};
                        if (btnRect.contains(logicalInWindow))
                        {
                            interactive = true;
                            break;
                        }
                    }

                    *result = interactive ? HTCLIENT : HTCAPTION;
                    return true;
                }

                // Remaining resize edges (below the title bar)
                if (nearLeft) { *result = HTLEFT; return true; }
                if (nearRight) { *result = HTRIGHT; return true; }
                if (nearBottom) { *result = HTBOTTOM; return true; }
                if (nearTop) { *result = HTTOP; return true; }

                // Everything else is plain client area. Never fall through to
                // DefWindowProc here: with WS_CAPTION kept it still computes
                // phantom caption-button zones at the top of the window,
                // hijacking clicks meant for our own buttons.
                *result = HTCLIENT;
                return true;
            }
        case WM_NCLBUTTONDOWN:
            // Start the window move ourselves: with the custom frame, letting
            // DefWindowProc handle the caption drag is unreliable in windowed
            // mode (it works only while maximized).
            if ((msg->wParam == HTCAPTION) && !::IsZoomed(msg->hwnd))
            {
                ::ReleaseCapture();
                ::SendMessageW(msg->hwnd, WM_SYSCOMMAND, (SC_MOVE | 0x0002), msg->lParam);
                *result = 0;
                return true;
            }
            // Swallow the press on the maximize button; act on release.
            if (msg->wParam == HTMAXBUTTON)
            {
                *result = 0;
                return true;
            }
            break;
        case WM_NCLBUTTONDBLCLK:
            // Double-click the caption toggles maximize/restore
            if ((msg->wParam == HTCAPTION) || (msg->wParam == HTMAXBUTTON))
            {
                if (isMaximized())
                    showNormal();
                else
                    showMaximized();
                *result = 0;
                return true;
            }
            break;
        case WM_NCLBUTTONUP:
            // Click on the maximize button (Windows owns the mouse there)
            if (msg->wParam == HTMAXBUTTON)
            {
                if (isMaximized())
                    showNormal();
                else
                    showMaximized();
                *result = 0;
                return true;
            }
            break;
        case WM_NCMOUSEMOVE:
            // Windows owns the mouse over HTMAXBUTTON, so Qt never sees the
            // hover - emulate it here. wParam carries the hit-test code.
            if (QAbstractButton *maxButton = m_windowBar->maxButton())
            {
                const bool hover = (msg->wParam == HTMAXBUTTON);
                if (maxButton->property("ncHover").toBool() != hover)
                {
                    maxButton->setProperty("ncHover", hover);
                    maxButton->style()->unpolish(maxButton);
                    maxButton->style()->polish(maxButton);
                    maxButton->update();
                }
                if (hover)
                {
                    TRACKMOUSEEVENT tme {sizeof(TRACKMOUSEEVENT), (TME_LEAVE | TME_NONCLIENT), msg->hwnd, 0};
                    ::TrackMouseEvent(&tme);
                }
            }
            break;
        case WM_NCMOUSELEAVE:
            if (QAbstractButton *maxButton = m_windowBar->maxButton()
                ; maxButton && maxButton->property("ncHover").toBool())
            {
                maxButton->setProperty("ncHover", false);
                maxButton->style()->unpolish(maxButton);
                maxButton->style()->polish(maxButton);
                maxButton->update();
            }
            break;
        default:
            break;
        }
    }
    return GUIApplicationComponent<QMainWindow>::nativeEvent(eventType, message, result);
}
#endif // Q_OS_WIN

bool MainWindow::isExecutionLogEnabled() const
{
    return m_storeExecutionLogEnabled;
}

void MainWindow::setExecutionLogEnabled(const bool value)
{
    m_storeExecutionLogEnabled = value;
}

Log::MsgTypes MainWindow::executionLogMsgTypes() const
{
    return m_storeExecutionLogTypes;
}

void MainWindow::setExecutionLogMsgTypes(const Log::MsgTypes value)
{
    m_executionLog->setMessageTypes(value);
    m_storeExecutionLogTypes = value;
}

bool MainWindow::isDownloadTrackerFavicon() const
{
    return m_storeDownloadTrackerFavicon;
}

void MainWindow::setDownloadTrackerFavicon(const bool value)
{
    if (m_transferListFiltersWidget)
        m_transferListFiltersWidget->setDownloadTrackerFavicon(value);
    m_storeDownloadTrackerFavicon = value;
}

void MainWindow::setTitleSuffix(const QString &suffix)
{
    const auto emDash = QChar(0x2014);
    const QString separator = u' ' + emDash + u' ';
    m_windowTitle = QStringLiteral("qBittorrent " QBT_VERSION)
        + (!suffix.isEmpty() ? (separator + suffix) : QString());

    refreshWindowTitle();
}

void MainWindow::addToolbarContextMenu()
{
    const Preferences *const pref = Preferences::instance();
    m_toolbarMenu = new QMenu(this);

    m_ui->toolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->toolBar, &QWidget::customContextMenuRequested, this, &MainWindow::toolbarMenuRequested);

    QAction *iconsOnly = m_toolbarMenu->addAction(tr("Icons Only"), this, &MainWindow::toolbarIconsOnly);
    QAction *textOnly = m_toolbarMenu->addAction(tr("Text Only"), this, &MainWindow::toolbarTextOnly);
    QAction *textBesideIcons = m_toolbarMenu->addAction(tr("Text Alongside Icons"), this, &MainWindow::toolbarTextBeside);
    QAction *textUnderIcons = m_toolbarMenu->addAction(tr("Text Under Icons"), this, &MainWindow::toolbarTextUnder);
    QAction *followSystemStyle = m_toolbarMenu->addAction(tr("Follow System Style"), this, &MainWindow::toolbarFollowSystem);

    auto *textPositionGroup = new QActionGroup(m_toolbarMenu);
    textPositionGroup->addAction(iconsOnly);
    iconsOnly->setCheckable(true);
    textPositionGroup->addAction(textOnly);
    textOnly->setCheckable(true);
    textPositionGroup->addAction(textBesideIcons);
    textBesideIcons->setCheckable(true);
    textPositionGroup->addAction(textUnderIcons);
    textUnderIcons->setCheckable(true);
    textPositionGroup->addAction(followSystemStyle);
    followSystemStyle->setCheckable(true);

    const auto buttonStyle = static_cast<Qt::ToolButtonStyle>(pref->getToolbarTextPosition());
    if ((buttonStyle >= Qt::ToolButtonIconOnly) && (buttonStyle <= Qt::ToolButtonFollowStyle))
        m_ui->toolBar->setToolButtonStyle(buttonStyle);
    switch (buttonStyle)
    {
    case Qt::ToolButtonIconOnly:
        iconsOnly->setChecked(true);
        break;
    case Qt::ToolButtonTextOnly:
        textOnly->setChecked(true);
        break;
    case Qt::ToolButtonTextBesideIcon:
        textBesideIcons->setChecked(true);
        break;
    case Qt::ToolButtonTextUnderIcon:
        textUnderIcons->setChecked(true);
        break;
    default:
        followSystemStyle->setChecked(true);
    }

    m_toolbarMenu->addSeparator();
    m_toolbarButtonsMenu = m_toolbarMenu->addMenu(tr("Toolbar Buttons"));
}

void MainWindow::manageCookies()
{
    auto *cookieDialog = new CookiesDialog(this);
    cookieDialog->setAttribute(Qt::WA_DeleteOnClose);
    cookieDialog->open();
}

void MainWindow::toolbarMenuRequested()
{
    // Rebuild the per-button visibility toggles so they reflect the current
    // toolbar actions and their visibility.
    if (m_toolbarButtonsMenu)
    {
        m_toolbarButtonsMenu->clear();
        for (QAction *action : asConst(m_ui->toolBar->actions()))
        {
            // Skip separators / separator-line widgets (they have no text).
            if (action->isSeparator() || action->text().isEmpty() || action->objectName().isEmpty())
                continue;

            QAction *toggle = m_toolbarButtonsMenu->addAction(action->text());
            toggle->setCheckable(true);
            toggle->setChecked(action->isVisible());
            if (!action->icon().isNull())
                toggle->setIcon(action->icon());
            connect(toggle, &QAction::toggled, this, [this, action](const bool visible)
            {
                action->setVisible(visible);

                const QString name = action->objectName();
                QStringList hidden = m_storeHiddenToolbarActions.get();
                hidden.removeAll(name);
                if (!visible)
                    hidden.append(name);
                m_storeHiddenToolbarActions = hidden;
            });
        }
    }

    m_toolbarMenu->popup(QCursor::pos());
}

void MainWindow::applyToolbarActionVisibility()
{
    const QStringList hidden = m_storeHiddenToolbarActions.get();
    if (hidden.isEmpty())
        return;

    for (QAction *action : asConst(m_ui->toolBar->actions()))
    {
        if (!action->objectName().isEmpty() && hidden.contains(action->objectName()))
            action->setVisible(false);
    }
}

void MainWindow::toolbarIconsOnly()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonIconOnly);
}

void MainWindow::toolbarTextOnly()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextOnly);
}

void MainWindow::toolbarTextBeside()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextBesideIcon);
}

void MainWindow::toolbarTextUnder()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::toolbarFollowSystem()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonFollowStyle);
}

bool MainWindow::defineUILockPassword()
{
    bool ok = false;
    const QString newPassword = AutoExpandableDialog::getText(this, tr("UI lock password")
        , tr("Please type the UI lock password:"), QLineEdit::Password, {}, &ok);
    if (!ok)
        return false;

    if (newPassword.size() < 3)
    {
        QMessageBox::warning(this, tr("Invalid password"), tr("The password must be at least 3 characters long"));
        return false;
    }

    Preferences::instance()->setUILockPassword(Utils::Password::PBKDF2::generate(newPassword));
    return true;
}

void MainWindow::clearUILockPassword()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Clear the password")
        , tr("Are you sure you want to clear the password?"), (QMessageBox::Yes | QMessageBox::No), QMessageBox::No);
    if (answer == QMessageBox::Yes)
        Preferences::instance()->setUILockPassword({});
}

void MainWindow::on_actionLock_triggered()
{
    Preferences *const pref = Preferences::instance();

    // Check if there is a password
    if (pref->getUILockPassword().isEmpty())
    {
        if (!defineUILockPassword())
            return;
    }

    // Lock the interface
    m_uiLocked = true;
    pref->setUILocked(true);
    app()->desktopIntegration()->menu()->setEnabled(false);
    hide();
}

void MainWindow::handleRSSUnreadCountUpdated(int count)
{
    m_tabs->setTabText(m_tabs->indexOf(m_rssWidget), tr("RSS (%1)").arg(count));
}

void MainWindow::displayRSSTab(bool enable)
{
    if (enable)
    {
        // RSS tab
        if (!m_rssWidget)
        {
            m_rssWidget = new RSSWidget(app(), m_tabs);
            connect(m_rssWidget.data(), &RSSWidget::unreadCountUpdated, this, &MainWindow::handleRSSUnreadCountUpdated);
            connect(m_rssWidget.data(), &RSSWidget::openRSSSettingsLinkActivated, this, [this]
            {
                on_actionOptions_triggered();
                m_options->showRSSTab();
            });
#ifdef Q_OS_MACOS
            m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
#else
            const int indexTab = m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
            m_tabs->setTabIcon(indexTab, UIThemeManager::instance()->getIcon(u"application-rss"_s));
#endif
        }
    }
    else
    {
        delete m_rssWidget;
    }
}

void MainWindow::showFilterContextMenu()
{
    const Preferences *pref = Preferences::instance();

    QMenu *menu = m_columnFilterEdit->createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addSeparator();

    QAction *useRegexAct = menu->addAction(tr("Use regular expressions"));
    useRegexAct->setCheckable(true);
    useRegexAct->setChecked(pref->getRegexAsFilteringPatternForTransferList());
    connect(useRegexAct, &QAction::toggled, pref, &Preferences::setRegexAsFilteringPatternForTransferList);
    connect(useRegexAct, &QAction::toggled, this, &MainWindow::applyTransferListFilter);

    menu->popup(QCursor::pos());
}

void MainWindow::displaySearchTab(bool enable)
{
    Preferences::instance()->setSearchEnabled(enable);
    if (enable)
    {
        // RSS tab
        if (!m_searchWidget)
        {
            m_searchWidget = new SearchWidget(app(), this);
            connect(m_searchWidget, &SearchWidget::searchFinished, this, [this](const bool failed)
            {
                if (app()->desktopIntegration()->isNotificationsEnabled() && (currentTabWidget() != m_searchWidget))
                {
                    if (failed)
                        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Search has failed"));
                    else
                        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Search has finished"));
                }
            });
            m_tabs->insertTab(1, m_searchWidget,
#ifndef Q_OS_MACOS
                UIThemeManager::instance()->getIcon(u"edit-find"_s),
#endif
                tr("Search"));
        }
    }
    else
    {
        delete m_searchWidget;
    }
}

void MainWindow::toggleFocusBetweenLineEdits()
{
    // Content/Files tab is index 4 in the QTabWidget-based properties panel.
    if (m_columnFilterEdit->hasFocus() && (m_propertiesWidget->tabBar()->currentIndex() == 4))
    {
        m_propertiesWidget->contentFilterLine()->setFocus();
        m_propertiesWidget->contentFilterLine()->selectAll();
    }
    else
    {
        m_columnFilterEdit->setFocus();
        m_columnFilterEdit->selectAll();
    }
}

void MainWindow::updateNbTorrents()
{
    m_tabs->setTabText(0, tr("Transfers (%1)").arg(m_transferListWidget->getSourceModel()->rowCount()));
}

void MainWindow::on_actionDocumentation_triggered() const
{
    QDesktopServices::openUrl(QUrl(u"https://wiki.qbittorrent.org"_s));
}

void MainWindow::tabChanged([[maybe_unused]] const int newTab)
{
    // We cannot rely on the index newTab
    // because the tab order is undetermined now
    if (m_tabs->currentWidget() == m_transfersTab)
    {
        qDebug("Changed tab to transfer list, refreshing the list");
        m_propertiesWidget->loadDynamicData();
        m_columnFilterWidget->setVisible(true);
        return;
    }
    m_columnFilterWidget->setVisible(false);

    if (m_tabs->currentWidget() == m_searchWidget)
    {
        qDebug("Changed tab to search engine, giving focus to search input");
        m_searchWidget->giveFocusToSearchInput();
    }
}

void MainWindow::saveSettings() const
{
    auto *pref = Preferences::instance();
    pref->setMainGeometry(saveGeometry());
    m_propertiesWidget->saveSettings();
}

void MainWindow::saveSplitterSettings() const
{
    if (!m_transferListFiltersWidget)
        return;

    auto *pref = Preferences::instance();
    pref->setFiltersSidebarWidth(m_splitter->sizes()[0]);
}

void MainWindow::cleanup()
{
    if (!m_neverShown)
    {
        saveSettings();
        saveSplitterSettings();
    }

    // delete RSSWidget explicitly to avoid crash in
    // handleRSSUnreadCountUpdated() at application shutdown
    delete m_rssWidget;

    delete m_executableWatcher;

    m_preventTimer->stop();
    delete m_pwr;

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    if (m_programUpdateTimer)
        m_programUpdateTimer->stop();
#endif

    // remove all child widgets
    while (auto *w = findChild<QWidget *>())
        delete w;
}

void MainWindow::loadSettings()
{
    const auto *pref = Preferences::instance();

    if (const QByteArray mainGeo = pref->getMainGeometry();
        !mainGeo.isEmpty() && restoreGeometry(mainGeo))
    {
        m_posInitialized = true;
    }
}

void MainWindow::desktopNotificationClicked()
{
    if (isHidden())
    {
        if (m_uiLocked)
        {
            // Ask for UI lock password
            if (!unlockUI())
                return;
        }
        show();
        if (isMinimized())
            showNormal();
    }

    raise();
    activateWindow();
}

void MainWindow::createKeyboardShortcuts()
{
    m_ui->actionCreateTorrent->setShortcut(QKeySequence::New);
    m_ui->actionOpen->setShortcut(QKeySequence::Open);
    m_ui->actionDelete->setShortcut(Utils::KeySequence::deleteItem());
    m_ui->actionDelete->setShortcutContext(Qt::WidgetShortcut);  // nullify its effect: delete key event is handled by respective widgets, not here
    m_ui->actionDownloadFromURL->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
    m_ui->actionExit->setShortcut(Qt::CTRL | Qt::Key_Q);
#ifdef Q_OS_MACOS
    m_ui->actionCloseWindow->setShortcut(QKeySequence::Close);
#else
    m_ui->actionCloseWindow->setVisible(false);
#endif

    const auto *switchTransferShortcut = new QShortcut((Qt::ALT | Qt::Key_1), this);
    connect(switchTransferShortcut, &QShortcut::activated, this, &MainWindow::displayTransferTab);
    const auto *switchSearchShortcut = new QShortcut((Qt::ALT | Qt::Key_2), this);
    connect(switchSearchShortcut, &QShortcut::activated, this, qOverload<>(&MainWindow::displaySearchTab));
    const auto *switchRSSShortcut = new QShortcut((Qt::ALT | Qt::Key_3), this);
    connect(switchRSSShortcut, &QShortcut::activated, this, qOverload<>(&MainWindow::displayRSSTab));
    const auto *switchExecutionLogShortcut = new QShortcut((Qt::ALT | Qt::Key_4), this);
    connect(switchExecutionLogShortcut, &QShortcut::activated, this, &MainWindow::displayExecutionLogTab);
    const auto *switchSearchFilterShortcut = new QShortcut(QKeySequence::Find, m_transferListWidget);
    connect(switchSearchFilterShortcut, &QShortcut::activated, this, &MainWindow::toggleFocusBetweenLineEdits);
    const auto *switchSearchFilterShortcutAlternative = new QShortcut((Qt::CTRL | Qt::Key_E), m_transferListWidget);
    connect(switchSearchFilterShortcutAlternative, &QShortcut::activated, this, &MainWindow::toggleFocusBetweenLineEdits);

    m_ui->actionDocumentation->setShortcut(QKeySequence::HelpContents);
    m_ui->actionOptions->setShortcut(Qt::ALT | Qt::Key_O);
    m_ui->actionStatistics->setShortcut(Qt::CTRL | Qt::Key_I);
    m_ui->actionStart->setShortcut(Qt::CTRL | Qt::Key_S);
    m_ui->actionStop->setShortcut(Qt::CTRL | Qt::Key_P);
    m_ui->actionPauseSession->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_P);
    m_ui->actionResumeSession->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    m_ui->actionBottomQueuePos->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus);
    m_ui->actionDecreaseQueuePos->setShortcut(Qt::CTRL | Qt::Key_Minus);
    m_ui->actionIncreaseQueuePos->setShortcut(Qt::CTRL | Qt::Key_Plus);
    m_ui->actionTopQueuePos->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus);
#ifdef Q_OS_MACOS
    m_ui->actionMinimize->setShortcut(Qt::CTRL | Qt::Key_M);
    addAction(m_ui->actionMinimize);
#endif
}

// Keyboard shortcuts slots
void MainWindow::displayTransferTab() const
{
    m_tabs->setCurrentWidget(m_transfersTab);
}

void MainWindow::displaySearchTab()
{
    if (!m_searchWidget)
    {
        m_ui->actionSearchWidget->setChecked(true);
        displaySearchTab(true);
    }

    m_tabs->setCurrentWidget(m_searchWidget);
}

void MainWindow::displayRSSTab()
{
    if (!m_rssWidget)
    {
        m_ui->actionRSSReader->setChecked(true);
        displayRSSTab(true);
    }

    m_tabs->setCurrentWidget(m_rssWidget);
}

void MainWindow::displayExecutionLogTab()
{
    if (!m_executionLog)
    {
        m_ui->actionExecutionLogs->setChecked(true);
        on_actionExecutionLogs_triggered(true);
    }

    m_tabs->setCurrentWidget(m_executionLog);
}

// End of keyboard shortcuts slots

void MainWindow::on_actionSetGlobalSpeedLimits_triggered()
{
    auto *dialog = new SpeedLimitDialog {this};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

// Necessary if we want to close the window
// in one time if "close to systray" is enabled
void MainWindow::on_actionExit_triggered()
{
    // UI locking enforcement.
    if (isHidden() && m_uiLocked)
        // Ask for UI lock password
        if (!unlockUI()) return;

    m_forceExit = true;
    close();
}

#ifdef Q_OS_MACOS
void MainWindow::on_actionCloseWindow_triggered()
{
    // On macOS window close is basically equivalent to window hide.
    // If you decide to implement this functionality for other OS,
    // then you will also need ui lock checks like in actionExit.
    close();
}
#endif

QWidget *MainWindow::currentTabWidget() const
{
    if (isMinimized() || !isVisible())
        return nullptr;
    if (m_tabs->currentIndex() == 0)
        return m_transferListWidget;
    return m_tabs->currentWidget();
}

TransferListWidget *MainWindow::transferListWidget() const
{
    return m_transferListWidget;
}

bool MainWindow::unlockUI()
{
    if (m_unlockDlgShowing)
        return false;

    bool ok = false;
    const QString password = AutoExpandableDialog::getText(this, tr("UI lock password")
        , tr("Please type the UI lock password:"), QLineEdit::Password, {}, &ok);
    if (!ok) return false;

    Preferences *const pref = Preferences::instance();

    const QByteArray secret = pref->getUILockPassword();
    if (!Utils::Password::PBKDF2::verify(secret, password))
    {
        QMessageBox::warning(this, tr("Invalid password"), tr("The password is invalid"));
        return false;
    }

    m_uiLocked = false;
    pref->setUILocked(false);
    app()->desktopIntegration()->menu()->setEnabled(true);
    return true;
}

void MainWindow::notifyOfUpdate(const QString &)
{
    // Show restart message
    m_statusBar->showRestartRequired();
    LogMsg(tr("qBittorrent was just updated and needs to be restarted for the changes to be effective.")
                                   , Log::CRITICAL);
    // Delete the executable watcher
    delete m_executableWatcher;
    m_executableWatcher = nullptr;
}

#ifndef Q_OS_MACOS
// Toggle Main window visibility
void MainWindow::toggleVisibility()
{
    if (isHidden())
    {
        if (m_uiLocked && !unlockUI())  // Ask for UI lock password
            return;

        // Make sure the window is not minimized
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);

        // Then show it
        show();
        raise();
        activateWindow();
    }
    else
    {
        hide();
    }
}
#endif // Q_OS_MACOS

// Display About Dialog
void MainWindow::on_actionAbout_triggered()
{
    // About dialog
    if (m_aboutDlg)
    {
        m_aboutDlg->activateWindow();
    }
    else
    {
        m_aboutDlg = new AboutDialog(this);
        m_aboutDlg->setAttribute(Qt::WA_DeleteOnClose);
        m_aboutDlg->show();
    }
}

void MainWindow::on_actionStatistics_triggered()
{
    if (m_statsDlg)
    {
        m_statsDlg->activateWindow();
    }
    else
    {
        m_statsDlg = new StatsDialog(this);
        m_statsDlg->setAttribute(Qt::WA_DeleteOnClose);
        m_statsDlg->show();
    }
}

void MainWindow::showEvent(QShowEvent *e)
{
    qDebug("** Show Event **");
    e->accept();

    if (isVisible())
    {
        // preparations before showing the window

        if (m_neverShown)
        {
            m_propertiesWidget->readSettings();
            m_neverShown = false;
        }

        if (currentTabWidget() == m_transferListWidget)
            m_propertiesWidget->loadDynamicData();

        // Make sure the window is initially centered
        if (!m_posInitialized)
        {
            move(Utils::Gui::screenCenter(this));
            m_posInitialized = true;
        }
    }
    else
    {
        // to avoid blank screen when restoring from tray icon
        show();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Paste))
    {
        const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
        if (mimeData->hasText())
        {
            const QStringList lines = mimeData->text().split(u'\n', Qt::SkipEmptyParts);
            for (QString line : lines)
            {
                line = line.trimmed();
                if (!Utils::Misc::isTorrentLink(line))
                    continue;

                app()->addTorrentManager()->addTorrent(line);
            }

            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

// Called when we close the program
void MainWindow::closeEvent(QCloseEvent *e)
{
    Preferences *const pref = Preferences::instance();
#ifdef Q_OS_MACOS
    if (!m_forceExit)
    {
        hide();
        e->ignore();
        return;
    }
#else
    const bool goToSystrayOnExit = pref->closeToTray();
    if (!m_forceExit && app()->desktopIntegration()->isActive() && goToSystrayOnExit && !this->isHidden())
    {
        e->ignore();
        QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
        if (!pref->closeToTrayNotified())
        {
            app()->desktopIntegration()->showNotification(tr("qBittorrent is closed to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
            pref->setCloseToTrayNotified(true);
        }
        return;
    }
#endif // Q_OS_MACOS

    const QList<BitTorrent::Torrent *> allTorrents = BitTorrent::Session::instance()->torrents();
    const bool hasActiveTorrents = std::ranges::any_of(allTorrents, [](const BitTorrent::Torrent *torrent)
    {
        return torrent->isActive();
    });
    if (pref->confirmOnExit() && hasActiveTorrents)
    {
        if (e->spontaneous() || m_forceExit)
        {
            if (!isVisible())
                show();
            QMessageBox confirmBox(QMessageBox::Question, tr("Exiting qBittorrent"),
                                   // Split it because the last sentence is used in the WebUI
                                   tr("Some files are currently transferring.") + u'\n' + tr("Are you sure you want to quit qBittorrent?"),
                                   QMessageBox::NoButton, this);
            QPushButton *noBtn = confirmBox.addButton(tr("&No"), QMessageBox::NoRole);
            confirmBox.addButton(tr("&Yes"), QMessageBox::YesRole);
            QPushButton *alwaysBtn = confirmBox.addButton(tr("&Always Yes"), QMessageBox::YesRole);
            confirmBox.setDefaultButton(noBtn);
            confirmBox.exec();
            if (!confirmBox.clickedButton() || (confirmBox.clickedButton() == noBtn))
            {
                // Cancel exit
                e->ignore();
                m_forceExit = false;
                return;
            }
            if (confirmBox.clickedButton() == alwaysBtn)
                // Remember choice
                Preferences::instance()->setConfirmOnExit(false);
        }
    }

    // Accept exit
    e->accept();
    qApp->exit();
}

// Display window to create a torrent
void MainWindow::on_actionCreateTorrent_triggered()
{
    createTorrentTriggered({});
}

void MainWindow::createTorrentTriggered(const Path &path)
{
    if (m_createTorrentDlg)
    {
        m_createTorrentDlg->updateInputPath(path);
        m_createTorrentDlg->activateWindow();
    }
    else
    {
        m_createTorrentDlg = new TorrentCreatorDialog(this, path);
        m_createTorrentDlg->setAttribute(Qt::WA_DeleteOnClose);
        m_createTorrentDlg->show();
    }
}

bool MainWindow::event(QEvent *e)
{
#ifndef Q_OS_MACOS
    switch (e->type())
    {
    case QEvent::WindowStateChange:
        qDebug("Window change event");
        // Now check to see if the window is minimised
        if (isMinimized())
        {
            qDebug("minimisation");
            Preferences *const pref = Preferences::instance();
            if (app()->desktopIntegration()->isActive() && pref->minimizeToTray())
            {
                qDebug() << "Has active window:" << (qApp->activeWindow() != nullptr);
                // Check if there is a modal window
                const QWidgetList allWidgets = QApplication::allWidgets();
                const bool hasModalWindow = std::ranges::any_of(allWidgets
                    , [](const QWidget *widget) { return widget->isModal(); });
                // Iconify if there is no modal window
                if (!hasModalWindow)
                {
                    qDebug("Minimize to Tray enabled, hiding!");
                    e->ignore();
                    QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
                    if (!pref->minimizeToTrayNotified())
                    {
                        app()->desktopIntegration()->showNotification(tr("qBittorrent is minimized to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
                        pref->setMinimizeToTrayNotified(true);
                    }
                    return true;
                }
            }
        }
        break;
    case QEvent::ToolBarChange:
        {
            qDebug("MAC: Received a toolbar change event!");
            const bool ret = QMainWindow::event(e);

            qDebug("MAC: new toolbar visibility is %d", !m_ui->actionTopToolBar->isChecked());
            m_ui->actionTopToolBar->toggle();
            Preferences::instance()->setToolbarDisplayed(m_ui->actionTopToolBar->isChecked());
            return ret;
        }
    default:
        break;
    }
#endif // Q_OS_MACOS

    return QMainWindow::event(e);
}

// Display a dialog to allow user to add
// torrents to download list
void MainWindow::on_actionOpen_triggered()
{
    Preferences *const pref = Preferences::instance();
    // Open File Open Dialog
    // Note: it is possible to select more than one file
    const QStringList pathsList = QFileDialog::getOpenFileNames(this, tr("Open Torrent Files")
            , pref->getMainLastDir().data(),  tr("Torrent Files") + u" (*" + TORRENT_FILE_EXTENSION + u')');

    if (pathsList.isEmpty())
        return;

    for (const QString &file : pathsList)
        app()->addTorrentManager()->addTorrent(file);

    // Save last dir to remember it
    const Path topDir {pathsList.at(0)};
    const Path parentDir = topDir.parentPath();
    pref->setMainLastDir(parentDir.isEmpty() ? topDir : parentDir);
}

void MainWindow::activate()
{
    if (!m_uiLocked || unlockUI())
    {
        show();
        activateWindow();
        raise();
    }
}

void MainWindow::optionsSaved()
{
    LogMsg(tr("Options saved."));
    loadPreferences();
}

void MainWindow::showStatusBar(bool show)
{
    if (!show)
    {
        // Remove status bar
        setStatusBar(nullptr);
    }
    else if (!m_statusBar)
    {
        // Create status bar
        m_statusBar = new StatusBar;
        connect(m_statusBar.data(), &StatusBar::connectionButtonClicked, this, &MainWindow::showConnectionSettings);
        connect(m_statusBar.data(), &StatusBar::alternativeSpeedsButtonClicked, this, &MainWindow::toggleAlternativeSpeeds);
        setStatusBar(m_statusBar);
    }
}

void MainWindow::showFiltersSidebar(const bool show)
{
    if (show && !m_transferListFiltersWidget)
    {
        m_transferListFiltersWidget = new TransferListFiltersWidget(m_splitter, m_transferListWidget, isDownloadTrackerFavicon());
        applyFilterSectionVisibility();
        m_splitter->insertWidget(0, m_transferListFiltersWidget);
        m_splitter->setCollapsible(0, true);
        // From https://doc.qt.io/qt-5/qsplitter.html#setSizes:
        // Instead, any additional/missing space is distributed amongst the widgets
        // according to the relative weight of the sizes.
        m_splitter->setStretchFactor(0, 0);
        m_splitter->setStretchFactor(1, 1);
        m_splitter->setSizes({Preferences::instance()->getFiltersSidebarWidth()});
    }
    else if (!show && m_transferListFiltersWidget)
    {
        saveSplitterSettings();
        delete m_transferListFiltersWidget;
        m_transferListFiltersWidget = nullptr;
    }
}

void MainWindow::loadPreferences()
{
    const Preferences *pref = Preferences::instance();

    // General
    if (pref->isToolbarDisplayed())
    {
        m_ui->toolBar->setVisible(true);
    }
    else
    {
        // Clear search filter before hiding the top toolbar
        m_columnFilterEdit->clear();
        m_ui->toolBar->setVisible(false);
    }

    showStatusBar(pref->isStatusbarDisplayed());

    m_transferListWidget->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getFilesList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getTrackerList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getPeerList()->setAlternatingRowColors(pref->useAlternatingRowColors());

    // Queueing System
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled())
    {
        if (!m_ui->actionDecreaseQueuePos->isVisible())
        {
            m_transferListWidget->hideQueuePosColumn(false);
            m_ui->actionDecreaseQueuePos->setVisible(true);
            m_ui->actionIncreaseQueuePos->setVisible(true);
            m_ui->actionTopQueuePos->setVisible(true);
            m_ui->actionBottomQueuePos->setVisible(true);
            m_queueSeparator->setVisible(true);
            m_queueSeparatorMenu->setVisible(true);
        }
    }
    else
    {
        if (m_ui->actionDecreaseQueuePos->isVisible())
        {
            m_transferListWidget->hideQueuePosColumn(true);
            m_ui->actionDecreaseQueuePos->setVisible(false);
            m_ui->actionIncreaseQueuePos->setVisible(false);
            m_ui->actionTopQueuePos->setVisible(false);
            m_ui->actionBottomQueuePos->setVisible(false);
            m_queueSeparator->setVisible(false);
            m_queueSeparatorMenu->setVisible(false);
        }
    }

    // Torrent properties
    m_propertiesWidget->reloadPreferences();

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (pref->isUpdateCheckEnabled())
    {
        if (!m_programUpdateTimer)
        {
            m_programUpdateTimer = new QTimer(this);
            m_programUpdateTimer->setInterval(24h);
            m_programUpdateTimer->setSingleShot(true);
            connect(m_programUpdateTimer, &QTimer::timeout, this, [this]() { checkProgramUpdate(false); });
            m_programUpdateTimer->start();
        }
    }
    else
    {
        delete m_programUpdateTimer;
        m_programUpdateTimer = nullptr;
    }
#endif

#ifdef Q_OS_MACOS
    // Clear dock badge immediately if speed display is disabled
    if (!pref->isSpeedInDockEnabled())
        m_badger->updateSpeed(0, 0);
    m_statusItem->setVisible(pref->isMacOSMenuBarIconEnabled());
#endif

    qDebug("GUI settings loaded");
}

void MainWindow::loadSessionStats()
{
    const auto *btSession = BitTorrent::Session::instance();
    const BitTorrent::SessionStatus &status = btSession->status();
    m_downloadRate = Utils::Misc::friendlyUnit(status.payloadDownloadRate, true);
    m_uploadRate = Utils::Misc::friendlyUnit(status.payloadUploadRate, true);

    // update global information
#ifdef Q_OS_MACOS
    if (Preferences::instance()->isSpeedInDockEnabled())
        m_badger->updateSpeed(status.payloadDownloadRate, status.payloadUploadRate);
    if (Preferences::instance()->isMacOSMenuBarIconEnabled())
        m_statusItem->updateSpeed(status.payloadDownloadRate, status.payloadUploadRate);
#else
    refreshTrayIconTooltip();
#endif  // Q_OS_MACOS

    if (m_speedGraph)
    {
        // No disk write-rate metric is exposed, so approximate total disk I/O
        // as writes (~= downloads) + reads (~= uploads).
        const qint64 diskIO = status.payloadDownloadRate + status.payloadUploadRate;
        m_speedGraph->addSample(status.payloadDownloadRate, status.payloadUploadRate, diskIO);
    }

#ifdef Q_OS_WIN
    updateTaskbarProgress();
#endif

    refreshWindowTitle();
}

void MainWindow::reloadTorrentStats(const QList<BitTorrent::Torrent *> &torrents)
{
    if (currentTabWidget() == m_transferListWidget)
    {
        if (torrents.contains(m_propertiesWidget->getCurrentTorrent()))
            m_propertiesWidget->loadDynamicData();
    }
}

void MainWindow::downloadFromURLList(const QStringList &urlList)
{
    for (const QString &url : urlList)
        app()->addTorrentManager()->addTorrent(url);
}

void MainWindow::populateDesktopIntegrationMenu()
{
    auto *menu = app()->desktopIntegration()->menu();
    menu->clear();

#ifndef Q_OS_MACOS
    connect(menu, &QMenu::aboutToShow, this, [this]()
    {
        m_ui->actionToggleVisibility->setText(isVisible() ? tr("Hide") : tr("Show"));
    });
    connect(m_ui->actionToggleVisibility, &QAction::triggered, this, &MainWindow::toggleVisibility);

    menu->addAction(m_ui->actionToggleVisibility);
    menu->addSeparator();
#endif

    menu->addAction(m_ui->actionOpen);
    menu->addAction(m_ui->actionDownloadFromURL);
    menu->addSeparator();

    menu->addAction(m_ui->actionUseAlternativeSpeedLimits);
    menu->addAction(m_ui->actionSetGlobalSpeedLimits);
    menu->addSeparator();

    menu->addAction(m_ui->actionResumeSession);
    menu->addAction(m_ui->actionPauseSession);

#ifndef Q_OS_MACOS
    menu->addSeparator();
    menu->addAction(m_ui->actionExit);
#endif

    if (m_uiLocked)
        menu->setEnabled(false);
}

void MainWindow::updateAltSpeedsBtn(const bool alternative)
{
    m_ui->actionUseAlternativeSpeedLimits->setChecked(alternative);
}

PropertiesWidget *MainWindow::propertiesWidget() const
{
    return m_propertiesWidget;
}

// Display Program Options
void MainWindow::on_actionOptions_triggered()
{
    if (m_options)
    {
        m_options->activateWindow();
    }
    else
    {
        m_options = new OptionsDialog(app(), this);
        m_options->setAttribute(Qt::WA_DeleteOnClose);
        m_options->open();
    }
}

void MainWindow::on_actionTopToolBar_triggered()
{
    const bool isVisible = static_cast<QAction *>(sender())->isChecked();
    m_ui->toolBar->setVisible(isVisible);
    Preferences::instance()->setToolbarDisplayed(isVisible);
}

void MainWindow::on_actionShowStatusbar_triggered()
{
    const bool isVisible = static_cast<QAction *>(sender())->isChecked();
    Preferences::instance()->setStatusbarDisplayed(isVisible);
    showStatusBar(isVisible);
}

void MainWindow::on_actionShowFiltersSidebar_triggered(const bool checked)
{
    Preferences *const pref = Preferences::instance();
    pref->setFiltersSidebarVisible(checked);
    showFiltersSidebar(checked);

    // Sidebar and the filter comboboxes are two views of the same filters:
    // enabling the sidebar turns the combos mode off.
    if (checked && m_ui->actionFilterCombos->isChecked())
    {
        m_ui->actionFilterCombos->setChecked(false);
        m_storeFilterCombosMode = false;
        setFilterCombosMode(false);
    }
}

void MainWindow::on_actionSpeedInTitleBar_triggered()
{
    m_displaySpeedInTitle = static_cast<QAction *>(sender())->isChecked();
    Preferences::instance()->showSpeedInTitleBar(m_displaySpeedInTitle);
    refreshWindowTitle();
}

void MainWindow::on_actionCompactRows_triggered(const bool checked)
{
    m_storeCompactRows = checked;
    m_transferListWidget->setProperty("compact", checked);
    // Re-polish so the TransferListWidget[compact="true"] stylesheet rule and
    // the denser row padding take effect immediately.
    m_transferListWidget->style()->unpolish(m_transferListWidget);
    m_transferListWidget->style()->polish(m_transferListWidget);
    m_transferListWidget->update();
    m_transferListWidget->doItemsLayout();
}

void MainWindow::on_actionShowSpeedGraph_triggered(const bool checked)
{
    m_storeSpeedGraphVisible = checked;
    if (m_speedGraph)
    {
        m_speedGraph->setVisible(checked);
        positionSpeedGraph();
    }
}

void MainWindow::on_actionShowFilterBar_triggered(const bool checked)
{
    m_storeFilterBarVisible = checked;
    if (m_filterBar)
        m_filterBar->setVisible(checked);
}

void MainWindow::on_actionShowDetailsPanel_triggered(const bool checked)
{
    m_propertiesWidget->setVisibility(checked);
}

void MainWindow::on_actionShowTabBar_triggered(const bool checked)
{
    m_storeTabBarVisible = checked;
    static_cast<HidableTabWidget *>(m_tabs.data())->setTabBarForcedHidden(!checked);
}

void MainWindow::applyFilterSectionVisibility()
{
    if (!m_transferListFiltersWidget)
        return;

    using Section = TransferListFiltersWidget::Section;
    m_transferListFiltersWidget->setSectionVisible(Section::Status, m_storeStatusSectionVisible.get(true));
    m_transferListFiltersWidget->setSectionVisible(Section::Categories, m_storeCategoriesSectionVisible.get(true));
    m_transferListFiltersWidget->setSectionVisible(Section::Tags, m_storeTagsSectionVisible.get(true));
    m_transferListFiltersWidget->setSectionVisible(Section::Trackers, m_storeTrackersSectionVisible.get(true));
}

void MainWindow::on_actionShowStatusFilter_triggered(const bool checked)
{
    m_storeStatusSectionVisible = checked;
    if (m_transferListFiltersWidget)
        m_transferListFiltersWidget->setSectionVisible(TransferListFiltersWidget::Section::Status, checked);
}

void MainWindow::on_actionShowCategoriesFilter_triggered(const bool checked)
{
    m_storeCategoriesSectionVisible = checked;
    if (m_transferListFiltersWidget)
        m_transferListFiltersWidget->setSectionVisible(TransferListFiltersWidget::Section::Categories, checked);
}

void MainWindow::on_actionShowTagsFilter_triggered(const bool checked)
{
    m_storeTagsSectionVisible = checked;
    if (m_transferListFiltersWidget)
        m_transferListFiltersWidget->setSectionVisible(TransferListFiltersWidget::Section::Tags, checked);
}

void MainWindow::on_actionShowTrackersFilter_triggered(const bool checked)
{
    m_storeTrackersSectionVisible = checked;
    if (m_transferListFiltersWidget)
        m_transferListFiltersWidget->setSectionVisible(TransferListFiltersWidget::Section::Trackers, checked);
}

void MainWindow::onStatusFilterChanged(const int index)
{
    if (!m_statusFilterComboBox)
        return;
    m_transferListWidget->applyStatusFilter(m_statusFilterComboBox->itemData(index).toInt());
}

void MainWindow::onCategoryFilterChanged([[maybe_unused]] const int index)
{
    const QVariant data = m_categoryFilterComboBox->currentData();
    m_transferListWidget->applyCategoryFilter(data.isValid() ? data.toString() : QString());
}

void MainWindow::onTagFilterChanged([[maybe_unused]] const int index)
{
    const QVariant data = m_tagFilterComboBox->currentData();
    m_transferListWidget->applyTagFilter(data.isValid() ? std::optional<Tag> {Tag(data.toString())} : std::nullopt);
}

void MainWindow::refreshCategoryComboBox()
{
    if (!m_categoryFilterComboBox)
        return;

    const QVariant current = m_categoryFilterComboBox->currentData();
    m_categoryFilterComboBox->blockSignals(true);
    m_categoryFilterComboBox->clear();
    m_categoryFilterComboBox->addItem(tr("All Categories"), QVariant());   // invalid → disable filter
    m_categoryFilterComboBox->addItem(tr("Uncategorized"), QString(u""_s)); // empty non-null → uncategorized

    for (const QString &cat : asConst(BitTorrent::Session::instance()->categories()))
        m_categoryFilterComboBox->addItem(cat, cat);

    const int idx = current.isValid() ? m_categoryFilterComboBox->findData(current) : -1;
    m_categoryFilterComboBox->setCurrentIndex(idx >= 0 ? idx : 0);
    m_categoryFilterComboBox->blockSignals(false);
}

void MainWindow::refreshTagComboBox()
{
    if (!m_tagFilterComboBox)
        return;

    const QVariant current = m_tagFilterComboBox->currentData();
    m_tagFilterComboBox->blockSignals(true);
    m_tagFilterComboBox->clear();
    m_tagFilterComboBox->addItem(tr("All Tags"), QVariant());

    for (const Tag &tag : asConst(BitTorrent::Session::instance()->tags()))
        m_tagFilterComboBox->addItem(tag.toString(), tag.toString());

    const int idx = current.isValid() ? m_tagFilterComboBox->findData(current) : -1;
    m_tagFilterComboBox->setCurrentIndex(idx >= 0 ? idx : 0);
    m_tagFilterComboBox->blockSignals(false);
}

void MainWindow::setFilterCombosMode(const bool enabled)
{
    if (m_statusFilterComboBox)
        m_statusFilterComboBox->setVisible(enabled);
    if (m_categoryFilterComboBox)
        m_categoryFilterComboBox->setVisible(enabled);
    if (m_tagFilterComboBox)
        m_tagFilterComboBox->setVisible(enabled);

    if (enabled)
    {
        // Combos replace the left sidebar: hide it, then apply the selections.
        if (m_ui->actionShowFiltersSidebar->isChecked())
        {
            m_ui->actionShowFiltersSidebar->setChecked(false);
            Preferences::instance()->setFiltersSidebarVisible(false);
            showFiltersSidebar(false);
        }
        onStatusFilterChanged(m_statusFilterComboBox->currentIndex());
        onCategoryFilterChanged(m_categoryFilterComboBox->currentIndex());
        onTagFilterChanged(m_tagFilterComboBox->currentIndex());
    }
}

void MainWindow::on_actionFilterCombos_triggered(const bool checked)
{
    m_storeFilterCombosMode = checked;
    setFilterCombosMode(checked);
    if (!checked)
    {
        // Clear any filtering the combos applied so the state is clean again.
        m_transferListWidget->applyStatusFilter(TorrentFilter::All);
        m_transferListWidget->applyCategoryFilter(QString());
        m_transferListWidget->applyTagFilter(std::nullopt);
    }
}

void MainWindow::positionSpeedGraph()
{
    if (!m_speedGraph || !m_transferListWidget)
        return;

    const int margin = 10;
    const int x = m_transferListWidget->width() - m_speedGraph->width() - margin;
    const int y = m_transferListWidget->height() - m_speedGraph->height() - margin;
    m_speedGraph->move(std::max(0, x), std::max(0, y));
    m_speedGraph->raise();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_transferListWidget)
        && ((event->type() == QEvent::Resize) || (event->type() == QEvent::Show)))
    {
        positionSpeedGraph();
    }
    return GUIApplicationComponent<QMainWindow>::eventFilter(watched, event);
}

void MainWindow::on_actionRSSReader_triggered()
{
    Preferences::instance()->setRSSWidgetVisible(m_ui->actionRSSReader->isChecked());
    displayRSSTab(m_ui->actionRSSReader->isChecked());
}

void MainWindow::on_actionSearchWidget_triggered()
{
    if (m_ui->actionSearchWidget->isChecked())
    {
        const Utils::ForeignApps::PythonInfo pyInfo = Utils::ForeignApps::pythonInfo();

        // Not found
        if (!pyInfo.isValid())
        {
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);

#ifdef Q_OS_WIN
            const QMessageBox::StandardButton buttonPressed = QMessageBox::question(this, tr("Missing Python Runtime")
                , tr("Python is required to use the search engine but it does not seem to be installed.\nDo you want to install it now?")
                , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
            if (buttonPressed == QMessageBox::Yes)
                installPython();
#else
            QMessageBox::information(this, tr("Missing Python Runtime")
                , tr("Python is required to use the search engine but it does not seem to be installed."));
#endif
            return;
        }

        // Check version requirement
        if (!pyInfo.isSupportedVersion())
        {
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);

#ifdef Q_OS_WIN
            const QMessageBox::StandardButton buttonPressed = QMessageBox::question(this, tr("Old Python Runtime")
                , tr("Your Python version (%1) is outdated. Minimum requirement: %2.\nDo you want to install a newer version now?")
                    .arg(pyInfo.version.toString(), Utils::ForeignApps::PythonInfo::MINIMUM_SUPPORTED_VERSION.toString())
                , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
            if (buttonPressed == QMessageBox::Yes)
                installPython();
#else
            QMessageBox::information(this, tr("Old Python Runtime")
                , tr("Your Python version (%1) is outdated. Please upgrade to latest version for search engines to work.\nMinimum requirement: %2.")
                .arg(pyInfo.version.toString(), Utils::ForeignApps::PythonInfo::MINIMUM_SUPPORTED_VERSION.toString()));
#endif
            return;
        }

        m_ui->actionSearchWidget->setChecked(true);
        Preferences::instance()->setSearchEnabled(true);
    }

    displaySearchTab(m_ui->actionSearchWidget->isChecked());
}

// Display an input dialog to prompt user for
// an url
void MainWindow::on_actionDownloadFromURL_triggered()
{
    if (!m_downloadFromURLDialog)
    {
        m_downloadFromURLDialog = new DownloadFromURLDialog(this);
        m_downloadFromURLDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_downloadFromURLDialog.data(), &DownloadFromURLDialog::urlsReadyToBeDownloaded, this, &MainWindow::downloadFromURLList);
        m_downloadFromURLDialog->open();
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void MainWindow::handleUpdateCheckFinished(ProgramUpdater *updater, const bool invokedByUser)
{
    m_ui->actionCheckForUpdates->setEnabled(true);
    m_ui->actionCheckForUpdates->setText(tr("&Check for Updates"));
    m_ui->actionCheckForUpdates->setToolTip(tr("Check for program updates"));

    const auto cleanup = [this, updater]()
    {
        if (m_programUpdateTimer)
            m_programUpdateTimer->start();
        updater->deleteLater();
    };

    const ProgramUpdater::Version newVersion = updater->getNewVersion();
    if (newVersion.isValid())
    {
        const QString msg {tr("A new version is available.") + u"<br/>"
            + tr("Do you want to download %1?").arg(newVersion.toString()) + u"<br/><br/>"
            + u"<a href=\"https://www.qbittorrent.org/news\">%1</a>"_s.arg(tr("Open changelog..."))};
        auto *msgBox = new QMessageBox {QMessageBox::Question, tr("qBittorrent Update Available"), msg
            , (QMessageBox::Yes | QMessageBox::No), this};
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->setAttribute(Qt::WA_ShowWithoutActivating);
        msgBox->setDefaultButton(QMessageBox::Yes);
        msgBox->setWindowModality(Qt::NonModal);
        connect(msgBox, &QMessageBox::buttonClicked, this, [msgBox, updater](QAbstractButton *button)
        {
            if (msgBox->buttonRole(button) == QMessageBox::YesRole)
            {
                updater->updateProgram();
            }
        });
        connect(msgBox, &QDialog::finished, this, cleanup);
        msgBox->show();
    }
    else
    {
        if (invokedByUser)
        {
            auto *msgBox = new QMessageBox {QMessageBox::Information, u"qBittorrent"_s
                , tr("No updates available.\nYou are already using the latest version.")
                , QMessageBox::Ok, this};
            msgBox->setAttribute(Qt::WA_DeleteOnClose);
            msgBox->setWindowModality(Qt::NonModal);
            connect(msgBox, &QDialog::finished, this, cleanup);
            msgBox->show();
        }
        else
        {
            cleanup();
        }
    }
}
#endif

void MainWindow::toggleAlternativeSpeeds()
{
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    session->setAltGlobalSpeedLimitEnabled(!session->isAltGlobalSpeedLimitEnabled());
}

void MainWindow::on_actionDonateMoney_triggered()
{
    QDesktopServices::openUrl(QUrl(u"https://www.qbittorrent.org/donate"_s));
}

void MainWindow::showConnectionSettings()
{
    on_actionOptions_triggered();
    m_options->showConnectionTab();
}

void MainWindow::minimizeWindow()
{
    setWindowState(windowState() | Qt::WindowMinimized);
}

void MainWindow::on_actionExecutionLogs_triggered(bool checked)
{
    if (checked)
    {
        Q_ASSERT(!m_executionLog);
        m_executionLog = new ExecutionLogWidget(executionLogMsgTypes(), m_tabs);
#ifdef Q_OS_MACOS
        m_tabs->addTab(m_executionLog, tr("Execution Log"));
#else
        const int indexTab = m_tabs->addTab(m_executionLog, tr("Execution Log"));
        m_tabs->setTabIcon(indexTab, UIThemeManager::instance()->getIcon(u"help-contents"_s));
#endif
    }
    else
    {
        delete m_executionLog;
    }

    m_ui->actionNormalMessages->setEnabled(checked);
    m_ui->actionInformationMessages->setEnabled(checked);
    m_ui->actionWarningMessages->setEnabled(checked);
    m_ui->actionCriticalMessages->setEnabled(checked);
    setExecutionLogEnabled(checked);
}

void MainWindow::on_actionNormalMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::NORMAL, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionInformationMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::INFO, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionWarningMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::WARNING, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionCriticalMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::CRITICAL, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionAutoExit_toggled(const bool enabled)
{
    Preferences::instance()->setShutdownqBTWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoSuspend_toggled(const bool enabled)
{
    Preferences::instance()->setSuspendWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoHibernate_toggled(const bool enabled)
{
    Preferences::instance()->setHibernateWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoShutdown_toggled(const bool enabled)
{
    Preferences::instance()->setShutdownWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoReboot_toggled(const bool enabled)
{
    Preferences::instance()->setRebootWhenDownloadsComplete(enabled);
}

void MainWindow::updatePowerManagementState() const
{
    const auto *pref = Preferences::instance();
    const bool preventFromSuspendWhenDownloading = pref->preventFromSuspendWhenDownloading();
    const bool preventFromSuspendWhenSeeding = pref->preventFromSuspendWhenSeeding();

    const QList<BitTorrent::Torrent *> allTorrents = BitTorrent::Session::instance()->torrents();
    const bool inhibitSuspend = std::ranges::any_of(allTorrents, [&](const BitTorrent::Torrent *torrent)
    {
        if (preventFromSuspendWhenDownloading && (!torrent->isFinished() && !torrent->isStopped() && !torrent->isErrored() && torrent->hasMetadata()))
            return true;

        if (preventFromSuspendWhenSeeding && (torrent->isFinished() && !torrent->isStopped()))
            return true;

        return torrent->isMoving();
    });
    m_pwr->setActivityState(inhibitSuspend ? PowerManagement::ActivityState::Busy : PowerManagement::ActivityState::Idle);

    m_preventTimer->start(PREVENT_SUSPEND_INTERVAL);
}

void MainWindow::applyTransferListFilter()
{
    m_transferListWidget->applyFilter(m_columnFilterEdit->text(), m_columnFilterComboBox->currentData().value<TransferListModel::Column>());
}

void MainWindow::refreshWindowTitle()
{
    const auto *btSession = BitTorrent::Session::instance();
    if (btSession->isPaused())
    {
        const QString title = tr("[PAUSED] %1", "%1 is the rest of the window title").arg(m_windowTitle);
        setWindowTitle(title);
    }
    else
    {
        if (m_displaySpeedInTitle)
        {
            const QString title = tr("[D: %1, U: %2] %3", "D = Download; U = Upload; %3 is the rest of the window title")
                    .arg(m_downloadRate, m_uploadRate, m_windowTitle);
            setWindowTitle(title);
        }
        else
        {
            setWindowTitle(m_windowTitle);
        }
    }

#ifdef Q_OS_WIN
    // The custom title bar shows the app-menu button, not the OS window title,
    // so reflect the "speed in title bar" option there to make it visible.
    if (m_appMenuButton)
    {
        if (!btSession->isPaused() && m_displaySpeedInTitle)
            m_appMenuButton->setText(tr("   qBittorrent   ↓ %1   ↑ %2").arg(m_downloadRate, m_uploadRate));
        else
            m_appMenuButton->setText(u"   qBittorrent"_s);
    }
#endif
}

void MainWindow::refreshTrayIconTooltip()
{
    const auto *btSession = BitTorrent::Session::instance();
    if (!btSession->isPaused())
    {
        const auto toolTip = u"%1\n%2"_s.arg(
                tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(m_downloadRate)
                , tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(m_uploadRate));
        app()->desktopIntegration()->setToolTip(toolTip);
    }
    else
    {
        app()->desktopIntegration()->setToolTip(tr("Paused"));
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void MainWindow::checkProgramUpdate(const bool invokedByUser)
{
    if (m_programUpdateTimer)
        m_programUpdateTimer->stop();

    m_ui->actionCheckForUpdates->setEnabled(false);
    m_ui->actionCheckForUpdates->setText(tr("Checking for Updates..."));
    m_ui->actionCheckForUpdates->setToolTip(tr("Already checking for program updates in the background"));

    auto *updater = new ProgramUpdater(this);
    connect(updater, &ProgramUpdater::updateCheckFinished
        , this, [this, invokedByUser, updater]()
    {
        handleUpdateCheckFinished(updater, invokedByUser);
    });
    updater->checkForUpdates();
}
#endif

#ifdef Q_OS_WIN
void MainWindow::installPython()
{
    m_ui->actionSearchWidget->setEnabled(false);
    m_ui->actionSearchWidget->setToolTip(tr("Python installation in progress..."));
    setCursor(Qt::WaitCursor);
    // Download python
    Net::DownloadManager::instance()->download(
            Net::DownloadRequest(PYTHON_INSTALLER_URL).saveToFile(true)
            , Preferences::instance()->useProxyForGeneralPurposes()
            , this, &MainWindow::pythonDownloadFinished);
}

bool MainWindow::verifyPythonInstaller(const Path &installerPath) const
{
    // Verify installer hash

    QFile file {installerPath.data()};
    if (!file.open(QIODevice::ReadOnly))
    {
        LogMsg((tr("Failed to open Python installer. File: \"%1\".").arg(installerPath.toString())), Log::WARNING);
        return false;
    }

    QCryptographicHash sha2Hash {QCryptographicHash::Sha256};
    sha2Hash.addData(&file);
    if (const QByteArray hashHex = sha2Hash.result().toHex(); hashHex != PYTHON_INSTALLER_SHA2_256)
    {
        LogMsg((tr("Failed SHA2-256 hash check for Python installer. File: \"%1\". Result hash: \"%2\". Expected hash: \"%3\".")
                .arg(installerPath.toString(), QString::fromLatin1(hashHex), QString::fromLatin1(PYTHON_INSTALLER_SHA2_256)))
            , Log::WARNING);
        return false;
    }

    return true;
}

void MainWindow::pythonDownloadFinished(const Net::DownloadResult &result)
{
    auto restoreWidgetsGuard = qScopeGuard([this]
    {
        m_ui->actionSearchWidget->setEnabled(true);
        m_ui->actionSearchWidget->setToolTip({});
        setCursor(Qt::ArrowCursor);
    });

    if (result.status != Net::DownloadStatus::Success)
    {
        QMessageBox::warning(
                    this, tr("Download error")
                    , tr("Python installer could not be downloaded. Error: %1.\nPlease install it manually.")
                    .arg(result.errorString));
        return;
    }

    const Path exePath = result.filePath + u".exe";
    if (!Utils::Fs::renameFile(result.filePath, exePath))
    {
        LogMsg(tr("Rename Python installer failed. Source: \"%1\". Destination: \"%2\".")
                .arg(result.filePath.toString(), exePath.toString())
            , Log::WARNING);
        return;
    }

    if (!verifyPythonInstaller(exePath))
        return;

    // launch installer
    auto *installer = new QProcess(this);
    installer->connect(installer, &QProcess::finished, this, [this, exePath, installer, restoreWidgetsGuard = std::move(restoreWidgetsGuard)](const int exitCode, const QProcess::ExitStatus exitStatus)
    {
        installer->deleteLater();

        if ((exitStatus == QProcess::NormalExit) && (exitCode == 0))
        {
            LogMsg(tr("Python installation success."), Log::INFO);

            // Delete installer
            std::ignore = Utils::Fs::removeFile(exePath);

            // Reload search engine
            if (Utils::ForeignApps::pythonInfo().isSupportedVersion())
            {
                m_ui->actionSearchWidget->setChecked(true);
                displaySearchTab(true);
            }
        }
        else
        {
            const QString errorInfo = (exitStatus == QProcess::NormalExit)
                ? tr("Exit code: %1.").arg(QString::number(exitCode))
                : tr("Reason: installer crashed.");
            LogMsg(u"%1 %2"_s.arg(tr("Python installation failed."), errorInfo), Log::WARNING);
        }
    });
    LogMsg(tr("Launching Python installer. File: \"%1\".").arg(exePath.toString()), Log::INFO);
    installer->start(exePath.toString(), {u"/passive"_s});
}
#endif // Q_OS_WIN

#ifdef ENABLE_PLUGINS
void MainWindow::populatePluginsMenu()
{
    auto *pluginsEngine = PluginsEngine::instance();

    for (const PluginInfo &pluginInfo : asConst(pluginsEngine->allPlugins()))
    {
        if (pluginInfo.invocable && pluginInfo.enabled)
            addPluginsMenuItem(pluginInfo);
    }

    connect(pluginsEngine, &PluginsEngine::pluginInstalled, this
            , [this](const Path &, const PluginInfo &pluginInfo)
    {
        if (pluginInfo.invocable && pluginInfo.enabled)
            addPluginsMenuItem(pluginInfo);
    });

    connect(pluginsEngine, &PluginsEngine::pluginUninstalled, this
            , [this](const QString &pluginID)
    {
        removePluginsMenuItem(pluginID);
    });

    connect(pluginsEngine, &PluginsEngine::pluginUpdated, this
            , [this](const Path &, const PluginInfo &oldPluginInfo, const PluginInfo &newPluginInfo)
    {
        if (newPluginInfo.enabled && newPluginInfo.invocable)
        {
            if (QAction *action = m_pluginActions.value(oldPluginInfo.id))
                action->setText(newPluginInfo.name);
            else
                addPluginsMenuItem(newPluginInfo);
        }
        else
        {
            removePluginsMenuItem(oldPluginInfo.id);
        }
    });

    connect(pluginsEngine, &PluginsEngine::pluginEnabledChanged, this
            , [this, pluginsEngine](const QString &pluginID, bool isEnabled)
    {
        if (isEnabled)
        {
            if (const auto pluginInfo = pluginsEngine->pluginInfo(pluginID);
                    pluginInfo && pluginInfo->invocable)
            {
                addPluginsMenuItem(*pluginInfo);
            }
        }
        else
        {
            removePluginsMenuItem(pluginID);
        }
    });
}

void MainWindow::addPluginsMenuItem(const PluginInfo &pluginInfo)
{
    auto *pluginsEngine = PluginsEngine::instance();
    auto *action = m_ui->menuPlugins->addAction(pluginInfo.name, this
            , [pluginsEngine, pluginID = pluginInfo.id]
    {
        pluginsEngine->invokePlugin(pluginID);
    });
    m_pluginActions.insert(pluginInfo.id, action);
}

void MainWindow::removePluginsMenuItem(const QString &pluginID)
{
    if (QAction *action = m_pluginActions.take(pluginID))
    {
        m_ui->menuPlugins->removeAction(action);
        delete action;
    }
}

void MainWindow::managePlugins()
{
    auto *pluginsDialog = new PluginsDialog(PluginsEngine::instance(), this);
    pluginsDialog->setAttribute(Qt::WA_DeleteOnClose);
    pluginsDialog->open();
}
#endif
