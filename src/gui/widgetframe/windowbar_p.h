// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef WINDOWBARPRIVATE_H
#define WINDOWBARPRIVATE_H

#include <QBoxLayout>

#include "windowbar.h"

namespace QWK {

    class WindowBarPrivate {
        Q_DECLARE_PUBLIC(WindowBar)
    public:
        WindowBarPrivate();
        virtual ~WindowBarPrivate();

        void init();

        WindowBar *q_ptr;

        QWidget *w;
        bool autoTitle;
        bool autoIcon;

        enum WindowBarItem {
            IconButton,
            TitleLabel,      // index 1 — shown before the menu bar
            MenuWidget,      // index 2 — shown after title
            PinButton,       // index 3 — defaults to an expanding drag spacer
            MinimizeButton,
            MaximizeButton,
            CloseButton,
        };

        QHBoxLayout *layout;

        inline QWidget *widgetAt(int index) const {
            return layout->itemAt(index)->widget();
        }

        void setWidgetAt(int index, QWidget *widget);

        QWidget *takeWidgetAt(int index);

        inline void insertDefaultSpace(int index) {
            if (index == PinButton) {
                // The pin slot doubles as the drag spacer between the menu bar
                // and the system buttons when no pin button is assigned.
                layout->insertSpacerItem(index,
                    new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
            } else {
                layout->insertSpacerItem(index, new QSpacerItem(0, 0));
            }
        }

    private:
        Q_DISABLE_COPY(WindowBarPrivate)
    };

}

#endif // WINDOWBARPRIVATE_H
