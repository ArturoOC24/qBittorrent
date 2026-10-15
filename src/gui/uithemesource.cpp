/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2019, 2021  Prince Gupta <jagannatharjun11@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#include "uithemesource.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "base/logger.h"
#include "base/profile.h"
#include "base/utils/io.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    const qint64 FILE_MAX_SIZE = 1024 * 1024;

    QJsonObject parseThemeConfig(const QByteArray &data)
    {
        if (data.isEmpty())
            return {};

        QJsonParseError jsonError;
        const QJsonDocument configJsonDoc = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
        {
            LogMsg(UIThemeSource::tr("Couldn't parse UI Theme configuration file. Reason: %1")
                   .arg(jsonError.errorString()), Log::WARNING);
            return {};
        }

        if (!configJsonDoc.isObject())
        {
            LogMsg(UIThemeSource::tr("UI Theme configuration file has invalid format. Reason: %1")
                   .arg(UIThemeSource::tr("Root JSON value is not an object")), Log::WARNING);
            return {};
        }

        return configJsonDoc.object();
    }

    QHash<QString, QColor> colorsFromJSON(const QJsonObject &jsonObj)
    {
        QHash<QString, QColor> colors;
        for (auto colorNode = jsonObj.constBegin(); colorNode != jsonObj.constEnd(); ++colorNode)
        {
            const QColor color {colorNode.value().toString()};
            if (!color.isValid())
            {
                LogMsg(UIThemeSource::tr("Invalid color for ID \"%1\" is provided by theme")
                       .arg(colorNode.key()), Log::WARNING);
                continue;
            }

            colors.insert(colorNode.key(), color);
        }

        return colors;
    }

    Path findIcon(const QString &iconId, const Path &dir)
    {
        const Path pathSvg = dir / Path(iconId + u".svg");
        if (pathSvg.exists())
            return pathSvg;

        const Path pathPng = dir / Path(iconId + u".png");
        if (pathPng.exists())
            return pathPng;

        return {};
    }
}

DefaultThemeSource::DefaultThemeSource()
    : m_defaultPath {u":"_s}
    , m_userPath {specialFolderLocation(SpecialFolder::Config) / Path(u"themes/default"_s)}
    , m_colors {defaultUIThemeColors()}
{
    loadColors();
    // Palette isn't customizable in default theme
    m_colors.insert(defaultPaletteColors());
}

