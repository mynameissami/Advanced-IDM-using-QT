#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QList>

inline QString formatSize(qint64 bytes) {
    if (bytes < 0) return "Unknown";
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        ++unitIndex;
    }
    return QString("%1 %2").arg(size, 0, 'f', unitIndex > 0 ? 2 : 0).arg(units[unitIndex]);
}

inline QString formatTimeLeft(qint64 seconds) {
    if (seconds < 0) return "-";
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    QStringList parts;
    if (hours > 0) parts << QString("%1h").arg(hours);
    if (minutes > 0 || hours > 0) parts << QString("%1m").arg(minutes, 2, 10, QChar('0'));
    parts << QString("%1s").arg(secs, 2, 10, QChar('0'));

    return parts.join(" ");
}
inline qint64 parseSize(const QString& sizeStr) {
    bool ok;
    qreal value = sizeStr.left(sizeStr.size() - 1).toDouble(&ok);
    if (!ok) return 0;
    QString unit = sizeStr.right(1).toUpper();
    if (unit == "B") return static_cast<qint64>(value);
    if (unit == "K") return static_cast<qint64>(value * 1024);
    if (unit == "M") return static_cast<qint64>(value * 1024 * 1024);
    if (unit == "G") return static_cast<qint64>(value * 1024 * 1024 * 1024);
    return 0;
}

#endif // UTILS_H
