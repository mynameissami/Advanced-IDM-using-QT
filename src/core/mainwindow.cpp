#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../dialogs/PreferencesDialog.h"
#include "../dialogs/about.h"
#include "../dialogs/speedlimiterdialog.h"
#include "../dialogs/connectionsettingsdialog.h"
#include "../network/downloaditem.h"
#include "../network/downloadmanager.h"
#include "../dialogs/youtubedownloaddialog.h"

#define MAX_CONCURRENT_DOWNLOADS 6 // this sets the max concurrent downloads

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QActionGroup>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
#include <QApplication>
#include <QRegularExpression>
#include <QClipboard>
#include "../utils/utils.h"
bool speedLimitEnabled = false;
int currentSpeedLimit = 0;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), m_downloadManager(new DownloadManager(this)), m_server(new QTcpServer(this)), m_confirmationDialog(nullptr)
{
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);

    qRegisterMetaType<DownloadItem*>("DownloadItem*");
    m_downloadManager = new DownloadManager(this);
    m_downloadManager->setMaxConcurrentDownloads(maxConcurrentDownloads);

    loadPreferences();

    QActionGroup* limiterGroup = new QActionGroup(this);
    limiterGroup->setExclusive(true);
    limiterGroup->addAction(ui->actionOn);
    limiterGroup->addAction(ui->actionOff);

    ui->actionOn->setCheckable(true);
    ui->actionOff->setCheckable(true);
    ui->actionOff->setChecked(true);

    aboutDialog = nullptr;

    if (!categories.contains("All Downloads")) {
        categories["All Downloads"] = QList<DownloadItem*>();
    }

    ui->downloadsTable->setColumnCount(8);
    ui->downloadsTable->setHorizontalHeaderLabels({
        "File Name", "Size", "Status", "Queue Position", "Time Left",
        "Transfer Rate", "Last Try Date", "Description"
    });

    if (QNetworkInformation::loadDefaultBackend()) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged,
                this, &MainWindow::onNetworkReachabilityChanged);
    }

    connect(ui->actionNewDownload, &QAction::triggered, this, &MainWindow::newDownload);
    connect(ui->actionStartAll, &QAction::triggered, this, &MainWindow::startAllDownloads);
    connect(ui->actionStopAll, &QAction::triggered, this, &MainWindow::stopAllDownloads);
    connect(ui->actionPauseAll, &QAction::triggered, this, &MainWindow::pauseAllDownloads);
    connect(ui->actionResume_All, &QAction::triggered, this, &MainWindow::resumeAllDownloads);
    connect(ui->actionClearCompleted, &QAction::triggered, this, &MainWindow::clearCompletedDownloads);
    connect(ui->actionRefreshLinks, &QAction::triggered, this, &MainWindow::refreshLinkForSelectedItem);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    connect(ui->actionConnectionSettings, &QAction::triggered, this, &MainWindow::openConnectionSettings);
    connect(ui->actionImportList, &QAction::triggered, this, &MainWindow::importDownloadList);
    connect(ui->actionExportList, &QAction::triggered, this, &MainWindow::exportDownloadList);
    connect(ui->actionOn, &QAction::triggered, this, [this]() { toggleSpeedLimiter(true); });
    connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui->actionOff, &QAction::triggered, this, [this]() { toggleSpeedLimiter(false); });
    connect(ui->actionSettings, &QAction::triggered, this, [this]() { openSpeedLimiterDialog(); });
    connect(ui->actionPreferences, &QAction::triggered, this, &MainWindow::openPreferences);
    connect(m_downloadManager, &DownloadManager::queueStatusChanged, this, &MainWindow::onQueueStatusChanged);

    contextMenu = new QMenu(this);
    contextMenu->setStyleSheet(
        "QMenu { background-color: #242424; border: 1px solid #D3D3D3; border-radius: 4px; padding: 2px; }"
        "QMenu::item { padding: 5px 25px 5px 20px; font-size: 12px; color: #ffffff; }"
        "QMenu::item:selected { background-color: #E0E0E0; color: #000000; }"
        "QMenu::separator { height: 1px; background: #D3D3D3; margin: 2px 0px; }"
        );
    pauseAction = new QAction("Pause", this);
    resumeAction = new QAction("Resume", this);
    stopAction = new QAction("Stop", this);
    updatelink = new QAction("Update Link",this);
    retryAction = new QAction("Retry", this);
    openFileAction = new QAction("Open File", this);
    openfilelocation = new QAction("Open File Location", this);
    deleteAction = new QAction("Delete", this);
    copyUrlAction = new QAction("Copy URL", this);
    youtubeAction = new QAction("Download from YouTube", this);
    detailsAction = new QAction("View Details", this);

    contextMenu->addAction(pauseAction);
    contextMenu->addAction(resumeAction);
    contextMenu->addAction(stopAction);
    contextMenu->addSeparator();
    contextMenu->addAction(updatelink);
    contextMenu->addAction(retryAction);
    contextMenu->addAction(openFileAction);
    contextMenu->addAction(openfilelocation);
    contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(copyUrlAction);
    contextMenu->addAction(youtubeAction);
    contextMenu->addAction(detailsAction);

    m_openFileLocationAction = new QAction(tr("Open File Location"), this);
    connect(m_openFileLocationAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        DownloadItem *item = getDownloadItemForRow(row);
        if (item) openFileLocation(item);
    });
    connect(detailsAction, &QAction::triggered, this, &MainWindow::showDownloadDetails);
    connect(pauseAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) pauseDownload(item);
        }
    });
    connect(updatelink, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) refreshLinkForSelectedItem();
        }
    });
    connect(resumeAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) resumeDownload(item);
        }
    });
    connect(stopAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) stopDownload(item);
        }
    });
    connect(retryAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) retryDownload(item);
        }
    });
    connect(openFileAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) openFile(item);
        }
    });
    connect(deleteAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) deleteDownload(item);
        }
    });
    connect(copyUrlAction, &QAction::triggered, this, [this]() {
        int row = ui->downloadsTable->currentRow();
        if (row >= 0) {
            DownloadItem *item = getDownloadItemForRow(row);
            if (item) copyUrl(item);
        }
    });
    connect(youtubeAction, &QAction::triggered, this, &MainWindow::downloadFromYouTube);

    ui->downloadsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->downloadsTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    updateTimer = new QTimer(this);
    updateTimer->setInterval(500);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateDownloadTable);
    updateTimer->start();
    toggleSpeedLimiter(false);
    setMaxConcurrentDownloads(MAX_CONCURRENT_DOWNLOADS);
    loadDownloadHistory();

    //to capture download links throgh extension
    if (!m_server->listen(QHostAddress::LocalHost, 8080)) {
        qDebug() << "Server could not start!";
    } else {
        connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
        qDebug() << "Server started on port 8080";
    }
}
void MainWindow::newConnection()
{
    QTcpSocket *clientSocket = m_server->nextPendingConnection();
    if (clientSocket) {
        qDebug() << "New connection from:" << clientSocket->peerAddress().toString() << "at" << QDateTime::currentDateTime().toString();
        connect(clientSocket, &QTcpSocket::readyRead, this, &MainWindow::readClient);
        connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
    } else {
        qDebug() << "Failed to get pending connection at" << QDateTime::currentDateTime().toString();
    }
}

