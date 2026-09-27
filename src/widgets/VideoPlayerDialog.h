#ifndef VIDEOPLAYERDIALOG_H
#define VIDEOPLAYERDIALOG_H

#include <QDialog>

class QAudioOutput;
class QMediaPlayer;
class QSlider;
class QVideoWidget;

class VideoPlayerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit VideoPlayerDialog(const QString &filePath, QWidget *parent = nullptr);

private slots:
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void seek(int position);

private:
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QSlider *m_positionSlider = nullptr;
};

#endif // VIDEOPLAYERDIALOG_H