QByteArray DefaultThemeSource::readStyleSheet()
{
    // Dark-only modern look (ported from the Modern UI fork).
    //   #151C26 toolbar / title bar        #21262D content background
    //   #2C3138 panels / buttons           #1A2030 inputs / table headers
    //   #3D4450 borders                    #E6EDF3 primary text
    //   #2F81F7 accent                     #388bfd progress chunk
    // %1 = hover overlay, %2 = pressed overlay.
    const QString qss = QStringLiteral(
        "* { outline: 0; }"

        "QMainWindow { border: none; background: #151C26; }"
        "QMainWindow::separator { width: 0; height: 0; background: transparent; }"


        "QDialog { background: #21262D; }"

        "QToolBar { background: #151C26; border: none; margin: 0; spacing: 2px; padding: 4px 2px 0 2px; }"
        "QToolBar::separator { height: 1px; background: rgba(255,255,255,0.08); margin: 2px 6px; }"
        "QToolBar::handle { background: transparent; width: 0; height: 0; }"
        "QToolButton { border: none; border-radius: 4px; padding: 6px; background: transparent; outline: none; }"
        "QToolButton:hover   { background: %1; }"
        "QToolButton:pressed { background: %2; }"
        "QToolButton:focus   { border: none; outline: none; }"

        "QMenuBar { background: transparent; border: none; }"
        "QMenuBar::item { padding: 4px 8px; border-radius: 3px; }"
        "QMenuBar::item:selected { background: %1; }"
        "QMenu { background: #21262D; border: 1px solid #3D4450; border-radius: 4px; padding: 2px; }"
        "QMenu::item { padding: 5px 24px 5px 12px; border-radius: 3px; }"
        "QMenu::item:selected { background: #2F81F7; color: #FFFFFF; }"
        "QMenu::separator { height: 1px; background: #3D4450; margin: 3px 6px; }"

        "QTabWidget::pane { background: #21262D; border: none; }"

        // Main content: the transfers page paints the dark frame color; the
        // filter bar sits at its top-left and carries the rounded corner.
        "#transfersPage { background: #151C26; }"
        "#filterBar { background: #21262D; border-top-left-radius: 8px; }"

        // When the filter bar is hidden the torrent list becomes the top-left
        // widget and takes over the rounded corner (set via dynamic property).
        "TransferListWidget[filterBarHidden=\"true\"] { border-top-left-radius: 8px; }"
        "TransferListWidget[filterBarHidden=\"true\"] QHeaderView { background: transparent; }"
        "TransferListWidget[filterBarHidden=\"true\"] QHeaderView::section:first { border-top-left-radius: 8px; }"

        "QTabBar::tab { padding: 6px 16px; border: none; border-bottom: 2px solid transparent; background: transparent; }"
        "QTabBar::tab:selected { border-bottom: 2px solid #2F81F7; color: #2F81F7; }"
        "QTabBar::tab:hover:!selected { background: %1; border-top-left-radius: 4px; border-top-right-radius: 4px; }"

        "QProgressBar { border: none; border-radius: 3px; background: #3D4450;"
        "  text-align: center; max-height: 8px; min-height: 8px; }"
        "QProgressBar::chunk { border-radius: 3px; background: #388bfd; }"

        "QTreeView, QListView { border: none; background: #21262D; alternate-background-color: transparent; show-decoration-selected: 1; }"
        // The properties panel is a rounded card floating over the table
        // background; the panel widget itself matches the table color.
        "#propertiesPanel { background: #21262D; }"
        "#propertiesPanel QTabWidget::pane { background: #2C3138; border: none; border-radius: 8px; padding: 8px; }"
        "#propertiesPanel QTabWidget QStackedWidget { background: #2C3138; }"
        "#propertiesPanel QTabWidget QStackedWidget > QWidget { background: #2C3138; }"
        "#propertiesPanel QScrollArea { background: #2C3138; border: none; }"
        "#propertiesPanel QScrollArea > QWidget > QWidget { background: #2C3138; }"
        "#propertiesPanel QFrame { background: #2C3138; }"
        // QLabel derives from QFrame: keep labels transparent so they don't
        // paint stripes over the card backgrounds
        "#propertiesPanel QLabel { background: transparent; }"
        // Card-style group boxes in the General tab
        "#propertiesPanel QGroupBox { background: #262C35; border: none; border-radius: 8px;"
        "  margin-top: 24px; padding: 10px 6px 6px 6px; }"
        "#propertiesPanel QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left;"
        "  left: 4px; top: 4px; color: #8B949E; font-weight: 600; }"
        "#propertiesPanel QTreeView, #propertiesPanel QListView { background: #2C3138; alternate-background-color: transparent; }"
        "#propertiesPanel QAbstractItemView { background: #2C3138; alternate-background-color: transparent; }"
        "#propertiesPanel QHeaderView { background: #2C3138; }"
        "#propertiesPanel QHeaderView::section { background: #2C3138; color: rgba(255,255,255,0.7); border: none; border-right: 1px solid #3D4450; border-bottom: 1px solid #3D4450; padding: 4px 6px; }"
        "#propertiesPanel QHeaderView::section:last { border-right: none; }"
        "#propertiesPanel QTabWidget::tab-bar { left: 12px; }"
        "#propertiesPanel QTabBar { background: transparent; }"
        "#propertiesPanel QTabBar::tab { background: #1E242E; color: rgba(255,255,255,0.5); padding: 6px 16px; border: none; margin: 0 2px 8px 0;"
        "  border-bottom-left-radius: 6px; border-bottom-right-radius: 6px; }"
        "#propertiesPanel QTabBar::tab:selected { background: #2C3138; color: #E6EDF3; }"
        "#propertiesPanel QTabBar::tab:hover:!selected { background: #252B38; color: rgba(255,255,255,0.7); }"
        "QTreeView::item, QListView::item { padding: 4px 0; border: none; }"
        "QTreeView::item:hover:!selected, QListView::item:hover:!selected { background: rgba(255,255,255,0.05); }"
        "QTreeView::item:selected, QListView::item:selected { background: rgba(47,129,247,0.30); }"

        // Transfer list: taller 'card-like' rows with a subtle divider and an
        // accent left edge on the selected row.
        "TransferListWidget::item { padding: 8px 0; border-bottom: 1px solid rgba(255,255,255,0.04); }"
        "TransferListWidget[compact=\"true\"]::item { padding: 2px 0; border-bottom: none; }"
        "TransferListWidget::item:hover:!selected { background: rgba(255,255,255,0.05); }"
        "TransferListWidget::item:selected { background: rgba(47,129,247,0.22); }"
        "QTreeView::branch:has-siblings:!adjoins-item { border-image: none; }"
        "QTreeView::branch:has-siblings:adjoins-item  { border-image: none; }"
        "QTreeView::branch:!has-children:!has-siblings:adjoins-item { border-image: none; }"

        "QHeaderView::section { border: none; border-right: 1px solid rgba(255,255,255,0.06);"
        "  border-bottom: 1px solid rgba(255,255,255,0.08); padding: 4px 6px; background: #1A2030; }"
        "QHeaderView::section:last { border-right: none; }"
        "QHeaderView::up-arrow { image: url(:/icons/sort-asc.svg); width: 10px; height: 10px;"
        "  subcontrol-origin: padding; subcontrol-position: center right; margin-right: 4px; }"
        "QHeaderView::down-arrow { image: url(:/icons/sort-desc.svg); width: 10px; height: 10px;"
        "  subcontrol-origin: padding; subcontrol-position: center right; margin-right: 4px; }"

        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #3D4450; margin: 0 8px; }"
        "QSlider::sub-page:horizontal { height: 4px; border-radius: 2px; background: #2F81F7; margin: 0 8px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -6px -8px; border-radius: 8px;"
        "  background: #E6EDF3; border: 3px solid #2F81F7; }"
        "QSlider::handle:horizontal:hover { background: #FFFFFF; }"
        "QSlider::groove:vertical { width: 4px; border-radius: 2px; background: #3D4450; margin: 8px 0; }"
        "QSlider::add-page:vertical { width: 4px; border-radius: 2px; background: #2F81F7; margin: 8px 0; }"
        "QSlider::handle:vertical { width: 14px; height: 14px; margin: -8px -6px; border-radius: 8px;"
        "  background: #E6EDF3; border: 3px solid #2F81F7; }"

        "QSplitter::handle { background: #21262D; }"
        "#propToggleHandle { border: none; border-top-left-radius: 7px; border-top-right-radius: 7px;"
        "  border-bottom-left-radius: 0; border-bottom-right-radius: 0; background: #2C3138; padding: 0; }"
        "#propToggleHandle:hover { background: #39414E; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical   { height: 1px; }"

        "QScrollBar:vertical   { border: none; background: transparent; width: 8px;  margin: 0; }"
        "QScrollBar:horizontal { border: none; background: transparent; height: 8px; margin: 0; }"
        "QScrollBar::handle:vertical   { background: #4B5565; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::handle:horizontal { background: #4B5565; border-radius: 4px; min-width:  24px; }"
        "QScrollBar::handle:vertical:hover,   QScrollBar::handle:horizontal:hover { background: #6B7480; }"
        "QScrollBar::add-line:vertical,  QScrollBar::sub-line:vertical  { height: 0; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"

        "QPushButton { border: 1px solid #3D4450; border-radius: 4px; padding: 5px 14px; background: #2C3138; }"
        "QPushButton:hover   { background: #39414E; }"
        "QPushButton:pressed { background: #454E5C; }"
        "QPushButton:default { border-color: #2F81F7; color: #2F81F7; }"
        "QPushButton:flat { border: none; background: transparent; }"
        "QPushButton:flat:hover { background: %1; }"

        "QLineEdit, QTextEdit, QPlainTextEdit { border: 1px solid #3D4450; border-radius: 4px; padding: 3px 6px; background: #1A2030; }"
        "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus { border-color: #2F81F7; }"

        "QComboBox { border: 1px solid #3D4450; border-radius: 4px; padding: 3px 6px; background: #2C3138; }"
        "QComboBox:focus { border-color: #2F81F7; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #21262D; border: 1px solid #3D4450; border-radius: 4px; }"

        "QSpinBox, QDoubleSpinBox { border: 1px solid #3D4450; border-radius: 4px; padding: 3px 6px; background: #1A2030; }"
        "QSpinBox:focus, QDoubleSpinBox:focus { border-color: #2F81F7; }"

        "QStatusBar { background: #151C26; border-top: 1px solid #3D4450; }"
        "QStatusBar::item { border: none; }"

        "QGroupBox { border: 1px solid #3D4450; border-radius: 4px; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 4px; left: 8px; }"

        "QToolTip { background: #1A2030; color: #E6EDF3; border: 1px solid #3D4450; border-radius: 3px; padding: 4px 6px; }"

        /* Native custom window bar (widgetframe, QWK namespace) - Windows only */
        "QWK--WindowBar { background: #151C26; border: none; }"
        "QWK--WindowBar[bar-active=false] { background: #151C26; }"
        "QWK--WindowBar > QAbstractButton[system-button=true] {"
        "  min-width: 46px; min-height: 40px; max-height: 40px; border: none; border-radius: 0; padding: 0; background: transparent; }"
        "QWK--WindowBar > QAbstractButton[system-button=true]:hover { background: %1; }"
        "QWK--WindowBar > QAbstractButton[system-button=true]:pressed { background: %2; }"
        // Emulated hover for the maximize button while Windows owns the mouse
        // (HTMAXBUTTON for Snap Layouts); driven by the ncHover dynamic property
        "QWK--WindowBar > QAbstractButton#max-button[ncHover=\"true\"] { background: %1; }"
        "QWK--WindowBar > QAbstractButton#close-button:hover { background: #e81123; }"
        "QWK--WindowBar > QAbstractButton#close-button:pressed { background: #c50f1f; }"
        "QWK--WindowBar > QAbstractButton#min-button {"
        "  qproperty-iconNormal: url(:/icons/window-bar/minimize-dark.svg);"
        "  qproperty-iconSize: 12px 12px; }"
        "QWK--WindowBar > QAbstractButton#max-button {"
        "  qproperty-iconNormal: url(:/icons/window-bar/maximize-dark.svg);"
        "  qproperty-iconChecked: url(:/icons/window-bar/restore-dark.svg);"
        "  qproperty-iconSize: 12px 12px; }"
        "QWK--WindowBar > QAbstractButton#close-button {"
        "  qproperty-iconNormal: url(:/icons/window-bar/close-dark.svg);"
        "  qproperty-iconSize: 12px 12px; }"
        "QWK--WindowBar QToolButton#app-menu-button { border: none; border-radius: 0; outline: none; background: transparent; padding: 0 10px; min-height: 40px; max-height: 40px; color: #E6EDF3; font-weight: bold; }"
        "QWK--WindowBar QToolButton#app-menu-button:hover { background: %1; }"
        "QWK--WindowBar QToolButton#app-menu-button:pressed { background: %2; }"
        "QWK--WindowBar QToolButton#app-menu-button::menu-indicator { image: none; }"
        "QWK--WindowBar QMenuBar { background: transparent; border: none; }"
    )
    .arg(u"rgba(255,255,255,30)"_s, u"rgba(255,255,255,55)"_s);

    return qss.toUtf8();
}