void MainWindow::readClient()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        qDebug() << "No valid socket sender at" << QDateTime::currentDateTime().toString();
        return;
    }

    QByteArray data = socket->readAll();
    QString request(data);
    qDebug() << "Received data at" << QDateTime::currentDateTime().toString() << ":" << request;

    QRegularExpression requestRegex("(GET|HEAD)\\s+(/[^\\s?]*)(?:\\?url=([^\\s]+))?(?:\\s+HTTP/\\d\\.\\d)");
    QRegularExpressionMatch match = requestRegex.match(request);
    if (match.hasMatch()) {
        QString method = match.captured(1);
        QString basePath = match.captured(2);
        QString urlParam = match.captured(3);
        qDebug() << "Parsed method at" << QDateTime::currentDateTime().toString() << ":" << method;
        qDebug() << "Parsed basePath at" << QDateTime::currentDateTime().toString() << ":" << basePath;
        qDebug() << "Parsed urlParam at" << QDateTime::currentDateTime().toString() << ":" << urlParam;

        QString host = request.contains("Host: ") ? request.mid(request.indexOf("Host: ") + 6).split("\r\n").first().trimmed() : "localhost";
        QUrl url;
        if (!urlParam.isEmpty()) {
            url = QUrl::fromUserInput(QUrl::fromPercentEncoding(urlParam.toUtf8()));
            qDebug() << "Constructed URL from param at" << QDateTime::currentDateTime().toString() << ":" << url.toString();
        } else {
            url = QUrl::fromUserInput("http://" + host + basePath);
            qDebug() << "Constructed URL from path at" << QDateTime::currentDateTime().toString() << ":" << url.toString();
        }

        QString fileExtension = QFileInfo(url.path()).suffix().toLower();
        QStringList downloadableExtensions = {"pdf", "mp4", "mp3", "avi", "mkv", "wav",
                                              "jpg", "jpeg", "png", "bmp", "gif", "webp",
                                              "zip", "rar", "7z", "tar", "gz",
                                              "exe", "msi", "apk", "iso", "bin",
                                              "doc", "docx", "xls", "xlsx", "ppt", "pptx",
                                              "txt", "csv", "json", "xml", "html",""};
        bool isDownloadable = !fileExtension.isEmpty() && downloadableExtensions.contains(fileExtension);
        bool isYouTube = url.toString().contains("youtube.com") || url.toString().contains("youtu.be");

        if (url.isValid() && (isDownloadable || isYouTube) && (method == "GET" || method == "HEAD")) {
            if (!m_confirmationDialog) {
                m_confirmationDialog = new DownloadConfirmationDialog(url, getCategories(), this);
                if (!m_confirmationDialog) {
                    qDebug() << "Failed to create DownloadConfirmationDialog at" << QDateTime::currentDateTime().toString();
                    socket->write("HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nDialog creation failed");
                    socket->flush();
                    socket->disconnectFromHost();
                    return;
                }
            } else {
                m_confirmationDialog->setAttribute(Qt::WA_DeleteOnClose);
                m_confirmationDialog->close();
                delete m_confirmationDialog;
                m_confirmationDialog = new DownloadConfirmationDialog(url, getCategories(), this);
                if (!m_confirmationDialog) {
                    qDebug() << "Failed to recreate DownloadConfirmationDialog at" << QDateTime::currentDateTime().toString();
                    socket->write("HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nDialog recreation failed");
                    socket->flush();
                    socket->disconnectFromHost();
                    return;
                }
            }
            if (method == "GET" && m_confirmationDialog->exec() == QDialog::Accepted && m_confirmationDialog->isConfirmed()) {
                QString category = m_confirmationDialog->getSelectedCategory();
                QString fileName = m_confirmationDialog->getCustomFileName().isEmpty() ? QFileInfo(url.path()).fileName() : m_confirmationDialog->getCustomFileName();
                bool showYouTubeDialog = m_confirmationDialog->shouldShowYouTubeDialog() && isYouTube;
                qDebug() << "Starting download for URL:" << url.toString() << "with category:" << category << "and fileName:" << fileName;
                addDownload(url, "", category, fileName, showYouTubeDialog);
                socket->write("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nDownload started");
            } else if (method == "HEAD") {
                socket->write("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nHEAD request acknowledged");
            } else {
                socket->write("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nDownload cancelled");
            }
            if (m_confirmationDialog) {
                delete m_confirmationDialog;
                m_confirmationDialog = nullptr;
            }
        } else {
            qDebug() << "Not a downloadable resource, invalid URL, or unsupported method at" << QDateTime::currentDateTime().toString() << ":" << url.toString();
            socket->write("HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nNot a downloadable resource or unsupported method");
        }
    } else {
        qDebug() << "Invalid request format at" << QDateTime::currentDateTime().toString() << ":" << request;
        socket->write("HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\nInvalid request");
    }
    socket->flush();
    socket->disconnectFromHost();
}
MainWindow::~MainWindow()
{
    saveDownloadHistory();
    if (m_downloadManager) {
        m_downloadManager->stopAll();
    }
    for (auto &list : categories) {
        qDeleteAll(list);
        list.clear();
    }
    delete ui;
    delete aboutDialog;
    delete contextMenu;
    delete pauseAction;
    delete resumeAction;
    delete stopAction;
    delete retryAction;
    delete openFileAction;
    delete deleteAction;
    delete copyUrlAction;
    delete m_detailsDialog;
    delete youtubeAction;
}
void MainWindow::showDownloadDetails()
{
    int row = ui->downloadsTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a download to view details.");
        return;
    }

    DownloadItem *item = getDownloadItemForRow(row);
    if (!item) {
        QMessageBox::warning(this, "Error", "Could not retrieve download item.");
        return;
    }

    // Safely delete the existing dialog if it exists
    if (m_detailsDialog) {
        m_detailsDialog->deleteLater();
        m_detailsDialog = nullptr;
    }

    // Create a new dialog
    m_detailsDialog = new DownloadDetailsDialog(item, this);
    if (!m_detailsDialog) {
        qDebug() << "Failed to create DownloadDetailsDialog";
        return;
    }

    // Connect to handle deletion
    connect(m_detailsDialog, &QDialog::finished, this, [this]() {
        if (m_detailsDialog) {
            m_detailsDialog->deleteLater();
            m_detailsDialog = nullptr;
        }
    });

    m_detailsDialog->exec();
}
void MainWindow::openFileLocation(DownloadItem *item)
{
    if (!item) {
        qDebug() << "openFileLocation: Invalid DownloadItem pointer";
        return;
    }

    QString filePath = item->getFullFilePath();
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "openFileLocation: File does not exist:" << filePath;
        QMessageBox::warning(this, tr("File Not Found"), tr("The file %1 does not exist.").arg(fileInfo.fileName()));
        return;
    }

    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        qDebug() << "openFileLocation: Directory does not exist:" << dir.absolutePath();
        QMessageBox::warning(this, tr("Directory Not Found"), tr("The directory for %1 does not exist.").arg(fileInfo.fileName()));
        return;
    }

    qDebug() << "Opening file location for:" << filePath;
    QUrl dirUrl = QUrl::fromLocalFile(dir.absolutePath());
    if (!QDesktopServices::openUrl(dirUrl)) {
        qDebug() << "openFileLocation: Failed to open directory:" << dir.absolutePath();
        QMessageBox::warning(this, tr("Error"), tr("Failed to open the file location."));
    }
}
void MainWindow::showContextMenu(const QPoint &pos)
{
    QPoint globalPos = ui->downloadsTable->viewport()->mapToGlobal(pos);
    int row = ui->downloadsTable->indexAt(pos).row();
    if (row >= 0) {
        DownloadItem *item = getDownloadItemForRow(row);
        if (item) {
            updateContextMenuActions(item);
            contextMenu->exec(globalPos);
        }
    }
}
void MainWindow::addDownload(const QUrl &url, const QString &path, const QString &category, const QString &customFileName, bool showYouTubeDialog)
{
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid URL"), tr("The entered URL is not valid."));
        return;
    }

    QString downloadPath = path.isEmpty() ? defaultDownloadFolder : QFileInfo(path).absolutePath();
    QString selectedCategory = category.isEmpty() ? "All Downloads" : category;
    QString fileName = customFileName.isEmpty() ?
                           (QFileInfo(url.path()).fileName().isEmpty() ?
                                "download_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") :
                                QFileInfo(url.path()).fileName()) :
                           customFileName;

    QDir dir(QDir(downloadPath).filePath(selectedCategory));
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot create directory: %1").arg(dir.absolutePath()));
            return;
        }
    }

    QString fullPath = dir.filePath(fileName);
    if (promptBeforeOverwrite && QFile::exists(fullPath)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "File Exists", "File already exists. Overwrite?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) return;
    }

    auto *item = new DownloadItem(url, fullPath, this);
    item->setNumChunks(8);
    item->setState(DownloadItem::Queued);
    item->setLastTryDate(QDateTime::currentDateTime());
    item->setDescription("Browser initiated");

    if (!categories.contains(selectedCategory)) {
        addCategory(selectedCategory);
    }
    categories[selectedCategory].append(item);

    connect(item, &DownloadItem::progress, this, &MainWindow::handleDownloadProgress, Qt::QueuedConnection);
    connect(item, &DownloadItem::finished, this, &MainWindow::handleDownloadFinished, Qt::QueuedConnection);
    connect(item, &DownloadItem::failed, this, &MainWindow::handleDownloadFailed, Qt::QueuedConnection);
    connect(item, &DownloadItem::stateChanged, this, &MainWindow::scheduleTableUpdate, Qt::QueuedConnection);

    if (isYouTubeUrl(url.toString())) {
        if (showYouTubeDialog) {
            YouTubeDownloadDialog youtubeDialog(this);
            if (youtubeDialog.exec() == QDialog::Accepted) {
                QString youtubePath = youtubeDialog.getOutputPath();
                QString youtubeFileName = youtubeDialog.getYtdlArgs().contains("--extract-audio") ?
                                              "audio_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".mp3" :
                                              "video_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".mp4";
                QString youtubeFullPath = QDir(youtubePath).filePath(youtubeFileName);
                item->setFullFilePath(youtubeFullPath);
                m_downloadManager->downloadYouTubeWithOptions(item, youtubeDialog.getYtdlArgs());
            } else {
                item->deleteLater();
                categories[selectedCategory].removeOne(item);
                return;
            }
        } else if (autoStartDownloads) {
            m_downloadManager->addToQueue(item);
        }
    } else if (autoStartDownloads) {
        m_downloadManager->addToQueue(item);
    }
    scheduleTableUpdate();
}
QStringList MainWindow::getCategories() const
{
    return categories.keys();
}
void MainWindow::updateContextMenuActions(DownloadItem *item)
{
    if (!item) return;
    DownloadItem::State state = item->getState();
    pauseAction->setEnabled(state == DownloadItem::Downloading);
    resumeAction->setEnabled(state == DownloadItem::Paused);
    stopAction->setEnabled(state != DownloadItem::Completed && state != DownloadItem::Failed);
    retryAction->setEnabled(state == DownloadItem::Failed || state == DownloadItem::Stopped);
    openFileAction->setEnabled(state == DownloadItem::Completed);
    deleteAction->setEnabled(state != DownloadItem::Downloading);
    copyUrlAction->setEnabled(true);
}

