#include "youtubedownloaddialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLabel>

YouTubeDownloadDialog::YouTubeDownloadDialog(QWidget *parent)
    : QDialog(parent), outputPath(QDir::homePath() + "/Downloads")
{
    setWindowTitle("Advanced YouTube Download Options");
    setMinimumWidth(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // URL Input
    QHBoxLayout *urlLayout = new QHBoxLayout();
    urlLayout->addWidget(new QLabel("YouTube URL:"));
    urlEdit = new QLineEdit(this);
    urlLayout->addWidget(urlEdit);
    mainLayout->addLayout(urlLayout);

    // Format Selection
    QHBoxLayout *formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel("Format:"));
    formatCombo = new QComboBox(this);
    formatCombo->addItems({"Best (1080p)", "720p", "480p", "360p", "Best Audio", "Custom Format ID"});
    formatLayout->addWidget(formatCombo);
    mainLayout->addLayout(formatLayout);

    // Audio Only
    audioOnlyCheck = new QCheckBox("Extract Audio Only", this);
    mainLayout->addWidget(audioOnlyCheck);

    // Subtitles
    QHBoxLayout *subLayout = new QHBoxLayout();
    subtitlesCheck = new QCheckBox("Download Subtitles", this);
    subLangCombo = new QComboBox(this);
    subLangCombo->addItems({"en", "es", "fr", "de", "all"});
    subLangCombo->setEnabled(false);
    subLayout->addWidget(subtitlesCheck);
    subLayout->addWidget(new QLabel("Language:"));
    subLayout->addWidget(subLangCombo);
    mainLayout->addLayout(subLayout);
    connect(subtitlesCheck, &QCheckBox::toggled, subLangCombo, &QComboBox::setEnabled);

    // Playlist
    playlistCheck = new QCheckBox("Download Entire Playlist", this);
    mainLayout->addWidget(playlistCheck);

    // Live Options
    QHBoxLayout *liveLayout = new QHBoxLayout();
    liveCheck = new QCheckBox("Download Live Stream", this);
    liveWaitSpin = new QSpinBox(this);
    liveWaitSpin->setRange(0, 3600);
    liveWaitSpin->setSuffix(" seconds");
    liveWaitSpin->setEnabled(false);
    liveLayout->addWidget(liveCheck);
    liveLayout->addWidget(new QLabel("Wait Time:"));
    liveLayout->addWidget(liveWaitSpin);
    mainLayout->addLayout(liveLayout);
    connect(liveCheck, &QCheckBox::toggled, liveWaitSpin, &QSpinBox::setEnabled);

    // Authentication
    QHBoxLayout *authLayout = new QHBoxLayout();
    authLayout->addWidget(new QLabel("Username (for private videos):"));
    usernameEdit = new QLineEdit(this);
    authLayout->addWidget(usernameEdit);
    mainLayout->addLayout(authLayout);

    QHBoxLayout *passLayout = new QHBoxLayout();
    passLayout->addWidget(new QLabel("Password:"));
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passLayout->addWidget(passwordEdit);
    mainLayout->addLayout(passLayout);

    // Proxy
    QHBoxLayout *proxyLayout = new QHBoxLayout();
    proxyLayout->addWidget(new QLabel("Proxy (e.g., http://proxy:8080):"));
    proxyEdit = new QLineEdit(this);
    proxyLayout->addWidget(proxyEdit);
    mainLayout->addLayout(proxyLayout);

    // Merge Options
    mergeCheck = new QCheckBox("Merge Video and Audio", this);
    mainLayout->addWidget(mergeCheck);

    QHBoxLayout *mergeFormatLayout = new QHBoxLayout();
    mergeFormatCombo = new QComboBox(this);
    mergeFormatCombo->addItems({"mp4", "mkv", "webm"});
    mergeFormatCombo->setEnabled(false);
    mergeFormatLayout->addWidget(new QLabel("Merge Format:"));
    mergeFormatLayout->addWidget(mergeFormatCombo);
    mainLayout->addLayout(mergeFormatLayout);
    connect(mergeCheck, &QCheckBox::toggled, mergeFormatCombo, &QComboBox::setEnabled);

    // Recode
    recodeCheck = new QCheckBox("Recode Video", this);
    mainLayout->addWidget(recodeCheck);

    QHBoxLayout *recodeFormatLayout = new QHBoxLayout();
    recodeFormatCombo = new QComboBox(this);
    recodeFormatCombo->addItems({"mp4", "mkv", "webm"});
    recodeFormatCombo->setEnabled(false);
    recodeFormatLayout->addWidget(new QLabel("Recode Format:"));
    recodeFormatLayout->addWidget(recodeFormatCombo);
    mainLayout->addLayout(recodeFormatLayout);
    connect(recodeCheck, &QCheckBox::toggled, recodeFormatCombo, &QComboBox::setEnabled);

    // Embed Thumbnail
    embedThumbnailCheck = new QCheckBox("Embed Thumbnail", this);
    mainLayout->addWidget(embedThumbnailCheck);

    // Remux
    remuxCheck = new QCheckBox("Remux Video", this);
    mainLayout->addWidget(remuxCheck);

    QHBoxLayout *remuxFormatLayout = new QHBoxLayout();
    remuxFormatCombo = new QComboBox(this);
    remuxFormatCombo->addItems({"mp4", "mkv", "webm"});
    remuxFormatCombo->setEnabled(false);
    remuxFormatLayout->addWidget(new QLabel("Remux Format:"));
    remuxFormatLayout->addWidget(remuxFormatCombo);
    mainLayout->addLayout(remuxFormatLayout);
    connect(remuxCheck, &QCheckBox::toggled, remuxFormatCombo, &QComboBox::setEnabled);

    // Postprocessor Args
    QHBoxLayout *ppArgsLayout = new QHBoxLayout();
    ppArgsLayout->addWidget(new QLabel("Postprocessor Args (e.g., -vf scale=1280:720):"));
    postprocessorArgsEdit = new QLineEdit(this);
    ppArgsLayout->addWidget(postprocessorArgsEdit);
    mainLayout->addLayout(ppArgsLayout);

    // Sponsorblock
    QHBoxLayout *sponsorLayout = new QHBoxLayout();
    sponsorblockCheck = new QCheckBox("Use Sponsorblock", this);
    sponsorblockFileEdit = new QLineEdit(this);
    sponsorblockFileEdit->setEnabled(false);
    sponsorBrowseButton = new QPushButton("Browse", this);
    sponsorBrowseButton->setEnabled(false);
    sponsorLayout->addWidget(sponsorblockCheck);
    sponsorLayout->addWidget(new QLabel("Segments File:"));
    sponsorLayout->addWidget(sponsorblockFileEdit);
    sponsorLayout->addWidget(sponsorBrowseButton);
    mainLayout->addLayout(sponsorLayout);
    connect(sponsorblockCheck, &QCheckBox::toggled, sponsorblockFileEdit, &QLineEdit::setEnabled);
    connect(sponsorblockCheck, &QCheckBox::toggled, sponsorBrowseButton, &QPushButton::setEnabled);
    connect(sponsorBrowseButton, &QPushButton::clicked, this, &YouTubeDownloadDialog::browseSponsorblockFile);

    // Cookies
    QHBoxLayout *cookiesLayout = new QHBoxLayout();
    cookiesCheck = new QCheckBox("Use Cookies File", this);
    cookiesFileEdit = new QLineEdit(this);
    cookiesFileEdit->setEnabled(false);
    cookiesBrowseButton = new QPushButton("Browse", this);
    cookiesBrowseButton->setEnabled(false);
    cookiesLayout->addWidget(cookiesCheck);
    cookiesLayout->addWidget(new QLabel("Cookies File:"));
    cookiesLayout->addWidget(cookiesFileEdit);
    cookiesLayout->addWidget(cookiesBrowseButton);
    mainLayout->addLayout(cookiesLayout);
    connect(cookiesCheck, &QCheckBox::toggled, cookiesFileEdit, &QLineEdit::setEnabled);
    connect(cookiesCheck, &QCheckBox::toggled, cookiesBrowseButton, &QPushButton::setEnabled);
    connect(cookiesBrowseButton, &QPushButton::clicked, this, &YouTubeDownloadDialog::browseCookiesFile);

    // Output Path
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel("Save to:"));
    outputPathEdit = new QLineEdit(outputPath, this); // Store the pointer
    pathLayout->addWidget(outputPathEdit);
    browseButton = new QPushButton("Browse", this);
    connect(browseButton, &QPushButton::clicked, this, &YouTubeDownloadDialog::browseDirectory);
    pathLayout->addWidget(browseButton);
    mainLayout->addLayout(pathLayout);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("Download", this);
    QPushButton *cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    setLayout(mainLayout);
}

