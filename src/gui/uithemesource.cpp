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
    return QByteArrayLiteral(
        /* ── Global ─────────────────────────────────────────────── */
        "* { outline: 0; }"

        /* ── QMainWindow / central widget ───────────────────────── */
        "QMainWindow { border: none; }"

        /* ── Toolbar ─────────────────────────────────────────────── */
        "QToolBar {"
        "  border: none;"
        "  spacing: 2px;"
        "  padding: 2px 4px;"
        "}"
        "QToolBar::separator {"
        "  width: 1px;"
        "  background: palette(mid);"
        "  margin: 4px 2px;"
        "}"
        "QToolButton {"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  background: transparent;"
        "}"
        "QToolButton:hover  { background: palette(midlight); }"
        "QToolButton:pressed { background: palette(mid); }"

        /* ── Menu bar ─────────────────────────────────────────────── */
        "QMenuBar { border-bottom: 1px solid palette(mid); }"
        "QMenuBar::item { padding: 4px 8px; border-radius: 3px; }"
        "QMenuBar::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
        "QMenu { border: 1px solid palette(mid); border-radius: 4px; padding: 2px; }"
        "QMenu::item { padding: 5px 24px 5px 12px; border-radius: 3px; }"
        "QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
        "QMenu::separator { height: 1px; background: palette(mid); margin: 3px 6px; }"

        /* ── Tab bar (Properties panel buttons are QPushButtons, */
        /* but QTabBar is used in Options dialog) ─────────────────── */
        "QTabBar::tab {"
        "  padding: 6px 16px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  background: transparent;"
        "}"
        "QTabBar::tab:selected {"
        "  border-bottom: 2px solid palette(highlight);"
        "  color: palette(highlight);"
        "}"
        "QTabBar::tab:hover:!selected { background: palette(midlight); border-radius: 4px 4px 0 0; }"

        /* ── Progress bar ─────────────────────────────────────────── */
        "QProgressBar {"
        "  border: none;"
        "  border-radius: 3px;"
        "  background: palette(mid);"
        "  text-align: center;"
        "  max-height: 8px;"
        "  min-height: 8px;"
        "}"
        "QProgressBar::chunk {"
        "  border-radius: 3px;"
        "  background: palette(highlight);"
        "}"

        /* ── Tree / List view ─────────────────────────────────────── */
        "QTreeView, QListView {"
        "  border: none;"
        "  alternate-background-color: transparent;"
        "  show-decoration-selected: 1;"
        "}"
        "QTreeView::item, QListView::item { padding: 3px 0; }"
        "QTreeView::branch:has-siblings:!adjoins-item { border-image: none; }"
        "QTreeView::branch:has-siblings:adjoins-item  { border-image: none; }"
        "QTreeView::branch:!has-children:!has-siblings:adjoins-item { border-image: none; }"

        /* ── Header ───────────────────────────────────────────────── */
        "QHeaderView::section {"
        "  border: none;"
        "  border-right: 1px solid palette(mid);"
        "  border-bottom: 1px solid palette(mid);"
        "  padding: 4px 6px;"
        "  background: palette(window);"
        "}"
        "QHeaderView::section:last { border-right: none; }"

        /* ── Splitter ─────────────────────────────────────────────── */
        "QSplitter::handle { background: palette(mid); }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical   { height: 1px; }"

        /* ── Scrollbar ────────────────────────────────────────────── */
        "QScrollBar:vertical {"
        "  border: none;"
        "  background: palette(window);"
        "  width: 8px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: palette(mid);"
        "  border-radius: 4px;"
        "  min-height: 24px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: palette(dark); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal {"
        "  border: none;"
        "  background: palette(window);"
        "  height: 8px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: palette(mid);"
        "  border-radius: 4px;"
        "  min-width: 24px;"
        "}"
        "QScrollBar::handle:horizontal:hover { background: palette(dark); }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"

        /* ── Push button ──────────────────────────────────────────── */
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 5px 14px;"
        "  background: palette(button);"
        "}"
        "QPushButton:hover   { background: palette(midlight); }"
        "QPushButton:pressed { background: palette(mid); }"
        "QPushButton:default {"
        "  border-color: palette(highlight);"
        "  color: palette(highlight);"
        "}"
        "QPushButton:flat { border: none; background: transparent; }"
        "QPushButton:flat:hover { background: palette(midlight); }"

        /* ── Line / text edit ────────────────────────────────────── */
        "QLineEdit, QTextEdit, QPlainTextEdit {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 3px 6px;"
        "  background: palette(base);"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {"
        "  border-color: palette(highlight);"
        "}"

        /* ── Combo box ───────────────────────────────────────────── */
        "QComboBox {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 3px 6px;"
        "  background: palette(button);"
        "}"
        "QComboBox:focus { border-color: palette(highlight); }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { border: 1px solid palette(mid); border-radius: 4px; }"

        /* ── Spin box ────────────────────────────────────────────── */
        "QSpinBox, QDoubleSpinBox {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 3px 6px;"
        "  background: palette(base);"
        "}"
        "QSpinBox:focus, QDoubleSpinBox:focus { border-color: palette(highlight); }"

        /* ── Status bar ──────────────────────────────────────────── */
        "QStatusBar { border-top: 1px solid palette(mid); }"
        "QStatusBar::item { border: none; }"

        /* ── Group box ───────────────────────────────────────────── */
        "QGroupBox {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  margin-top: 8px;"
        "  padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0 4px;"
        "  left: 8px;"
        "}"

        /* ── Tooltip ─────────────────────────────────────────────── */
        "QToolTip {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 3px;"
        "  padding: 4px 6px;"
        "}"
    );
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