void MainWindow::retryDownload(DownloadItem *item)
{
    if (item && (item->getState() == DownloadItem::Failed || item->getState() == DownloadItem::Stopped)) {
        item->setState(DownloadItem::Queued);
        m_downloadManager->addToQueue(item);
        scheduleTableUpdate();
    }
}

void MainWindow::openFile(DownloadItem *item)
{
    if (item && item->getState() == DownloadItem::Completed) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(item->getFullFilePath()));
    }
}

void MainWindow::deleteDownload(DownloadItem *item)
{
    if (!item || item->getState() == DownloadItem::Downloading) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete", "Are you sure you want to delete this download and its file?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        int row = itemRowMap.value(item, -1);
        if (row >= 0) {
            ui->downloadsTable->removeRow(row);
            itemRowMap.remove(item);
        }

        for (auto &list : categories) {
            list.removeOne(item);
        }

        QString filePath = item->getFullFilePath();
        if (QFile::exists(filePath)) {
            QFile::remove(filePath);
        }
        if (!item->isSingleChunk()) {
            for (int chunk = 0; chunk < item->getNumChunks(); ++chunk) {
                QString chunkFileName = QString("%1.chunk%2").arg(filePath).arg(chunk);
                if (QFile::exists(chunkFileName)) {
                    QFile::remove(chunkFileName);
                }
            }
        }

        item->deleteLater();
        scheduleTableUpdate();
        saveDownloadHistory();
    }
}

