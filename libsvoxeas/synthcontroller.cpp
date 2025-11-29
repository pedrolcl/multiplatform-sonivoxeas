/*
    Sonivox EAS Synthesizer for Qt applications
    Copyright (C) 2016-2023, Pedro Lopez-Cabanillas <plcl@users.sf.net>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <QDebug>
#include "synthcontroller.h"
#include "synthrenderer.h"

SynthController::SynthController(int bufTime, QObject *parent)
    : QObject(parent),
    m_requestedBufferTime(bufTime),
    m_running(false)
{
    qDebug() << Q_FUNC_INFO;
    m_renderer.reset(new SynthRenderer());
    m_format = m_renderer->format();
    initAudioDevices();
    initAudio();
    connect(&m_stallDetector, &QTimer::timeout, this, [=]{
        if (m_running) {
            if (m_renderer->lastBufferSize() == 0) {
                emit stallDetected();
            }
            m_renderer->resetLastBufferSize();
        }
    });
}

SynthController::~SynthController()
{
    qDebug() << Q_FUNC_INFO;
}

void
SynthController::start()
{
    qDebug() << Q_FUNC_INFO;
    auto bufferBytes = m_format.bytesForDuration(m_requestedBufferTime * 1000);
    qDebug() << Q_FUNC_INFO
             << "Requested buffer size:" << bufferBytes << "bytes,"
             << m_requestedBufferTime << "milliseconds";
    m_renderer->reserveBuffer(bufferBytes * 2);
    m_renderer->start();
    m_audioOutput->setBufferSize(bufferBytes);
    m_audioOutput->start(m_renderer.get());
    auto bufferTime = m_format.durationForBytes(m_audioOutput->bufferSize()) / 1000;
    qDebug() << Q_FUNC_INFO
             << "Applied Audio Output buffer size:" << m_audioOutput->bufferSize() << "bytes,"
             << bufferTime << "milliseconds";
    QTimer::singleShot(bufferTime * 2, this, [=]{
        m_running = true;
        m_stallDetector.start(bufferTime * 4);
     });
}

void
SynthController::stop()
{
    qDebug() << Q_FUNC_INFO;
    m_running = false;
    m_stallDetector.stop();
    if (!m_audioOutput.isNull()) {
        m_audioOutput->stop();
    }
    if(!m_renderer.isNull()) {
        m_renderer->stop();
    }
}

SynthRenderer*
SynthController::renderer() const
{
    return m_renderer.get();
}

void
SynthController::initAudio()
{
    qDebug() << Q_FUNC_INFO;
    if (!m_audioDevice.isFormatSupported(m_format)) {
        qCritical() << Q_FUNC_INFO << "Audio format not supported" << m_format;
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_audioOutput.reset(new QAudioOutput(m_audioDevice, m_format));
    m_audioOutput->setCategory("MIDI Synthesizer");
    QObject::connect(m_audioOutput.data(), &QAudioOutput::stateChanged, this, [=](QAudio::State state){
#else
    m_audioOutput.reset(new QAudioSink(m_audioDevice, m_format));
    QObject::connect(m_audioOutput.data(), &QAudioSink::stateChanged, this, [=](QAudio::State state){
#endif
        qDebug() << "Audio Output state:" << state << "error:" << m_audioOutput->error();
        if (m_running && (m_audioOutput->error() == QAudio::UnderrunError)) {
            emit underrunDetected();
        }
    });
}

void
SynthController::initAudioDevices()
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    m_audioDevice = QAudioDeviceInfo::defaultOutputDevice();
    foreach(auto &dev, devices) {
        if (dev.isFormatSupported(m_format)) {
            qDebug() << Q_FUNC_INFO << dev.deviceName();
            m_availableDevices.insert(dev.deviceName(), dev);
        }
    }
#else
    QMediaDevices mediaDevices;
    auto devices = mediaDevices.audioOutputs();
    m_audioDevice = mediaDevices.defaultAudioOutput();
    foreach(auto &dev, devices) {
        if (dev.isFormatSupported(m_format)) {
            qDebug() << Q_FUNC_INFO << dev.description();
            m_availableDevices.insert(dev.description(), dev);
        }
    }
#endif
    qDebug() << Q_FUNC_INFO << audioDeviceName();
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
const QAudioDeviceInfo&
SynthController::audioDevice() const
{
    qDebug() << Q_FUNC_INFO;
    return m_audioDevice;
}

void
SynthController::setAudioDevice(const QAudioDeviceInfo &newAudioDevice)
{
    qDebug() << Q_FUNC_INFO;
    m_audioDevice = newAudioDevice;
}
#else
const QAudioDevice&
SynthController::audioDevice() const
{
    qDebug() << Q_FUNC_INFO;
    return m_audioDevice;
}

void
SynthController::setAudioDevice(const QAudioDevice &newAudioDevice)
{
    qDebug() << Q_FUNC_INFO;
    m_audioDevice = newAudioDevice;
}
#endif

QStringList
SynthController::availableAudioDevices() const
{
    qDebug() << Q_FUNC_INFO;
    return m_availableDevices.keys();
}

QString
SynthController::audioDeviceName() const
{
    qDebug() << Q_FUNC_INFO;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return m_audioDevice.deviceName();
#else
    return m_audioDevice.description();
#endif
}

void
SynthController::setAudioDeviceName(const QString newName)
{
    qDebug() << Q_FUNC_INFO << newName;
    if (m_availableDevices.contains(newName) &&
        (m_audioDevice.isNull() || (audioDeviceName() != newName) )) {
        stop();
        m_audioDevice = m_availableDevices.value(newName);
        initAudio();
        start();
    }
}

void SynthController::setBufferSize(int milliseconds)
{
    qDebug() << Q_FUNC_INFO << milliseconds;
    if (milliseconds != m_requestedBufferTime) {
        stop();
        m_requestedBufferTime = milliseconds;
        start();
    }
}

void SynthController::setVolume(int volume)
{
    qDebug() << Q_FUNC_INFO << volume;
    qreal linearVolume = QAudio::convertVolume(volume / 100.0,
                                               QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);
    m_audioOutput->setVolume(linearVolume);
}
