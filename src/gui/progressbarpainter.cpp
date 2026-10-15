/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
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

#include "progressbarpainter.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionViewItem>

#include "base/global.h"
#include "gui/uithememanager.h"

ProgressBarPainter::ProgressBarPainter(QObject *parent)
    : QObject(parent)
{
    applyUITheme();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &ProgressBarPainter::applyUITheme);
}

void ProgressBarPainter::paint(QPainter *painter, const QStyleOptionViewItem &option, const QString &text, const int progress, const QColor &color) const
{
    painter->save();

    // Selection/hover background behind the bar
    const QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Flat rounded bar: dark groove, accent fill, centered percentage text
    const QRect barRect = option.rect.adjusted(4, 4, -4, -4);
    const qreal radius = 4;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0x3D4450));
    painter->drawRoundedRect(barRect, radius, radius);

    if (progress > 0)
    {
        QColor fill = color.isValid() ? color : m_chunkColor;
        if (!fill.isValid())
            fill = QColor(0x2F81F7);

        const int fillWidth = std::max<int>((barRect.width() * progress) / 100, (2 * radius));
        QPainterPath clipPath;
        clipPath.addRoundedRect(barRect, radius, radius);
        painter->setClipPath(clipPath);
        painter->setBrush(fill);
        painter->drawRoundedRect(QRect(barRect.left(), barRect.top(), fillWidth, barRect.height()), radius, radius);
        painter->setClipping(false);
    }

    painter->setPen(QColor(0xE6EDF3));
    painter->drawText(option.rect, Qt::AlignCenter, text);

    painter->restore();
}

void ProgressBarPainter::applyUITheme()
{
    m_chunkColor = UIThemeManager::instance()->getColor(u"ProgressBar"_s);
}