void MainWindow::copyUrl(DownloadItem *item)
{
    if (item) {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(item->getUrl().toString());
        ui->statusBar->showMessage("URL copied to clipboard", 2000);
    }
}

void MainWindow::newDownload()
{
    QString urlStr = QInputDialog::getText(this, "New Download", "Enter URL:");
    if (urlStr.isEmpty()) return;

    QUrl url(urlStr);
    if (!url.isValid()) {
        QMessageBox::warning(this, "Invalid URL", "Please enter a valid URL.");
        return;
    }

    QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty()) {
        fileName = "download_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    }

    QString savePath = QDir(defaultDownloadFolder).filePath(fileName);
    if (promptBeforeOverwrite && QFile::exists(savePath)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "File Exists", "File already exists. Overwrite?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) return;
    }

    addDownload(url, savePath);
}

void MainWindow::onNetworkReachabilityChanged(QNetworkInformation::Reachability reachability)
{
    if (reachability == QNetworkInformation::Reachability::Disconnected) {
        qDebug() << "🔌 Disconnected from network. Pausing downloads.";
        m_downloadManager->pauseAll();
        ui->statusBar->showMessage("Disconnected from internet. Downloads paused.");
    } else if (reachability == QNetworkInformation::Reachability::Online) {
        qDebug() << "🌐 Network reconnected.";
        ui->statusBar->showMessage("Network reconnected.");
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Resume Downloads",
            "The internet connection is back. Do you want to resume downloads?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            resumeAllDownloads();
        }
    }
}

void MainWindow::addDownload(const QUrl &url, const QString &path)
{
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid URL"), tr("The entered URL is not valid."));
        return;
    }

    QString downloadPath = path.isEmpty() ? defaultDownloadFolder : path;
    QDir dir(downloadPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot create directory: %1").arg(downloadPath));
            return;
        }
    }

    if (isYouTubeUrl(url.toString())) {
        YouTubeDownloadDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString filePath = dialog.getOutputPath();
            if (!filePath.isEmpty()) {
                QString fileName = dialog.getYtdlArgs().contains("--extract-audio") ? "audio.mp3" : "video.mp4";
                auto *item = new DownloadItem(url, filePath + "/" + fileName, this);
                item->setNumChunks(8);
                item->setState(DownloadItem::Queued);
                item->setLastTryDate(QDateTime::currentDateTime());
                item->setDescription("User added YouTube");

                categories["All Downloads"].append(item);

                connect(item, &DownloadItem::progress, this, &MainWindow::handleDownloadProgress, Qt::QueuedConnection);
                connect(item, &DownloadItem::finished, this, &MainWindow::handleDownloadFinished, Qt::QueuedConnection);
                connect(item, &DownloadItem::failed, this, &MainWindow::handleDownloadFailed, Qt::QueuedConnection);
                connect(item, &DownloadItem::stateChanged, this, &MainWindow::scheduleTableUpdate, Qt::QueuedConnection);

                m_downloadManager->downloadYouTubeWithOptions(item, dialog.getYtdlArgs());
            }
        }
    } else {
        auto *item = new DownloadItem(url, downloadPath + "/" + (url.fileName().isEmpty() ? "download" : url.fileName()), this);
        item->setNumChunks(8);
        item->setState(DownloadItem::Queued);
        item->setLastTryDate(QDateTime::currentDateTime());
        item->setDescription("User added");

        categories["All Downloads"].append(item);

        connect(item, &DownloadItem::progress, this, &MainWindow::handleDownloadProgress, Qt::QueuedConnection);
        connect(item, &DownloadItem::finished, this, &MainWindow::handleDownloadFinished, Qt::QueuedConnection);
        connect(item, &DownloadItem::failed, this, &MainWindow::handleDownloadFailed, Qt::QueuedConnection);
        connect(item, &DownloadItem::stateChanged, this, &MainWindow::scheduleTableUpdate, Qt::QueuedConnection);

        m_downloadManager->addToQueue(item);
    }
    scheduleTableUpdate();
}
void MainWindow::pauseDownload(DownloadItem *item)
{
    if (!item) {
        qDebug() << "pauseDownload: Invalid DownloadItem pointer";
        return;
    }
    if (item->getState() != DownloadItem::Downloading) {
        qDebug() << "pauseDownload: Item" << item->getFileName() << "not in Downloading state, current state:" << item->getState();
        return;
    }
    qDebug() << "Requesting pause for:" << item->getFileName();
    item->pause();
    scheduleTableUpdate();
    qDebug() << "Paused item:" << item->getFileName();
}

void MainWindow::resumeDownload(DownloadItem *item)
{
    if (item && item->getState() == DownloadItem::Paused) {
        item->resume();
        m_downloadManager->addToQueue(item);
        scheduleTableUpdate();
    }
}

void MainWindow::stopDownload(DownloadItem *item)
{
    if (item) {
        item->stop();
        scheduleTableUpdate();
    }
}

void MainWindow::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    DownloadItem *item = qobject_cast<DownloadItem*>(sender());
    if (!item) return;
    qDebug() << "Progress signal received for" << item->getFileName() << ":" << bytesReceived << "/" << bytesTotal;
    scheduleTableUpdate();
}

