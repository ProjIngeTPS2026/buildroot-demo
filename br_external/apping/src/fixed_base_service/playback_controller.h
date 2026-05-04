#pragma once

#include "common/models.h"

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QMediaPlayer>
#else
#include <QAudioOutput>
#include <QMediaPlayer>
#endif
#include <QObject>
#include <QTimer>

namespace apping {

class LibraryManager;

class PlaybackController : public QObject {
    Q_OBJECT

public:
    explicit PlaybackController(const QString& audioOutputUri,
                                LibraryManager* libraryManager,
                                QObject* parent = nullptr);

    bool play(const PlaybackRequest& request, QString* error = nullptr);
    void stop();
    void interruptForLive();
    QString audioState() const;

signals:
    void stateChanged();

private:
    QString m_audioOutputUri;
    LibraryManager* m_libraryManager = nullptr;
    QMediaPlayer m_player;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput m_audioOutput;
#endif
    QTimer m_restartTimer;
    PlaybackRequest m_request;
    QString m_currentAssetPath;
    QDateTime m_deadline;
    bool m_active = false;

    bool shouldRepeat() const;
    void configureOutputDevice();
    void startCurrentAsset();
};

} // namespace apping