QColor DefaultThemeSource::getColor(const QString &colorId, const ColorMode colorMode) const
{
    const auto iter = m_colors.constFind(colorId);
    Q_ASSERT(iter != m_colors.constEnd());
    if (iter == m_colors.constEnd()) [[unlikely]]
        return {};

    return (colorMode == ColorMode::Light)
            ? iter.value().light : iter.value().dark;
}

Path DefaultThemeSource::getIconPath(const QString &iconId, const ColorMode colorMode) const
{
    const Path iconsPath {u"icons"_s};
    const Path lightModeIconsPath = iconsPath / Path(u"light"_s);
    const Path darkModeIconsPath = iconsPath / Path(u"dark"_s);

    if (colorMode == ColorMode::Dark)
    {
        if (const Path iconPath = findIcon(iconId, (m_userPath / darkModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }
    else
    {
        if (const Path iconPath = findIcon(iconId, (m_userPath / lightModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }

    return getDefaultIconPath(iconId, colorMode);
}

Path DefaultThemeSource::getDefaultIconPath(const QString &iconId, const ColorMode colorMode) const
{
    const Path iconsPath {u"icons"_s};
    const Path darkModeIconsPath = iconsPath / Path(u"dark"_s);

    if (colorMode == ColorMode::Dark)
    {
        if (const Path iconPath = findIcon(iconId, (m_defaultPath / darkModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }

    return findIcon(iconId, (m_defaultPath / iconsPath));
}

void DefaultThemeSource::loadColors()
{
    const auto readResult = Utils::IO::readFile((m_userPath / Path(CONFIG_FILE_NAME)), FILE_MAX_SIZE, QIODevice::Text);
    if (!readResult)
    {
        if (readResult.error().status != Utils::IO::ReadError::NotExist)
            LogMsg(tr("Failed to load default theme colors. %1").arg(readResult.error().message), Log::WARNING);

        return;
    }

    const QByteArray &configData = readResult.value();
    if (configData.isEmpty())
        return;

    const QJsonObject config = parseThemeConfig(configData);

    const QHash<QString, QColor> lightModeColorOverrides = colorsFromJSON(config.value(KEY_COLORS_LIGHT).toObject());
    for (auto overridesIt = lightModeColorOverrides.cbegin(); overridesIt != lightModeColorOverrides.cend(); ++overridesIt)
    {
        const auto it = m_colors.find(overridesIt.key());
        if (it != m_colors.end())
            it.value().light = overridesIt.value();
    }

    const QHash<QString, QColor> darkModeColorOverrides = colorsFromJSON(config.value(KEY_COLORS_DARK).toObject());
    for (auto overridesIt = darkModeColorOverrides.cbegin(); overridesIt != darkModeColorOverrides.cend(); ++overridesIt)
    {
        const auto it = m_colors.find(overridesIt.key());
        if (it != m_colors.end())
            it.value().dark = overridesIt.value();
    }
}

CustomThemeSource::CustomThemeSource(const Path &themeRootPath)
    : m_themeRootPath {themeRootPath}
{
    loadColors();
}

QColor CustomThemeSource::getColor(const QString &colorId, const ColorMode colorMode) const
{
    if (colorMode == ColorMode::Dark)
    {
        if (const QColor color = m_darkModeColors.value(colorId)
                ; color.isValid())
        {
            return color;
        }
    }

    if (const QColor color = m_colors.value(colorId)
            ; color.isValid())
    {
        return color;
    }

    return defaultThemeSource()->getColor(colorId, colorMode);
}

Path CustomThemeSource::getIconPath(const QString &iconId, const ColorMode colorMode) const
{
    const Path iconsPath {u"icons"_s};
    const Path darkModeIconsPath = iconsPath / Path(u"dark"_s);

    if (colorMode == ColorMode::Dark)
    {
        if (const Path iconPath = findIcon(iconId, (themeRootPath() / darkModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }

    if (const Path iconPath = findIcon(iconId, (themeRootPath() / iconsPath))
            ; !iconPath.isEmpty())
    {
        return iconPath;
    }

    return defaultThemeSource()->getIconPath(iconId, colorMode);
}

QByteArray CustomThemeSource::readStyleSheet()
{
    const auto readResult = Utils::IO::readFile((themeRootPath() / Path(STYLESHEET_FILE_NAME)), FILE_MAX_SIZE, QIODevice::Text);
    if (!readResult)
    {
        if (readResult.error().status != Utils::IO::ReadError::NotExist)
            LogMsg(tr("Failed to load custom theme style sheet. %1").arg(readResult.error().message), Log::WARNING);

        return {};
    }

    return readResult.value();
}

DefaultThemeSource *CustomThemeSource::defaultThemeSource() const
{
    return m_defaultThemeSource.get();
}

Path CustomThemeSource::themeRootPath() const
{
    return m_themeRootPath;
}

void CustomThemeSource::loadColors()
{
    const auto readResult = Utils::IO::readFile((themeRootPath() / Path(CONFIG_FILE_NAME)), FILE_MAX_SIZE, QIODevice::Text);
    if (!readResult)
    {
        if (readResult.error().status != Utils::IO::ReadError::NotExist)
            LogMsg(tr("Failed to load custom theme colors. %1").arg(readResult.error().message), Log::WARNING);

        return;
    }

    const QByteArray &configData = readResult.value();
    if (configData.isEmpty())
        return;

    const QJsonObject config = parseThemeConfig(configData);

    m_colors.insert(colorsFromJSON(config.value(KEY_COLORS).toObject()));
    m_darkModeColors.insert(colorsFromJSON(config.value(KEY_COLORS_DARK).toObject()));
}

FolderThemeSource::FolderThemeSource(const Path &folderPath)
    : CustomThemeSource(folderPath)
    , m_folder {folderPath}
{
}

QByteArray FolderThemeSource::readStyleSheet()
{
    // Directory used by stylesheet to reference internal resources
    // for example `icon: url(:/uitheme/file.svg)` will be expected to
    // point to a file `file.svg` in root directory of CONFIG_FILE_NAME
    const QString stylesheetResourcesDir = u":/uitheme"_s;

    QByteArray styleSheetData = CustomThemeSource::readStyleSheet();
    return styleSheetData.replace(stylesheetResourcesDir.toUtf8(), m_folder.data().toUtf8());
}

QRCThemeSource::QRCThemeSource()
    : CustomThemeSource(Path(u":/uitheme"_s))
{
}