void MainWindow::onQueueStatusChanged(int activeDownloads, int queuedDownloads)
{
    QString statusMessage = QString("Active downloads: %1 | Queued downloads: %2").arg(activeDownloads).arg(queuedDownloads);
    ui->statusBar->showMessage(statusMessage);
    scheduleTableUpdate();
}

void MainWindow::handleDownloadFinished()
{
    DownloadItem *item = qobject_cast<DownloadItem*>(sender());
    if (!item) return;
    qDebug() << "Finished signal received for" << item->getFileName();
    scheduleTableUpdate();
}

void MainWindow::handleDownloadFailed(const QString &reason)
{
    qDebug() << "Handling download failure:" << reason;

    if (!reason.contains("paused by user", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Download Failed", reason);
    }

    updateDownloadTable();
}

void MainWindow::setMaxConcurrentDownloads(int max)
{
    if (m_downloadManager) {
        m_downloadManager->setMaxConcurrentDownloads(max);
    }
}

void MainWindow::startAllDownloads()
{
    m_downloadManager->resumeAll();
    scheduleTableUpdate();
}

void MainWindow::stopAllDownloads()
{
    m_downloadManager->stopAll();
    scheduleTableUpdate();
}

void MainWindow::pauseAllDownloads()
{
    m_downloadManager->pauseAll();
    scheduleTableUpdate();
}

void MainWindow::resumeAllDownloads()
{
    m_downloadManager->resumeAll();
    scheduleTableUpdate();
}

void MainWindow::clearCompletedDownloads()
{
    qDebug() << "Clearing completed/stopped/failed downloads...";

    for (auto &list : categories) {
        for (int i = list.size() - 1; i >= 0; --i) {
            DownloadItem* item = list.at(i);
            if (!item) continue;

            DownloadItem::State state = item->getState();
            if (state == DownloadItem::Completed || state == DownloadItem::Stopped || state == DownloadItem::Failed || state == DownloadItem::Paused) {
                QString fileName = item->getFileName();
                qDebug() << "Preparing to remove item:" << fileName;

                if (itemRowMap.contains(item)) {
                    int row = itemRowMap.take(item);
                    ui->downloadsTable->removeRow(row);
                }

                list.removeAt(i);

                QString filePath = item->getFullFilePath();
                if (QFile::exists(filePath)) {
                    if (!QFile::remove(filePath)) {
                        qWarning() << "Failed to remove main file:" << filePath;
                    }
                }

                if (!item->isSingleChunk()) {
                    for (int chunk = 0; chunk < item->getNumChunks(); ++chunk) {
                        QString chunkFileName = QString("%1.chunk%2").arg(filePath).arg(chunk);
                        if (QFile::exists(chunkFileName)) {
                            if (!QFile::remove(chunkFileName)) {
                                qWarning() << "Failed to remove chunk file:" << chunkFileName;
                            }
                        }
                    }
                }

                item->deleteLater();
                qDebug() << "Item" << fileName << "and its files have been removed.";
            }
        }
    }

    scheduleTableUpdate();
    saveDownloadHistory();
    qDebug() << "Cleanup complete.";
}

void MainWindow::openSpeedLimiterDialog()
{
    SpeedLimiterDialog dialog(this);

    dialog.setSpeedLimitEnabled(speedLimitEnabled);
    dialog.setSpeedLimitKBps(currentSpeedLimit / 1024);

    if (dialog.exec() == QDialog::Accepted) {
        speedLimitEnabled = dialog.isSpeedLimitEnabled();
        currentSpeedLimit = dialog.getSpeedLimitKBps() * 1024;

        for (auto& list : categories) {
            for (DownloadItem* item : list) {
                if (item) {
                    item->setSpeedLimit(speedLimitEnabled ? currentSpeedLimit : 0);
                }
            }
        }

        ui->actionOn->setChecked(speedLimitEnabled);
        scheduleTableUpdate();
    }
}

void MainWindow::toggleSpeedLimiter(bool enabled)
{
    speedLimitEnabled = enabled;

    if (enabled) {
        qint64 limit = currentSpeedLimit > 0 ? currentSpeedLimit : 100 * 1024;
        m_downloadManager->setGlobalSpeedLimit(limit, true);

        for (auto& list : categories) {
            for (DownloadItem* item : list) {
                if (item) {
                    item->setSpeedLimit(limit);
                }
            }
        }
    } else {
        for (auto& list : categories) {
            for (DownloadItem* item : list) {
                if (item) {
                    item->setSpeedLimit(0);
                }
            }
        }
    }

    ui->actionOn->setChecked(enabled);
    ui->actionOff->setChecked(!enabled);
    scheduleTableUpdate();
}

void MainWindow::checkForUpdates()
{
    QMessageBox::information(this, "Check for Updates", "Feature not implemented yet.");
}

void MainWindow::showAboutDialog()
{
    qDebug() << "showAboutDialog called, aboutDialog:" << aboutDialog;
    if (!aboutDialog) {
        qDebug() << "Creating new AboutDialog";
        aboutDialog = new AboutDialog(this);
        if (!aboutDialog) {
            qDebug() << "Failed to create AboutDialog";
            return;
        }
    } else {
        qDebug() << "Reusing existing AboutDialog";
    }
    qDebug() << "Showing AboutDialog";
    aboutDialog->show();
}

void MainWindow::openUserGuide()
{
    QDesktopServices::openUrl(QUrl("https://example.com/user-guide"));
}

void MainWindow::importDownloadList()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Import Download List", "", "JSON Files (*.json)");
    if (fileName.isEmpty()) return;
    loadDownloadListFromFile(fileName);
}

void MainWindow::exportDownloadList()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Download List", "", "JSON Files (*.json)");
    if (fileName.isEmpty()) return;
    saveDownloadListToFile(fileName);
}

void MainWindow::openConnectionSettings()
{
    ConnectionSettingsDialog dlg(this);
    dlg.setProxy(proxySettings);
    if (dlg.exec() == QDialog::Accepted) {
        proxySettings = dlg.proxy();
        m_downloadManager->setProxy(proxySettings);
    }
}

DownloadItem* MainWindow::getDownloadItemForRow(int row)
{
    QTableWidgetItem* itemCell = ui->downloadsTable->item(row, 0);
    if (!itemCell) return nullptr;
    QVariant v = itemCell->data(Qt::UserRole);
    if (v.isValid() && v.canConvert<DownloadItem*>()) {
        return v.value<DownloadItem*>();
    }
    return nullptr;
}

