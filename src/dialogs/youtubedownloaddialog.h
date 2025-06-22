#ifndef YOUTUBEDOWNLOADDIALOG_H
#define YOUTUBEDOWNLOADDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

class YouTubeDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YouTubeDownloadDialog(QWidget *parent = nullptr);
    QString getUrl() const { return urlEdit->text(); }
    QStringList getYtdlArgs() const;
    QString getOutputPath() const { return outputPath; }
    QString getSponsorblockFile() const { return sponsorblockFileEdit->text(); }
    QString getCookiesFile() const { return cookiesFileEdit->text(); }
    QString getFormat() const; // Add this declaration

private:
    QLineEdit *urlEdit;
    QComboBox *formatCombo;
    QCheckBox *audioOnlyCheck;
    QCheckBox *subtitlesCheck;
    QComboBox *subLangCombo;
    QCheckBox *playlistCheck;
    QCheckBox *liveCheck;
    QSpinBox *liveWaitSpin;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *proxyEdit;
    QCheckBox *mergeCheck;
    QComboBox *mergeFormatCombo;
    QCheckBox *recodeCheck;
    QComboBox *recodeFormatCombo;
    QCheckBox *embedThumbnailCheck;
    QCheckBox *remuxCheck;
    QComboBox *remuxFormatCombo;
    QLineEdit *postprocessorArgsEdit;
    QCheckBox *sponsorblockCheck;
    QLineEdit *sponsorblockFileEdit;
    QCheckBox *cookiesCheck;
    QLineEdit *cookiesFileEdit;
    QPushButton *browseButton;
    QPushButton *sponsorBrowseButton;
    QPushButton *cookiesBrowseButton;
    QLineEdit *outputPathEdit; // New member for output path QLineEdit
    QString outputPath;

private slots:
    void browseDirectory();
    void browseSponsorblockFile();
    void browseCookiesFile();
};

#endif // YOUTUBEDOWNLOADDIALOG_H