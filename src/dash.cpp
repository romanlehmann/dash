#include <QApplication>
#include <QStringList>
#include <QWindow>

#include "app/window.hpp"
#include "app/action.hpp"

// [CHANGED] Include the overlay splash header instead of using QSplashScreen
#include "app/splashoverlay.hpp"

int main(int argc, char *argv[])
{
    QApplication dash(argc, argv);

    dash.setOrganizationName("openDsh");
    dash.setApplicationName("dash");
    dash.installEventFilter(ActionEventFilter::get_instance());

    QSize  size = dash.primaryScreen()->geometry().size();
    QPoint pos  = dash.primaryScreen()->geometry().topLeft();
    bool fullscreen = true;

    QSettings settings;
    DASH_LOG(info) << "loaded config: " << settings.fileName().toStdString();

    QStringList args = dash.arguments();
    if (args.size() > 2) {
        size = QSize(args.at(1).toInt(), args.at(2).toInt());
        if (args.size() > 4)
            pos = QPoint(args.at(3).toInt(), args.at(4).toInt());
        fullscreen = false;
    }
    else {
        settings.beginGroup("Window");
        if (settings.contains("size")) {
            size = settings.value("size").toSize();
            if (settings.contains("pos"))
                pos = settings.value("pos").toPoint();
            fullscreen = false;
        }
    }

    // Use a fullscreen transparent overlay and center the image inside
    {
        // Prepare the splash pixmap scaled relative to the primary screen height
        const QRect screenRect = dash.primaryScreen()->geometry();  // screen size for scaling
        QPixmap splashPm(":/splash.svg");
        // Skaliere unter Berücksichtigung beider Dimensionen
        splashPm = splashPm.scaled(screenRect.width()/2, screenRect.height()/2, 
                                 Qt::KeepAspectRatio, 
                                 Qt::SmoothTransformation);

        // Create and show the overlay fullscreen (kiosk-shell forces fullscreen anyway)
        SplashOverlay overlay;
        overlay.setPixmap(splashPm);     // centers by default
        overlay.showFullScreen();
        dash.processEvents();            // ensure it's painted before main window shows


        MainWindow window(QRect(pos, size));
        window.setWindowIcon(QIcon(":/logo.png"));
        window.setWindowFlags(Qt::FramelessWindowHint);
        if (fullscreen)
            window.setWindowState(Qt::WindowFullScreen);

        window.show();                   // show main UI

        // [ADDED] Close the overlay immediately after showing the main window
        overlay.close();
        return dash.exec();
    }
}