void MainWindow::refreshLinkForSelectedItem()
{
    int row = ui->downloadsTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a download to refresh.");
        return;
    }
    DownloadItem* item = getDownloadItemForRow(row);
    if (!item) {
        QMessageBox::warning(this, "Error", "Could not retrieve download item.");
        return;
    }
    QString newUrlStr = QInputDialog::getText(this, "Refresh Link", "Enter new URL:", QLineEdit::Normal, item->getUrl().toString());
    if (newUrlStr.isEmpty()) return;
    QUrl newUrl(newUrlStr);
    if (!newUrl.isValid()) {
        QMessageBox::warning(this, "Invalid URL", "Please enter a valid URL.");
        return;
    }
    item->setUrl(newUrl);
    item->setState(DownloadItem::Downloading);
    m_downloadManager->addToQueue(item);
    scheduleTableUpdate();
}

void MainWindow::scheduleTableUpdate()
{
    qDebug() << "Scheduling table update";
    if (!updateTimer->isActive()) {
        updateTimer->start();
    }
}

void MainWindow::updateDownloadTable()
{
    qDebug() << "Updating download table, rows:" << ui->downloadsTable->rowCount();
    QList<DownloadItem*> allDownloads;
    for (auto &list : categories) {
        allDownloads.append(list);
    }

    QSet<DownloadItem*> itemsInTable;
    for (int row = 0; row < ui->downloadsTable->rowCount(); ++row) {
        DownloadItem* item = getDownloadItemForRow(row);
        if (item) itemsInTable.insert(item);
    }

    bool updated = false;
    for (DownloadItem* item : allDownloads) {
        if (!item) continue;
        int row;
        if (!itemRowMap.contains(item)) {
            row = ui->downloadsTable->rowCount();
            ui->downloadsTable->insertRow(row);
            itemRowMap[item] = row;
            qDebug() << "Inserted new row" << row << "for" << item->getFileName();
            updated = true;
        } else {
            row = itemRowMap[item];
        }

        itemsInTable.remove(item);

        QString fileName = item->getFileName();
        if (fileName.isEmpty() || fileName.contains(QRegularExpression("[\\x00-\\x1F\\x7F-\\x9F]"))) {
            fileName = "unnamed_file_" + QString::number(row);
            qDebug() << "Invalid filename detected, replaced with" << fileName;
        }
        qint64 totalSize = item->getTotalSize();
        qint64 downloadedSize = item->getDownloadedSize();
        QString status = [item]() {
            switch (item->getState()) {
            case DownloadItem::Queued: return "Queued";
            case DownloadItem::Downloading: return "Downloading";
            case DownloadItem::Paused: return "Paused";
            case DownloadItem::Stopped: return "Stopped";
            case DownloadItem::Completed: return "Completed";
            case DownloadItem::Failed: return "Failed";
            default: return "Unknown";
            }
        }();
        int queuePosition = -1;
        if (m_downloadManager->isItemActive(item)) {
            queuePosition = 0;
        } else {
            int posInQueue = m_downloadManager->getQueuePosition(item);
            if (posInQueue >= 0) {
                queuePosition = posInQueue + 1;
            }
        }
        qint64 transferRate = item->getTransferRate();
        QDateTime lastTryDate = item->getLastTryDate();
        QString description = item->getDescription();

        QTableWidgetItem* existingItem = ui->downloadsTable->item(row, 0);
        bool rowNeedsUpdate = !existingItem ||
                              existingItem->text() != fileName ||
                              ui->downloadsTable->item(row, 1)->text() != formatSize(totalSize) ||
                              ui->downloadsTable->item(row, 2)->text() != status ||
                              ui->downloadsTable->item(row, 3)->text() != (queuePosition >= 0 ? QString::number(queuePosition) : "-") ||
                              ui->downloadsTable->item(row, 5)->text() != (transferRate > 0 ? QString("%1/s").arg(formatSize(transferRate).remove(QRegularExpression("\\s*[A-Z]+"))) : "-") ||
                              ui->downloadsTable->item(row, 4)->text() != (status == "Downloading" && transferRate > 0 && totalSize > 0 ? formatTimeLeft((totalSize - downloadedSize) / (transferRate > 0 ? transferRate : 1)) : "-");

        if (rowNeedsUpdate) {
            qDebug() << "Updating row" << row << "for" << fileName
                     << "Size:" << totalSize << "Downloaded:" << downloadedSize
                     << "Status:" << status << "Queue Pos:" << queuePosition;
            updated = true;

            QString sizeText = formatSize(totalSize);
            QString timeLeft = "-";
            if (status == "Downloading" && transferRate > 0 && totalSize > 0) {
                qint64 remaining = totalSize - downloadedSize;
                qint64 secondsLeft = remaining / (transferRate > 0 ? transferRate : 1);
                timeLeft = formatTimeLeft(secondsLeft);
            }

            QString rateText = transferRate > 0 ? QString("%1/s").arg(formatSize(transferRate).remove(QRegularExpression("\\s*[A-Z]+"))) : "-";

            QTableWidgetItem* itemCell = new QTableWidgetItem(fileName);
            itemCell->setData(Qt::UserRole, QVariant::fromValue(item));
            itemCell->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            QTableWidgetItem* sizeItem = new QTableWidgetItem(sizeText);
            sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            QTableWidgetItem* statusItem = new QTableWidgetItem(status);
            statusItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

            QTableWidgetItem* queueItem = new QTableWidgetItem(queuePosition >= 0 ? QString::number(queuePosition) : "-");
            queueItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

            QTableWidgetItem* timeItem = new QTableWidgetItem(timeLeft);
            timeItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

            QTableWidgetItem* rateItem = new QTableWidgetItem(rateText);
            rateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            QTableWidgetItem* dateItem = new QTableWidgetItem(lastTryDate.isValid() ? lastTryDate.toString("yyyy-MM-dd hh:mm") : "-");
            dateItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

            QTableWidgetItem* descItem = new QTableWidgetItem(description);
            descItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            ui->downloadsTable->setItem(row, 0, itemCell);
            ui->downloadsTable->setItem(row, 1, sizeItem);
            ui->downloadsTable->setItem(row, 2, statusItem);
            ui->downloadsTable->setItem(row, 3, queueItem);
            ui->downloadsTable->setItem(row, 4, timeItem);
            ui->downloadsTable->setItem(row, 5, rateItem);
            ui->downloadsTable->setItem(row, 6, dateItem);
            ui->downloadsTable->setItem(row, 7, descItem);
        }
    }

    for (DownloadItem* item : itemsInTable) {
        if (itemRowMap.contains(item)) {
            int row = itemRowMap[item];
            ui->downloadsTable->removeRow(row);
            itemRowMap.remove(item);
            qDebug() << "Removed stale row for" << item->getFileName();
            updated = true;
        }
    }

    if (updated) {
        ui->downloadsTable->resizeColumnsToContents();
        ui->downloadsTable->viewport()->update();
        QApplication::processEvents();
    } else {
        qDebug() << "No changes in download table, skipping UI update";
    }
}

