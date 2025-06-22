#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QList>
#include <QTimer>
#include <QMutex>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QSettings>
#include "../dialogs/about.h"
#include "../dialogs/downloadconfirmationdialog.h"
#include "../dialogs/downloaddetailsdialog.h"
#include "../network/downloaditem.h"
#include "QTcpServer"
#include "../network/downloadmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class DownloadConfirmationDialog;
class DownloadDetailsDialog;
class AboutDialog;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showDownloadDetails();
    void onQueueStatusChanged(int activeDownloads, int queuedDownloads);
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleDownloadFinished();
    void handleDownloadFailed(const QString &reason);
    void openSpeedLimiterDialog();
    void openPreferences();
    void toggleSpeedLimiter(bool enabled);
    void onNetworkReachabilityChanged(QNetworkInformation::Reachability reachability);
    void showAboutDialog();
    void showContextMenu(const QPoint &pos);
    void retryDownload(DownloadItem *item);
    void openFile(DownloadItem *item);
    void deleteDownload(DownloadItem *item);
    void copyUrl(DownloadItem *item);
    void downloadFromYouTube(); // New slot for YouTube download
    void newConnection();
    void readClient();
    void openFileLocation(DownloadItem *item);

private:
    DownloadDetailsDialog *m_detailsDialog = nullptr;
    QNetworkAccessManager *networkManager;
    Ui::MainWindow *ui;
    QMap<QString, QList<DownloadItem*>> categories;
    QNetworkProxy proxySettings;
    DownloadManager *m_downloadManager;
    QMutex mutex;
    QHash<DownloadItem*, int> itemRowMap;
    QTimer *updateTimer;
    QString defaultDownloadFolder;
    QAction *m_openFileLocationAction;
    bool autoStartDownloads = true;
    bool promptBeforeOverwrite = true;
    QString fileNamingPolicy = "Use original name";
    int maxConcurrentDownloads = 3;
    QSettings settings{"Advanced", "IDMApp"};
    AboutDialog *aboutDialog;
    QMenu *contextMenu;
    QAction *pauseAction;
    QAction *resumeAction;
    QAction *stopAction;
    QAction *retryAction;
    QAction *openFileAction;
    QAction *deleteAction;
    QAction *copyUrlAction;
    QAction *youtubeAction;
    QAction *detailsAction;
    QAction *updatelink;
    QAction *openfilelocation;
    QTcpServer *m_server;
    QMap<QByteArray, bool> m_pendingRequests;
    DownloadConfirmationDialog *m_confirmationDialog;
    QStringList m_categories;

    DownloadItem* getDownloadItemForRow(int row);
    void init();
    void addDownload(const QUrl &url, const QString &path);
    void addDownload(const QUrl &url, const QString &path, const QString &category, const QString &customFileName, bool showYouTubeDialog); // New enhanced version
    void updateDownloadTable();
    void scheduleTableUpdate();
    void saveDownloadListToFile(const QString& filename);
    void loadDownloadListFromFile(const QString& filename);
    void updateDownloadSpeed(DownloadItem *item);
    void saveDownloadHistory();
    void loadDownloadHistory();
    void newDownload();
    void startDownload(DownloadItem *item);
    void pauseDownload(DownloadItem *item);
    void resumeDownload(DownloadItem *item);
    void stopDownload(DownloadItem *item);
    void startAllDownloads();
    void stopAllDownloads();
    void pauseAllDownloads();
    void resumeAllDownloads();
    void clearCompletedDownloads();
    void refreshLinkForSelectedItem();
    void checkForUpdates();
    void openUserGuide();
    void toggleToolbar();
    void toggleStatusbar();
    void importDownloadList();
    void exportDownloadList();
    void openConnectionSettings();
    void loadPreferences();
    void savePreferences();
    void addCategory(const QString& category);
    void setMaxConcurrentDownloads(int max);
    void updateContextMenuActions(DownloadItem *item);
    bool isYouTubeUrl(const QString &url); // New helper method
    void processYouTubeDownload(const QString &url); // New helper method
    void loadExtensionsUI();
    QStringList getCategories() const;



protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
