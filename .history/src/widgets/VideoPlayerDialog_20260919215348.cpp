#include "VideoPlayerDialog.h"

#include <QAudioOutput>
#include <QHBoxLayout>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

VideoPlayerDialog::VideoPlayerDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_videoWidget(new QVideoWidget(this))
    , m_positionSlider(new QSlider(Qt::Horizontal, this))
{
    setWindowTitle(tr("Video Player - %1").arg(filePath));
    resize(960, 640);

    auto *playButton = new QPushButton(tr("Play / Pause"), this);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_videoWidget, 1);
    layout->addWidget(m_positionSlider);

    auto *controls = new QHBoxLayout;
    controls->addWidget(playButton);
    layout->addLayout(controls);

    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setSource(QUrl::fromLocalFile(filePath));

    connect(playButton, &QPushButton::clicked, this, [this]() {
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_player->pause();
        } else {
            m_player->play();
        }
    });
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &VideoPlayerDialog::updatePosition);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &VideoPlayerDialog::updateDuration);
    connect(m_positionSlider, &QSlider::sliderMoved,
            this, &VideoPlayerDialog::seek);

    m_player->play();
}

void VideoPlayerDialog::updatePosition(qint64 position)
{
    if (!m_positionSlider->isSliderDown()) {
        m_positionSlider->setValue(static_cast<int>(position));
    }
}

void VideoPlayerDialog::updateDuration(qint64 duration)
{
    m_positionSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayerDialog::seek(int position)
{
    m_player->setPosition(position);
}