void MainWindow::saveDownloadListToFile(const QString &filename)
{
    QJsonArray jsonArray;
    for (const auto& list : categories) {
        for (DownloadItem* item : list) {
            if (!item) continue;
            QJsonObject obj;
            obj["url"] = item->getUrl().toString();
            obj["fileName"] = item->getFileName();
            obj["totalSize"] = QString::number(item->getTotalSize());
            obj["downloadedSize"] = QString::number(item->getDownloadedSize());
            obj["status"] = [item]() {
                switch (item->getState()) {
                case DownloadItem::Queued: return "Queued";
                case DownloadItem::Downloading: return "Downloading";
                case DownloadItem::Paused: return "Paused";
                case DownloadItem::Stopped: return "Stopped";
                case DownloadItem::Completed: return "Completed";
                case DownloadItem::Failed: return "Failed";
                default: return "Unknown";
                }
            }();
            obj["lastTryDate"] = item->getLastTryDate().toString(Qt::ISODate);
            obj["description"] = item->getDescription();
            jsonArray.append(obj);
        }
    }

    QJsonDocument doc(jsonArray);
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Save Error", "Cannot save download list.");
        return;
    }
    file.write(doc.toJson());
    file.close();
}

void MainWindow::loadDownloadListFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Load Error", "Cannot load download list.");
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::warning(this, "Import Failed", "Invalid JSON format.");
        return;
    }

    QJsonArray jsonArray = doc.array();
    for (auto &list : categories) {
        qDeleteAll(list);
        list.clear();
    }

    for (const QJsonValue &val : jsonArray) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        QUrl url(obj["url"].toString());
        QString fileName = obj["fileName"].toString();
        QString path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileName;

        DownloadItem *item = new DownloadItem(url, path, this);
        item->setFileName(fileName);
        item->setTotalSize(obj["totalSize"].toString().toLongLong());
        item->setDownloadedSize(obj["downloadedSize"].toString().toLongLong());
        QString statusStr = obj["status"].toString();
        if (statusStr == "Paused") item->setState(DownloadItem::Paused);
        else if (statusStr == "Queued") item->setState(DownloadItem::Queued);
        else if (statusStr == "Downloading") item->setState(DownloadItem::Downloading);
        else if (statusStr == "Stopped") item->setState(DownloadItem::Stopped);
        else if (statusStr == "Completed") item->setState(DownloadItem::Completed);
        else if (statusStr == "Failed") item->setState(DownloadItem::Failed);
        item->setLastTryDate(QDateTime::fromString(obj["lastTryDate"].toString(), Qt::ISODate));
        item->setDescription(obj["description"].toString());
        item->setNumChunks(8);

        categories["All Downloads"].append(item);
        connect(item, &DownloadItem::progress, this, &MainWindow::handleDownloadProgress, Qt::QueuedConnection);
        connect(item, &DownloadItem::finished, this, &MainWindow::handleDownloadFinished, Qt::QueuedConnection);
        connect(item, &DownloadItem::failed, this, &MainWindow::handleDownloadFailed, Qt::QueuedConnection);
        connect(item, &DownloadItem::stateChanged, this, &MainWindow::scheduleTableUpdate, Qt::QueuedConnection);

        if (item->getState() != DownloadItem::Completed && item->getState() != DownloadItem::Paused) {
            m_downloadManager->addToQueue(item);
        }
    }
    scheduleTableUpdate();
}

void MainWindow::saveDownloadHistory()
{
    QJsonArray jsonArray;
    for (const auto& list : categories) {
        for (DownloadItem* item : list) {
            if (!item) continue;
            QJsonObject itemObj;
            itemObj["url"] = item->getUrl().toString();
            itemObj["filePath"] = item->getFullFilePath();
            itemObj["fileName"] = item->getFileName();
            itemObj["status"] = [item]() {
                switch (item->getState()) {
                case DownloadItem::Queued: return "Queued";
                case DownloadItem::Downloading: return "Downloading";
                case DownloadItem::Paused: return "Paused";
                case DownloadItem::Stopped: return "Stopped";
                case DownloadItem::Completed: return "Completed";
                case DownloadItem::Failed: return "Failed";
                default: return "Unknown";
                }
            }();
            itemObj["downloadedSize"] = QString::number(item->getDownloadedSize());
            itemObj["totalSize"] = QString::number(item->getTotalSize());
            itemObj["lastTryDate"] = item->getLastTryDate().toString(Qt::ISODate);
            itemObj["description"] = item->getDescription();
            itemObj["paused"] = (item->getState() == DownloadItem::Paused);
            jsonArray.append(itemObj);
        }
    }

    QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    QFile file(path + "/download_history.json");
    if (jsonArray.isEmpty()) {
        if (file.exists()) {
            qDebug() << "Download list empty. Removing history file...";
            file.remove();
        }
        return;
    }

    QJsonDocument doc(jsonArray);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "Saved download history with" << jsonArray.size() << "items.";
    }
}