void YouTubeDownloadDialog::browseDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Download Directory"),
                                                    outputPath);
    if (!dir.isEmpty()) {
        outputPath = dir;
        if (outputPathEdit) {
            outputPathEdit->setText(outputPath);
        } else {
            qDebug() << "Error: outputPathEdit is null";
        }
    }
}

void YouTubeDownloadDialog::browseSponsorblockFile()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Select Sponsorblock File"),
                                                QDir::homePath(), tr("Text Files (*.txt)"));
    if (!file.isEmpty()) {
        sponsorblockFileEdit->setText(file);
    }
}

void YouTubeDownloadDialog::browseCookiesFile()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Select Cookies File"),
                                                QDir::homePath(), tr("Cookies Files (*.txt)"));
    if (!file.isEmpty()) {
        cookiesFileEdit->setText(file);
    }
}

QStringList YouTubeDownloadDialog::getYtdlArgs() const
{
    QStringList args;
    QString format = getFormat();
    if (!format.isEmpty()) args << "-f" << format;
    if (audioOnlyCheck->isChecked()) args << "--extract-audio" << "--audio-format" << "mp3";
    if (subtitlesCheck->isChecked()) args << "--write-subs" << "--sub-lang" << subLangCombo->currentText();
    if (playlistCheck->isChecked()) args << "--yes-playlist";
    if (liveCheck->isChecked()) args << "--live-from-start" << "--wait-for-video" << QString::number(liveWaitSpin->value());
    if (!usernameEdit->text().isEmpty()) args << "--username" << usernameEdit->text();
    if (!passwordEdit->text().isEmpty()) args << "--password" << passwordEdit->text();
    if (!proxyEdit->text().isEmpty()) args << "--proxy" << proxyEdit->text();
    if (mergeCheck->isChecked()) args << "--merge-output-format" << mergeFormatCombo->currentText();
    if (recodeCheck->isChecked()) args << "--recode-video" << recodeFormatCombo->currentText();
    if (embedThumbnailCheck->isChecked()) args << "--embed-thumbnail";
    if (remuxCheck->isChecked()) args << "--remux-video" << remuxFormatCombo->currentText();
    if (!postprocessorArgsEdit->text().isEmpty()) args << "--postprocessor-args" << postprocessorArgsEdit->text();
    if (sponsorblockCheck->isChecked() && !sponsorblockFileEdit->text().isEmpty())
        args << "--sponsorblock-list" << sponsorblockFileEdit->text();
    if (cookiesCheck->isChecked() && !cookiesFileEdit->text().isEmpty())
        args << "--cookies" << cookiesFileEdit->text();
    return args;
}

QString YouTubeDownloadDialog::getFormat() const
{
    switch (formatCombo->currentIndex()) {
    case 0: return "bestvideo[height<=?1080]+bestaudio/best";
    case 1: return "bestvideo[height<=?720]+bestaudio/best";
    case 2: return "bestvideo[height<=?480]+bestaudio/best";
    case 3: return "bestvideo[height<=?360]+bestaudio/best";
    case 4: return "bestaudio/best";
    case 5: return ""; // Custom format ID (user can specify via URL or future extension)
    default: return "bestvideo+bestaudio/best";
    }
}
