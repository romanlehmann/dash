#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPixmap>
#include <algorithm>

class SplashOverlay : public QWidget {

public:
    explicit SplashOverlay(QWidget *parent = nullptr)
        : QWidget(parent), logo(new QLabel(this))
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0,0,0,0);
        layout->addWidget(logo, 0, Qt::AlignCenter);
        setLayout(layout);
    }

    void setPixmap(const QPixmap &pm) {
        logo->setPixmap(pm);
        logo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        logo->adjustSize();
    }

    void setRelativeOffset(float dx, float dy) {
        if (!logo->pixmap()) return;
        const QSize scr = size();
        const QSize img = logo->size();
        const int x = (scr.width()  - img.width())  / 2 + int(dx * scr.width());
        const int y = (scr.height() - img.height()) / 2 + int(dy * scr.height());
        int left   = std::max(0, x);
        int top    = std::max(0, y);
        int right  = std::max(0, scr.width()  - x - img.width());
        int bottom = std::max(0, scr.height() - y - img.height());
        layout()->setContentsMargins(left, top, right, bottom);
        layout()->update();
    }

private:
    QLabel *logo;
};
