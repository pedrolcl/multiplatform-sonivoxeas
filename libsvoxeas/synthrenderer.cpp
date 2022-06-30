/*
    Sonivox EAS Synthesizer for Qt applications
    Copyright (C) 2016-2022, Pedro Lopez-Cabanillas <plcl@users.sf.net>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
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

using namespace drumstick::ALSA;

SynthRenderer::SynthRenderer(int bufTime, QObject *parent) : QObject(parent),
    m_Stopped(true),
    m_isPlaying(false),
    m_requestedBufferTime(bufTime)
{
    initALSA();
    initEAS();
    initAudioDevices();
}

void
SynthRenderer::initALSA()
{
    const QString errorstr = "Fatal error from the ALSA sequencer. "
        "This usually happens when the kernel doesn't have ALSA support, "
        "or the device node (/dev/snd/seq) doesn't exists, "
        "or the kernel module (snd_seq) is not loaded. "
        "Please check your ALSA/MIDI configuration.";
    try {
        m_Client = new MidiClient(this);
        m_Client->open();
        m_Client->setClientName("Sonivox EAS");
        connect( m_Client, &MidiClient::eventReceived,
                 this, &SynthRenderer::sequencerEvent);
        m_Port = new MidiPort(this);
        m_Port->attach( m_Client );
        m_Port->setPortName("Synthesizer input");
        m_Port->setCapability( SND_SEQ_PORT_CAP_WRITE |
                               SND_SEQ_PORT_CAP_SUBS_WRITE );
        m_Port->setPortType( SND_SEQ_PORT_TYPE_APPLICATION |
                             SND_SEQ_PORT_TYPE_MIDI_GENERIC );
        connect( m_Port, &MidiPort::subscribed,
                 this, &SynthRenderer::subscription);
        m_Port->subscribeFromAnnounce();
        m_codec = new MidiCodec(256);
        m_codec->enableRunningStatus(false);
    } catch (const SequencerError& ex) {
        qCritical() << errorstr + "Returned error was:" + ex.qstrError();
    } catch (...) {
        qCritical() << errorstr;
    }
    qDebug() << Q_FUNC_INFO;
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
    format.setSample(m_sample_size);
    format.setCodec("audio/pcm");
    format.setSampleFormat(QAudioFormat::SignedInt);
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
        qDebug() << "QAudioOutput state changed:" << state;
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
    m_Port->detach();
    delete m_Port;
    m_Client->close();
    delete m_Client;
    delete m_codec;

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

void
SynthRenderer::subscription(MidiPort*, Subscription* subs)
{
    qDebug() << "Subscription made from "
             << subs->getSender()->client << ":"
             << subs->getSender()->port;
}

void
SynthRenderer::subscribe(const QString& portName)
{
    try {
        qDebug() << "Trying to subscribe " << portName.toLocal8Bit().data();
        m_Port->subscribeFrom(portName);
    } catch (const SequencerError& err) {
        qWarning() << "SequencerError exception. Error code: " << err.code()
                   << " (" << err.qstrError() << ")";
        qWarning() << "Location: " << err.location();
        throw err;
    }
}

void
SynthRenderer::run()
{
    QByteArray audioData;
    initAudio();
    qDebug() << Q_FUNC_INFO << "started";
    try {
        m_Client->setRealTimeInput(false);
        m_Client->startSequencerInput();
        m_Stopped = false;
        m_isPlaying = false;
        if (m_files.length() > 0) {
            preparePlayback();
        }
        QIODevice *iodevice = m_audioOutput->start();
        qDebug() << "QAudioOutput started with buffer size =" << m_audioOutput->bufferSize();
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
                while(audioData.size() < m_audioOutput->bufferSize()) {
                    char data[m_bufferSize * sizeof (EAS_PCM) * m_channels];
                    EAS_PCM *buffer = (EAS_PCM *) data;
                    eas_res = EAS_Render(m_easData, buffer, m_bufferSize, &numGen);
                    if (eas_res != EAS_SUCCESS) {
                        qWarning() << "EAS_Render error:" << eas_res;
                        break;
                    } else {
                        int bytes = numGen * sizeof(EAS_PCM) * m_channels;
                        audioData.append(data, bytes);
                    }
                }
                // hand over to audiooutput, pushing the rendered buffer
                int written = iodevice->write(audioData);
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
        m_Client->stopSequencerInput();
        m_audioOutput->stop();
        qDebug() << "QAudioOutput stopped";
    } catch (const SequencerError& err) {
        qWarning() << "SequencerError exception. Error code: " << err.code()
                   << " (" << err.qstrError() << ")";
        qWarning() << "Location: " << err.location();
    }
    qDebug() << Q_FUNC_INFO << "ended";
    emit finished();
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

void
SynthRenderer::sequencerEvent(SequencerEvent *ev)
{
    switch (ev->getSequencerType()) {
    case SND_SEQ_EVENT_CHANPRESS:
    case SND_SEQ_EVENT_NOTEOFF:
    case SND_SEQ_EVENT_NOTEON:
    case SND_SEQ_EVENT_CONTROLLER:
    case SND_SEQ_EVENT_KEYPRESS:
    case SND_SEQ_EVENT_PGMCHANGE:
    case SND_SEQ_EVENT_PITCHBEND:
        writeMIDIData(ev);
        break;
    }
    delete ev;
}

void
SynthRenderer::writeMIDIData(SequencerEvent *ev)
{
    EAS_RESULT eas_res = EAS_ERROR_ALREADY_STOPPED;
    EAS_I32 count;
    EAS_U8 buffer[256];

    if (m_easData != 0 && m_streamHandle != 0)
    {
        count = m_codec->decode((unsigned char *)&buffer, sizeof(buffer), ev->getHandle());
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
