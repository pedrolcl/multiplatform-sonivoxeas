/*
    Sonivox EAS Synthesizer for Qt applications
    Copyright (C) 2016-2022, Pedro Lopez-Cabanillas <plcl@users.sf.net>

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

#include <QObject>
#include <QString>
#include <QCoreApplication>
#include <QTextStream>
#include <QtDebug>
#include <QReadLocker>
#include <QWriteLocker>
#include <eas_reverb.h>
#include <eas_chorus.h>
#include <drumstick/sequencererror.h>
#include "synthrenderer.h"
#include "filewrapper.h"

using namespace drumstick::rt;

SynthRenderer::SynthRenderer(int bufTime, QObject *parent) : QObject(parent),
    m_Stopped(true),
    m_isPlaying(false),
    m_input(nullptr),
	m_requestedBufferTime(bufTime)
{
    initMIDI();
    initEAS();
    initAudioDevices();
}

void
SynthRenderer::initMIDI()
{
    const QString DEFAULT_DRIVER = 
#if defined(Q_OS_LINUX)
        QStringLiteral("ALSA");
#elif defined(Q_OS_WINDOWS)
        QStringLiteral("Windows MM");
#elif defined(Q_OS_MACOS)
        QStringLiteral("CoreMIDI");
#elif defined(Q_OS_UNIX)
        QStringLiteral("OSS");
#else
        QStringLiteral("Network");
#endif
    qDebug() << Q_FUNC_INFO << DEFAULT_DRIVER;
    if (m_midiDriver.isEmpty()) {
        setMidiDriver(DEFAULT_DRIVER);
    }
    if (m_input == nullptr) {
        qWarning() << "Input Backend is Missing. You may need to set the DRUMSTICKRT environment variable";
    }
}

void
SynthRenderer::initEAS()
{
    /* SONiVOX EAS initialization */
    EAS_RESULT eas_res;
    EAS_DATA_HANDLE dataHandle;
    EAS_HANDLE handle;

    const S_EAS_LIB_CONFIG *easConfig = EAS_Config();
    if (easConfig == 0) {
        qCritical() << "EAS_Config returned null";
        return;
    }

    eas_res = EAS_Init(&dataHandle);
    if (eas_res != EAS_SUCCESS) {
      qCritical() << "EAS_Init error: " << eas_res;
      return;
    }

    eas_res = EAS_OpenMIDIStream(dataHandle, &handle, NULL);
    if (eas_res != EAS_SUCCESS) {
      qCritical() << "EAS_OpenMIDIStream error: " << eas_res;
      EAS_Shutdown(dataHandle);
      return;
    }

    m_easData = dataHandle;
    m_streamHandle = handle;
    assert(m_streamHandle != 0);
    m_sampleRate = easConfig->sampleRate;
    m_bufferSize = easConfig->mixBufferSize;
    m_channels = easConfig->numChannels;
    m_sample_size = CHAR_BIT * sizeof (EAS_PCM);
    qDebug() << Q_FUNC_INFO << "EAS bufferSize=" << m_bufferSize << " sampleRate=" << m_sampleRate << " channels=" << m_channels;
}

