/*
    Sonivox EAS Synthesizer for Qt applications
    Copyright (C) 2016-2025, Pedro Lopez-Cabanillas <plcl@users.sf.net>

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

//#include <QDebug>
#include "synthcontroller.h"
#include "synthrenderer.h"

SynthController::SynthController(int bufTime, QObject *parent)
    : QObject(parent)
    , m_renderer(new SynthRenderer)
    , m_requestedBufferTime(bufTime)
    , m_running(false)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    , m_devices(new QMediaDevices(this))
#endif
{
    m_format = m_renderer->format();
    //qDebug() << Q_FUNC_INFO << m_format;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_devices,
            &QMediaDevices::audioOutputsChanged,
            this,
            &SynthController::updateAudioDevices);
#endif
    updateAudioDevices();
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
    if (m_renderer) {
        if (!m_renderer->stopped()) {
            m_renderer->stop();
        }
        delete m_renderer;
        m_renderer = nullptr;
    }
    //qDebug() << Q_FUNC_INFO;
}

void
SynthController::start()
{
    auto bufferBytes = m_format.bytesForDuration(m_requestedBufferTime * 1000);
    // qDebug() << Q_FUNC_INFO
    //          << "Requested buffer size:" << bufferBytes << "bytes,"
    //          << m_requestedBufferTime << "milliseconds";
    if (!m_renderer) {
        m_renderer = new SynthRenderer();
    }
    if (m_renderer) {
        m_renderer->reserveBuffer(bufferBytes * 2);
        if(m_renderer->stopped()) {
            m_renderer->start();
        }
    }
    if (!m_audioOutput) {
        initAudio();
    }
    m_audioOutput->setBufferSize(bufferBytes);
    m_audioOutput->start(m_renderer);
    auto bufferTime = m_format.durationForBytes(m_audioOutput->bufferSize()) / 1000;
    // qDebug() << Q_FUNC_INFO << "Applied Audio Output buffer size:" << m_audioOutput->bufferSize()
    //          << "bytes," << bufferTime << "milliseconds";
    QTimer::singleShot(bufferTime * 2, this, [=]{
        m_running = true;
        m_stallDetector.start(bufferTime * 4);
     });
}

void
SynthController::stop()
{
    // qDebug() << Q_FUNC_INFO;
    m_running = false;
    m_stallDetector.stop();
    if (m_audioOutput) {
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
    if (m_renderer && !m_renderer->stopped()) {
        m_renderer->stop();
    }
    delete m_renderer;
    m_renderer = nullptr;
}

SynthRenderer*
SynthController::renderer() const
{
    return m_renderer;
}

void
SynthController::initAudio()
{
    // qDebug() << Q_FUNC_INFO << "audio device:" << m_audioDevice.description();
    if (!m_audioDevice.isFormatSupported(m_format)) {
        qCritical() << Q_FUNC_INFO << "Audio format not supported" << m_format;
        return;
    }
    Q_ASSERT_X(m_audioOutput == nullptr, Q_FUNC_INFO, "m_audioOutput is not null");
    if (!m_audioOutput) {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        m_audioOutput = new QAudioOutput(m_audioDevice, m_format);
        m_audioOutput->setCategory("MIDI Synthesizer");
        QObject::connect(m_audioOutput, &QAudioOutput::stateChanged, this, [=](QAudio::State state) {
#else
        m_audioOutput = new QAudioSink(m_audioDevice, m_format);
        QObject::connect(m_audioOutput, &QAudioSink::stateChanged, this, [=](QAudio::State state) {
#endif
            // qDebug() << "Audio Output state:" << state << "error:" << m_audioOutput->error();
            if (m_running && (m_audioOutput->error() == QAudio::UnderrunError)) {
                emit underrunDetected();
            }
        });
    }
}

void SynthController::updateAudioDevices()
{
    m_availableDevices.clear();
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const auto devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    m_audioDevice = QAudioDeviceInfo::defaultOutputDevice();
    foreach(auto &dev, devices) {
        // qDebug() << Q_FUNC_INFO << dev.deviceName() << dev.isFormatSupported(m_format);
        if (dev.isFormatSupported(m_format)) {
            m_availableDevices.insert(dev.deviceName(), dev);
        }
    }
#else
    const auto devices = m_devices->audioOutputs();
    m_audioDevice = m_devices->defaultAudioOutput();
    foreach(auto &dev, devices) {
        // qDebug() << Q_FUNC_INFO << dev.description() << dev.isFormatSupported(m_format);
        if (dev.isFormatSupported(m_format)) {
            m_availableDevices.insert(dev.description(), dev);
        }
    }
#endif
   // qDebug() << Q_FUNC_INFO << "current audio device:" << m_audioDevice.description();
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
const QAudioDeviceInfo&
SynthController::audioDevice() const
{
    //qDebug() << Q_FUNC_INFO;
    return m_audioDevice;
}

void
SynthController::setAudioDevice(const QAudioDeviceInfo &newAudioDevice)
{
    //qDebug() << Q_FUNC_INFO;
    m_audioDevice = newAudioDevice;
}
#else
const QAudioDevice&
SynthController::audioDevice() const
{
    // qDebug() << Q_FUNC_INFO << m_audioDevice.description();
    return m_audioDevice;
}

void
SynthController::setAudioDevice(const QAudioDevice &newAudioDevice)
{
    // qDebug() << Q_FUNC_INFO << newAudioDevice.description();
    m_audioDevice = newAudioDevice;
}
#endif

QStringList SynthController::availableAudioDevices()
{
    // qDebug() << Q_FUNC_INFO << m_availableDevices.keys();
    return m_availableDevices.keys();
}

QString
SynthController::audioDeviceName() const
{
    QString n =
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        m_audioDevice.deviceName();
#else
        m_audioDevice.description();
#endif
    // qDebug() << Q_FUNC_INFO << n;
    return n;
}

void
SynthController::setAudioDeviceName(const QString newName)
{
    // qDebug() << Q_FUNC_INFO << newName;
    if (m_availableDevices.contains(newName) &&
        (m_audioDevice.isNull() || (audioDeviceName() != newName) )) {
        stop();
        m_audioDevice = m_availableDevices.value(newName);
        start();
    } else {
        restart();
    }
}

void SynthController::setBufferSize(int milliseconds)
{
    //qDebug() << Q_FUNC_INFO << milliseconds;
    if (milliseconds != m_requestedBufferTime) {
        stop();
        m_requestedBufferTime = milliseconds;
        start();
    }
}

void SynthController::setVolume(int volume)
{
    //qDebug() << Q_FUNC_INFO << volume;
    qreal linearVolume = QAudio::convertVolume(volume / 100.0,
                                               QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);
    if (m_audioOutput) {
        m_audioOutput->setVolume(linearVolume);
    }
}

void SynthController::restart()
{
    stop();
    start();
}