void MainWindow::loadDownloadHistory()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString fullPath = path + "/download_history.json";

    QFile file(fullPath);
    if (!file.exists()) {
        qDebug() << "No download history file found at:" << fullPath;
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open download history file:" << fullPath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "Invalid JSON format in download history.";
        return;
    }

    QJsonArray jsonArray = doc.array();
    for (const QJsonValue &val : jsonArray) {
        if (!val.isObject()) continue;
        QJsonObject itemObj = val.toObject();

        QUrl url(itemObj["url"].toString());
        QString filePath = itemObj["filePath"].toString();
        if (!url.isValid()) continue;

        auto *item = new DownloadItem(url, filePath, this);
        item->setFileName(itemObj["fileName"].toString());
        item->setTotalSize(itemObj["totalSize"].toString().toLongLong());
        item->setDownloadedSize(itemObj["downloadedSize"].toString().toLongLong());
        QString statusStr = itemObj["status"].toString();
        if (statusStr == "Paused") item->setState(DownloadItem::Paused);
        else if (statusStr == "Queued") item->setState(DownloadItem::Queued);
        else if (statusStr == "Downloading") item->setState(DownloadItem::Downloading);
        else if (statusStr == "Stopped") item->setState(DownloadItem::Stopped);
        else if (statusStr == "Completed") item->setState(DownloadItem::Completed);
        else if (statusStr == "Failed") item->setState(DownloadItem::Failed);
        item->setLastTryDate(QDateTime::fromString(itemObj["lastTryDate"].toString(), Qt::ISODate));
        item->setDescription(itemObj["description"].toString());
        item->setNumChunks(8);

        connect(item, &DownloadItem::progress, this, &MainWindow::handleDownloadProgress, Qt::QueuedConnection);
        connect(item, &DownloadItem::finished, this, &MainWindow::handleDownloadFinished, Qt::QueuedConnection);
        connect(item, &DownloadItem::failed, this, &MainWindow::handleDownloadFailed, Qt::QueuedConnection);
        connect(item, &DownloadItem::stateChanged, this, &MainWindow::scheduleTableUpdate, Qt::QueuedConnection);

        if (!categories.contains("All Downloads")) {
            categories["All Downloads"] = QList<DownloadItem*>();
        }
        categories["All Downloads"].append(item);

        if (item->getState() != DownloadItem::Completed && item->getState() != DownloadItem::Paused) {
            m_downloadManager->addToQueue(item);
        }
    }
    qDebug() << "Loaded" << jsonArray.size() << "download items from history.";
    scheduleTableUpdate();
}

void MainWindow::openPreferences()
{
    PreferencesDialog dialog(this);
    dialog.setDefaultFolder(defaultDownloadFolder);
    dialog.setAutoStart(autoStartDownloads);
    dialog.setPromptBeforeOverwrite(promptBeforeOverwrite);
    dialog.setFileNamingPolicy(fileNamingPolicy);
    dialog.setMaxConcurrentDownloads(maxConcurrentDownloads);

    if (dialog.exec() == QDialog::Accepted) {
        defaultDownloadFolder = dialog.getDefaultFolder();
        autoStartDownloads = dialog.isAutoStartEnabled();
        promptBeforeOverwrite = dialog.shouldPromptBeforeOverwrite();
        fileNamingPolicy = dialog.getFileNamingPolicy();
        maxConcurrentDownloads = dialog.getMaxConcurrentDownloads();
        m_downloadManager->setMaxConcurrentDownloads(maxConcurrentDownloads);
        savePreferences();
    }
}

void MainWindow::loadPreferences()
{
    defaultDownloadFolder = settings.value("defaultFolder", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
    autoStartDownloads = settings.value("autoStart", true).toBool();
    promptBeforeOverwrite = settings.value("promptBeforeOverwrite", true).toBool();
    fileNamingPolicy = settings.value("fileNamingPolicy", "Use original name").toString();
    maxConcurrentDownloads = settings.value("maxConcurrentDownloads", 3).toInt();
    if (m_downloadManager) {
        m_downloadManager->setMaxConcurrentDownloads(maxConcurrentDownloads);
    }
}

void MainWindow::savePreferences()
{
    settings.setValue("defaultFolder", defaultDownloadFolder);
    settings.setValue("autoStart", autoStartDownloads);
    settings.setValue("promptBeforeOverwrite", promptBeforeOverwrite);
    settings.setValue("fileNamingPolicy", fileNamingPolicy);
    settings.setValue("maxConcurrentDownloads", maxConcurrentDownloads);
    settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePreferences();
    saveDownloadHistory();
    event->accept();
}

void MainWindow::addCategory(const QString& category)
{
    if (!categories.contains(category)) {
        categories[category] = QList<DownloadItem*>();
    }
}

void MainWindow::toggleStatusbar()
{
    ui->statusBar->setVisible(!ui->statusBar->isVisible());
}

void MainWindow::updateDownloadSpeed(DownloadItem *item)
{
    Q_UNUSED(item);
}

void MainWindow::startDownload(DownloadItem *item)
{
    if (item) {
        item->setState(DownloadItem::Queued);
        m_downloadManager->addToQueue(item);
        scheduleTableUpdate();
    }
}

void MainWindow::downloadFromYouTube()
{
    YouTubeDownloadDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString url = dialog.getUrl();
        if (!isYouTubeUrl(url)) {
            QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid YouTube URL."));
            return;
        }
        QUrl youtubeUrl(url);
        QString filePath = dialog.getOutputPath();
        if (!filePath.isEmpty()) {
            QString fileName = dialog.getYtdlArgs().contains("--extract-audio") ? "audio.mp3" : "video.mp4";
            auto *item = new DownloadItem(youtubeUrl, filePath + "/" + fileName, this);
            item->setNumChunks(8);
            item->setState(DownloadItem::Queued);
            item->setLastTryDate(QDateTime::currentDateTime());
            item->setDescription("YouTube download");

            categories["All Downloads"].append(item);

            connect(item, &DownloadItem::progress, this, &MainWindow::handleDownloadProgress, Qt::QueuedConnection);
            connect(item, &DownloadItem::finished, this, &MainWindow::handleDownloadFinished, Qt::QueuedConnection);
            connect(item, &DownloadItem::failed, this, &MainWindow::handleDownloadFailed, Qt::QueuedConnection);
            connect(item, &DownloadItem::stateChanged, this, &MainWindow::scheduleTableUpdate, Qt::QueuedConnection);

            m_downloadManager->downloadYouTubeWithOptions(item, dialog.getYtdlArgs());
        }
    }
}

bool MainWindow::isYouTubeUrl(const QString &url)
{
    QUrl qurl(url);
    QString host = qurl.host().toLower();
    QString path = qurl.path().toLower();
    return (host.contains("youtube.com") || host.contains("youtu.be")) &&
           !path.contains("channel") && !path.contains("user"); // Avoid non-video pages
}
