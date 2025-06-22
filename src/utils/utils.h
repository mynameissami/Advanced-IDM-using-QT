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

#endif // UTILS_H