void
SynthRenderer::initAudio()
{
    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(m_channels);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    format.setSampleSize(m_sample_size);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
#else
    format.setSampleFormat(QAudioFormat::Int16);
#endif

    if (!m_audioDevice.isFormatSupported(format)) {
        qCritical() << "Audio format not supported" << format;
        return;
    }

    qint64 requested_size = m_channels * (m_sample_size / CHAR_BIT) * m_requestedBufferTime * m_sampleRate / 1000;
    qint64 period_bytes = m_channels * (m_sample_size / CHAR_BIT) * m_bufferSize;
    qDebug() << Q_FUNC_INFO << "requested buffer sizes:" << period_bytes << requested_size;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_audioOutput.reset(new QAudioOutput(m_audioDevice, format));
    m_audioOutput->setCategory("MIDI Synthesizer");
    QObject::connect(m_audioOutput.data(), &QAudioOutput::stateChanged, this, [](QAudio::State state){
#else
    m_audioOutput.reset(new QAudioSink(m_audioDevice, format));
    QObject::connect(m_audioOutput.data(), &QAudioSink::stateChanged, this, [](QAudio::State state){
#endif
        qDebug() << "Audio Output state changed:" << state;
    });
    m_audioOutput->setBufferSize( qMax(period_bytes, requested_size) );
}

void SynthRenderer::initAudioDevices()
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_availableDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    m_audioDevice = QAudioDeviceInfo::defaultOutputDevice();
#else
    QMediaDevices devices;
    m_availableDevices = devices.audioOutputs();
    m_audioDevice = devices.defaultAudioOutput();
#endif
    /*foreach(auto &dev, m_availableDevices) {
        qDebug() << Q_FUNC_INFO << dev.deviceName();
    }*/
}

SynthRenderer::~SynthRenderer()
{
    if (m_input != nullptr) {
        m_input->close();
    }

    EAS_RESULT eas_res;
    if (m_easData != 0 && m_streamHandle != 0) {
      eas_res = EAS_CloseMIDIStream(m_easData, m_streamHandle);
      if (eas_res != EAS_SUCCESS) {
          qWarning() << "EAS_CloseMIDIStream error: " << eas_res;
      }
      eas_res = EAS_Shutdown(m_easData);
      if (eas_res != EAS_SUCCESS) {
          qWarning() << "EAS_Shutdown error: " << eas_res;
      }
    }
    qDebug() << Q_FUNC_INFO;
}

bool
SynthRenderer::stopped()
{
	QReadLocker locker(&m_mutex);
    return m_Stopped;
}

void
SynthRenderer::stop()
{
	QWriteLocker locker(&m_mutex);
    qDebug() << Q_FUNC_INFO;
    m_Stopped = true;
}

QStringList 
SynthRenderer::connections() const
{
    Q_ASSERT(m_input != nullptr);
    QStringList result;
    auto avail = m_input->connections(true);
    foreach(const auto &c, avail) {
        result << c.first;
    }
    return result;
}

QString 
SynthRenderer::subscription() const
{
    return m_portName;
}

void
SynthRenderer::subscribe(const QString& portName)
{
    qDebug() << Q_FUNC_INFO << portName;
    Q_ASSERT(m_input != nullptr);
    auto avail = m_input->connections(true);
    auto it = std::find_if(avail.constBegin(), avail.constEnd(),
                           [portName](const MIDIConnection& c) { 
                               return c.first == portName; 
                           });
    m_input->close();
    if (it == avail.constEnd()) {
        MIDIConnection conn;
        m_input->open(conn);
    } else {
        m_input->open(*it);
    }
}

void
SynthRenderer::run()
{
    QByteArray audioData;
    initAudio();
    qDebug() << Q_FUNC_INFO << "started";
    try {
        if (m_input != nullptr) {
            m_input->disconnect();
            QObject::connect(m_input, &MIDIInput::midiNoteOn, this, &SynthRenderer::noteOn);
            QObject::connect(m_input, &MIDIInput::midiNoteOff, this, &SynthRenderer::noteOff);
            QObject::connect(m_input, &MIDIInput::midiKeyPressure, this, &SynthRenderer::keyPressure);
            QObject::connect(m_input, &MIDIInput::midiController, this, &SynthRenderer::controller);
            QObject::connect(m_input, &MIDIInput::midiProgram, this, &SynthRenderer::program);
            QObject::connect(m_input, &MIDIInput::midiChannelPressure, this, &SynthRenderer::channelPressure);
            QObject::connect(m_input, &MIDIInput::midiPitchBend, this, &SynthRenderer::pitchBend);
        }
        m_Stopped = false;
        m_isPlaying = false;
        if (m_files.length() > 0) {
            preparePlayback();
        }
        QIODevice *iodevice = m_audioOutput->start();
        qDebug() << "Audio Output started with buffer size =" << m_audioOutput->bufferSize() << "bytesfree= " << m_audioOutput->bytesFree();
        audioData.reserve(m_audioOutput->bufferSize());
        while (!stopped()) {
            EAS_RESULT eas_res;
            EAS_I32 numGen = 0;
            QCoreApplication::sendPostedEvents();
            if (m_isPlaying) {
                int t = getPlaybackLocation();
                emit playbackTime(t);
            }
            if (m_audioOutput->state() == QAudio::SuspendedState || 
                m_audioOutput->state() == QAudio::StoppedState) {
                qDebug() << Q_FUNC_INFO << "leaving";
                break;
            }
            if (m_easData != 0)
            {
                // synth audio rendering
                int maxlen = m_audioOutput->bufferSize();
                while(audioData.size() < maxlen) {
                    char data[m_bufferSize * sizeof (EAS_PCM) * m_channels];
                    EAS_PCM *buffer = (EAS_PCM *) data;
                    eas_res = EAS_Render(m_easData, buffer, m_bufferSize, &numGen);
                    if (eas_res != EAS_SUCCESS) {
                        qWarning() << Q_FUNC_INFO << "EAS_Render error:" << eas_res;
                        break;
                    } else {
                        int bytes = numGen * sizeof(EAS_PCM) * m_channels;
                        audioData.append(data, bytes);
                    }
                }
                // hand over to audiooutput, pushing the rendered buffer
                maxlen = qMin(maxlen, m_audioOutput->bytesFree());
                int written = iodevice->write(audioData, maxlen);
                if (written < 0 || m_audioOutput->error() != QAudio::NoError) {
                    qWarning() << Q_FUNC_INFO << "write audio error:" << m_audioOutput->error();
                    break;
                } else if (written > 0) {
                    audioData.remove(0, written);
                }
            }
            if (m_isPlaying && playbackCompleted()) {
                closePlayback();
                if (m_files.length() == 0) {
                    m_isPlaying = false;
                    emit playbackStopped();
                } else {
                    preparePlayback();
                }
            }
        }
        if (m_isPlaying) {
            closePlayback();
        }
        m_audioOutput->stop();
        qDebug() << "QAudioOutput stopped";
    } catch (...) {
        qWarning() << "Error! exception";
    }
    qDebug() << Q_FUNC_INFO << "ended";
    emit finished();
}

const QString SynthRenderer::midiDriver() const
{
    return m_midiDriver;
}

void SynthRenderer::setMidiDriver(const QString newMidiDriver)
{
    if (m_midiDriver != newMidiDriver) {
        m_midiDriver = newMidiDriver;
        if (m_input != nullptr) {
            m_input->close();
        }
        m_input = m_man.inputBackendByName(m_midiDriver);
    }
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
const QAudioDeviceInfo &SynthRenderer::audioDevice() const
{
    return m_audioDevice;
}

void SynthRenderer::setAudioDevice(const QAudioDeviceInfo &newAudioDevice)
{
    m_audioDevice = newAudioDevice;
}
#else
const QAudioDevice &SynthRenderer::audioDevice() const
{
    return m_audioDevice;
}

void SynthRenderer::setAudioDevice(const QAudioDevice &newAudioDevice)
{
    m_audioDevice = newAudioDevice;
}
#endif


QStringList SynthRenderer::availableAudioDevices() const
{
    QStringList result;
    foreach(const auto &device, m_availableDevices) {
    #if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        result << device.deviceName();
    #else
        result << device.description();
    #endif
    }
    return result;
}

QString SynthRenderer::audioDeviceName() const
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return m_audioDevice.deviceName();
#else
    return m_audioDevice.description();
#endif
}

void SynthRenderer::setAudioDeviceName(const QString newName)
{
    foreach(auto device, m_availableDevices) {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        if (device.deviceName() == newName) {
#else
        if (device.description() == newName) {
#endif
            m_audioDevice = device;
            break;
        }
    }
}

void SynthRenderer::noteOn(int chan, int note, int vel) 
{
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_NOTEON | chan;
    ev[1] = 0xff & note;
    ev[2] = 0xff & vel;
    //qDebug() << Q_FUNC_INFO << ev;
    writeMIDIData(ev);
}

void SynthRenderer::noteOff(int chan, int note, int vel) 
{
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_NOTEOFF | chan;
    ev[1] = 0xff & note;
    ev[2] = 0xff & vel;
    //qDebug() << Q_FUNC_INFO << ev;
    writeMIDIData(ev);
}

void SynthRenderer::keyPressure(const int chan, const int note, const int value) 
{
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_KEYPRESURE | chan;
    ev[1] = 0xff & note;
    ev[2] = 0xff & value;
    writeMIDIData(ev);
}

void SynthRenderer::controller(const int chan, const int control, const int value) 
{
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_CONTROLCHANGE | chan;
    ev[1] = 0xff & control;
    ev[2] = 0xff & value;
    writeMIDIData(ev);
}

void SynthRenderer::program(const int chan, const int program) 
{
    QByteArray ev(2, 0);
    ev[0] = MIDI_STATUS_PROGRAMCHANGE | chan;
    ev[1] = 0xff & program;
    writeMIDIData(ev);
}

void SynthRenderer::channelPressure(const int chan, const int value) 
{
    QByteArray ev(2, 0);
    ev[0] = MIDI_STATUS_CHANNELPRESSURE | chan;
    ev[1] = 0xff & value;
    writeMIDIData(ev);
}

void SynthRenderer::pitchBend(const int chan, const int v) 
{
    QByteArray ev(3, 0);
    int value = 8192 + v;
    ev[0] = MIDI_STATUS_PITCHBEND | chan;
    ev[1] = MIDI_LSB(value);
    ev[2] = MIDI_MSB(value);
    //qDebug() << Q_FUNC_INFO << chan << v << ev;;
    writeMIDIData(ev);
}

void
SynthRenderer::writeMIDIData(QByteArray &ev)
{
    EAS_RESULT eas_res = EAS_ERROR_ALREADY_STOPPED;
    EAS_I32 count = ev.size();
    EAS_U8 buffer[count];

    if (m_easData != 0 && m_streamHandle != 0 && !ev.isEmpty())
    {
        ::memcpy(buffer, ev.data(), ev.size());
        if (count > 0) {
            //qDebug() << Q_FUNC_INFO << QByteArray((char *)&buffer, count).toHex();
            eas_res = EAS_WriteMIDIStream(m_easData, m_streamHandle, buffer, count);
            if (eas_res != EAS_SUCCESS) {
                qWarning() << "EAS_WriteMIDIStream error: " << eas_res;
            }
        }
    }
}

void
SynthRenderer::initReverb(int reverb_type)
{
    EAS_RESULT eas_res;
    EAS_BOOL sw = EAS_TRUE;
    if ( reverb_type >= EAS_PARAM_REVERB_LARGE_HALL && reverb_type <= EAS_PARAM_REVERB_ROOM ) {
        sw = EAS_FALSE;
        eas_res = EAS_SetParameter(m_easData, EAS_MODULE_REVERB, EAS_PARAM_REVERB_PRESET, (EAS_I32) reverb_type);
        if (eas_res != EAS_SUCCESS) {
            qWarning() << "EAS_SetParameter error:" << eas_res;
        }
    }
    eas_res = EAS_SetParameter(m_easData, EAS_MODULE_REVERB, EAS_PARAM_REVERB_BYPASS, sw);
    if (eas_res != EAS_SUCCESS) {
        qWarning() << "EAS_SetParameter error: " << eas_res;
    }
}

void
SynthRenderer::initChorus(int chorus_type)
{
    EAS_RESULT eas_res;
    EAS_BOOL sw = EAS_TRUE;
    if (chorus_type >= EAS_PARAM_CHORUS_PRESET1 && chorus_type <= EAS_PARAM_CHORUS_PRESET4 ) {
        sw = EAS_FALSE;
        eas_res = EAS_SetParameter(m_easData, EAS_MODULE_CHORUS, EAS_PARAM_CHORUS_PRESET, (EAS_I32) chorus_type);
        if (eas_res != EAS_SUCCESS) {
            qWarning() << "EAS_SetParameter error:" << eas_res;
        }
    }
    eas_res = EAS_SetParameter(m_easData, EAS_MODULE_CHORUS, EAS_PARAM_CHORUS_BYPASS, sw);
    if (eas_res != EAS_SUCCESS) {
        qWarning() << "EAS_SetParameter error:" << eas_res;
    }
}

void
SynthRenderer::setReverbWet(int amount)
{
    EAS_RESULT eas_res = EAS_SetParameter(m_easData, EAS_MODULE_REVERB, EAS_PARAM_REVERB_WET, (EAS_I32) amount);
    if (eas_res != EAS_SUCCESS) {
        qWarning() << "EAS_SetParameter error:" << eas_res;
    }
}

void
SynthRenderer::setChorusLevel(int amount)
{
    EAS_RESULT eas_res = EAS_SetParameter(m_easData, EAS_MODULE_CHORUS, EAS_PARAM_CHORUS_LEVEL, (EAS_I32) amount);
    if (eas_res != EAS_SUCCESS) {
        qWarning() << "EAS_SetParameter error:" << eas_res;
    }
}

void
SynthRenderer::playFile(const QString fileName)
{
    qDebug() << Q_FUNC_INFO << fileName;
    m_files.append(fileName);
}

void
SynthRenderer::preparePlayback()
{
    EAS_HANDLE handle;
    EAS_RESULT result;
    EAS_I32 playTime;

    m_currentFile = new FileWrapper(m_files.first());
    m_files.removeFirst();

    /* call EAS library to open file */
    if ((result = EAS_OpenFile(m_easData, m_currentFile->getLocator(), &handle)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_OpenFile" << result;
        return;
    }

    /* prepare to play the file */
    if ((result = EAS_Prepare(m_easData, handle)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_Prepare" << result;
        return;
    }

    /* get play length */
    if ((result = EAS_ParseMetaData(m_easData, handle, &playTime)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_ParseMetaData. result=" << result;
        return;
    }
    else
    {
        qDebug() << "EAS_ParseMetaData. playTime=" << playTime;
    }

    qDebug() << Q_FUNC_INFO;
    m_fileHandle = handle;
    m_isPlaying = true;
}

bool
SynthRenderer::playbackCompleted()
{
    EAS_RESULT result;
    EAS_STATE state = EAS_STATE_EMPTY;
    if (m_fileHandle != 0 && (result = EAS_State(m_easData, m_fileHandle, &state)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_State:" << result;
    }
    //qDebug() << Q_FUNC_INFO << state;
    /* is playback complete */
    return ((state == EAS_STATE_STOPPED) || (state == EAS_STATE_ERROR) || (state == EAS_STATE_EMPTY));
}

void
SynthRenderer::closePlayback()
{
    qDebug() << Q_FUNC_INFO;
    EAS_RESULT result = EAS_SUCCESS;
    /* close the input file */
    if (m_fileHandle != 0 && (result = EAS_CloseFile(m_easData, m_fileHandle)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_CloseFile" << result;
    }
    m_fileHandle = 0;
    delete m_currentFile;
    m_currentFile = 0;
    m_isPlaying = false;
}

int
SynthRenderer::getPlaybackLocation()
{
    EAS_I32 playTime = 0;
    EAS_RESULT result = EAS_SUCCESS;
    /* get the current time */
    if ((result = EAS_GetLocation(m_easData, m_fileHandle, &playTime)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_GetLocation" << result;
    }
    //qDebug() << Q_FUNC_INFO << playTime;
    return playTime;
}

void
SynthRenderer::startPlayback(const QString fileName)
{
    if (!stopped())
    {
        playFile(fileName);
        preparePlayback();
    }
}

void
SynthRenderer::stopPlayback()
{
    if (!stopped()) {
        closePlayback();
    }
}
