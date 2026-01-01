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

#include <QObject>
#include <QString>
#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>

#include <eas_reverb.h>
#include <eas_chorus.h>

#include "programsettings.h"
#include "synthrenderer.h"
#include "filewrapper.h"

using namespace drumstick::rt;

SynthRenderer::SynthRenderer(QObject *parent):
    QIODevice(parent),
    m_isPlaying(false),
    m_input(nullptr),
    m_currentFile(nullptr),
    m_lastBufferSize(0),
    m_soundfont("")
{
    //qDebug() << Q_FUNC_INFO;
    initMIDI();
    initEAS();
}

void
SynthRenderer::initMIDI()
{
    auto defaultsMap = QVariantMap{{BackendManager::QSTR_DRUMSTICKRT_PUBLICNAMEIN,
                                    QStringLiteral("MIDI IN")},
                                   {BackendManager::QSTR_DRUMSTICKRT_PUBLICNAMEOUT,
                                    QStringLiteral("MIDI OUT")}};
    m_man.refresh(defaultsMap);
    auto inputs = m_man.availableInputs();
    //qDebug() << Q_FUNC_INFO << inputs;
    //qDebug() << Q_FUNC_INFO << ProgramSettings::DEFAULT_MIDI_DRIVER;
    if (m_midiDriver.isEmpty()) {
        setMidiDriver(ProgramSettings::DEFAULT_MIDI_DRIVER);
    }
    if (m_input == nullptr) {
        qWarning() << Q_FUNC_INFO << "Input Backend is Missing. You may need to set the DRUMSTICKRT environment variable";    
    }
}

void
SynthRenderer::initEAS()
{
    //qDebug() << Q_FUNC_INFO;
    /* SONiVOX EAS initialization */
    EAS_RESULT eas_res;
    EAS_DATA_HANDLE dataHandle;
    EAS_HANDLE handle;

    const S_EAS_LIB_CONFIG *easConfig = EAS_Config();
    if (easConfig == 0) {
        qCritical() << Q_FUNC_INFO << "EAS_Config returned null";
        return;
    }

    eas_res = EAS_Init(&dataHandle);
    if (eas_res != EAS_SUCCESS) {
      qCritical() << Q_FUNC_INFO << "EAS_Init error: " << eas_res;
      return;
    }

    if (!m_soundfont.isEmpty()) {
        FileWrapper Soundfont(m_soundfont);
        if (Soundfont.ok()) {
            eas_res = EAS_LoadDLSCollection(dataHandle, nullptr, Soundfont.getLocator());
            if (eas_res != EAS_SUCCESS) {
                qWarning() << QString("EAS_LoadDLSCollection(%1) error: %2")
                                  .arg(m_soundfont)
                                  .arg(eas_res);
            }
        } else {
            qWarning() << "Failed to open" << m_soundfont;
        }
    }

    eas_res = EAS_OpenMIDIStream(dataHandle, &handle, NULL);
    if (eas_res != EAS_SUCCESS) {
      qCritical() << Q_FUNC_INFO << "EAS_OpenMIDIStream error: " << eas_res;
      EAS_Shutdown(dataHandle);
      return;
    }

    m_easData = dataHandle;
    m_streamHandle = handle;
    assert(m_streamHandle != 0);
    m_sampleRate = easConfig->sampleRate;
    m_renderFrames = easConfig->mixBufferSize;
    m_channels = easConfig->numChannels;
    m_sample_size = CHAR_BIT * sizeof (EAS_PCM);
    //qDebug() << Q_FUNC_INFO << "EAS renderFrames=" << m_renderFrames << " sampleRate=" << m_sampleRate << " channels=" << m_channels;

    //QAudioFormat initialization;
    m_format.setSampleRate(m_sampleRate);
    m_format.setChannelCount(m_channels);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    m_format.setSampleSize(m_sample_size);
    m_format.setCodec("audio/pcm");
    m_format.setSampleType(QAudioFormat::SignedInt);
#else
    m_format.setSampleFormat(QAudioFormat::Int16);
#endif
}

void SynthRenderer::uninitEAS()
{
    EAS_RESULT eas_res;
    if (m_easData != 0 && m_streamHandle != 0) {
      eas_res = EAS_CloseMIDIStream(m_easData, m_streamHandle);
      if (eas_res != EAS_SUCCESS) {
          qWarning() << Q_FUNC_INFO << "EAS_CloseMIDIStream error: " << eas_res;
      }
      eas_res = EAS_Shutdown(m_easData);
      if (eas_res != EAS_SUCCESS) {
          qWarning() << Q_FUNC_INFO << "EAS_Shutdown error: " << eas_res;
      }
    }
}

SynthRenderer::~SynthRenderer()
{
    if (m_input != nullptr) {
        m_input->disconnect();
        m_input->close();
    }
    uninitEAS();
    //qDebug() << Q_FUNC_INFO;
}

qint64 SynthRenderer::readData(char *data, qint64 maxlen)
{
    EAS_RESULT eas_res;
    EAS_I32 numGen = 0;
    const qint64 bufferSamples = m_renderFrames * m_channels;
    const qint64 bufferBytes = bufferSamples * sizeof(EAS_PCM);
    // qDebug() << Q_FUNC_INFO << "starting with maxlen:" << maxlen << bufferBytes;

    if (m_isPlaying) {
        int t = getPlaybackLocation();
        emit playbackTime(t);
    }

    while (m_audioBuffer.size() < maxlen) {
        QByteArray buf{bufferBytes, '\0'};
        eas_res = EAS_Render(m_easData,
                             reinterpret_cast<EAS_PCM *>(buf.data()),
                             m_renderFrames,
                             &numGen);
        if (eas_res == EAS_SUCCESS) {
            m_audioBuffer.append(buf);
        } else {
            qWarning() << Q_FUNC_INFO << "EAS_Render() error:" << eas_res;
        }
    }

    if (maxlen > 0) {
        memcpy(data, m_audioBuffer.constData(), maxlen);
        m_audioBuffer.remove(0, maxlen);
    }

    if (m_isPlaying && isPlaybackCompleted()) {
        closePlayback();
        if (m_files.isEmpty()) {
            m_isPlaying = false;
            emit playbackStopped();
        } else {
            preparePlayback();
        }
    }

    m_lastBufferSize = maxlen;
    //qDebug() << Q_FUNC_INFO << "before returning" << buflen;
    return maxlen;
}

qint64 SynthRenderer::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    //qDebug() << Q_FUNC_INFO;
	return 0;
}

qint64 SynthRenderer::size() const
{
    //qDebug() << Q_FUNC_INFO;
    return std::numeric_limits<qint64>::max();
}

qint64 SynthRenderer::bytesAvailable() const
{
    //qDebug() << Q_FUNC_INFO;
    return std::numeric_limits<qint64>::max();
}

bool
SynthRenderer::stopped()
{
    //qDebug() << Q_FUNC_INFO;
    return !isOpen();
}

void
SynthRenderer::start()
{
    Q_ASSERT_X(!isOpen(), Q_FUNC_INFO, "renderer already open");
    m_isPlaying = false;
    /*bool ok =*/ open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    // qDebug() << Q_FUNC_INFO << "opened:" << ok;
    if (m_files.length() > 0) {
        preparePlayback();
    }
}

void
SynthRenderer::stop()
{
    Q_ASSERT_X(isOpen(), Q_FUNC_INFO, "renderer not open");
    if (isOpen()) {
        close();
    }
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
    // qDebug() << Q_FUNC_INFO << result;
    return result;
}

QString 
SynthRenderer::subscription() const
{
    //qDebug() << Q_FUNC_INFO << m_portName;
    return m_portName;
}

void
SynthRenderer::subscribe(const QString& portName)
{
    //qDebug() << Q_FUNC_INFO << newPort;
    Q_ASSERT(m_input != nullptr);
    if (m_portName != portName || portName.isEmpty()) {
        auto avail = m_input->connections(true);
        auto it = std::find_if(avail.constBegin(),
                               avail.constEnd(),
                               [portName](const MIDIConnection &c) { return c.first == portName; });
        m_input->close();
        if (it == avail.constEnd()) {
            MIDIConnection conn = avail.isEmpty() ? MIDIConnection() : avail.first();
            m_input->open(conn);
            m_portName = conn.first;
        } else {
            m_input->open(*it);
            m_portName = (*it).first;
        }
    }
}

const QString 
SynthRenderer::midiDriver() const
{
    //qDebug() << Q_FUNC_INFO << m_midiDriver;
    return m_midiDriver;
}

void 
SynthRenderer::setMidiDriver(const QString newMidiDriver)
{
    if (m_midiDriver != newMidiDriver) {
        //qDebug() << Q_FUNC_INFO << newMidiDriver;
        m_midiDriver = newMidiDriver;
        if (m_input != nullptr) {
            m_input->disconnect();
            m_input->close();
        }
        m_input = m_man.inputBackendByName(m_midiDriver);
        if (m_input != nullptr) {
            QObject::connect(m_input, &MIDIInput::midiNoteOn, this, &SynthRenderer::noteOn);
            QObject::connect(m_input, &MIDIInput::midiNoteOff, this, &SynthRenderer::noteOff);
            QObject::connect(m_input, &MIDIInput::midiKeyPressure, this, &SynthRenderer::keyPressure);
            QObject::connect(m_input, &MIDIInput::midiController, this, &SynthRenderer::controller);
            QObject::connect(m_input, &MIDIInput::midiProgram, this, &SynthRenderer::program);
            QObject::connect(m_input, &MIDIInput::midiChannelPressure, this, &SynthRenderer::channelPressure);
            QObject::connect(m_input, &MIDIInput::midiPitchBend, this, &SynthRenderer::pitchBend);
        }
    }
}

void 
SynthRenderer::noteOn(int chan, int note, int vel) 
{
    //qDebug() << Q_FUNC_INFO << chan << note << vel;
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_NOTEON | chan;
    ev[1] = 0xff & note;
    ev[2] = 0xff & vel;
    writeMIDIData(ev);
    emit midiNoteOn(note,vel);
}

void 
SynthRenderer::noteOff(int chan, int note, int vel) 
{
    //qDebug() << Q_FUNC_INFO << chan << note << vel;
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_NOTEOFF | chan;
    ev[1] = 0xff & note;
    ev[2] = 0xff & vel;
    writeMIDIData(ev);
    emit midiNoteOff(note,vel);
}

void 
SynthRenderer::keyPressure(const int chan, const int note, const int value) 
{
    //qDebug() << Q_FUNC_INFO << chan << note << value;
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_KEYPRESURE | chan;
    ev[1] = 0xff & note;
    ev[2] = 0xff & value;
    writeMIDIData(ev);
}

void 
SynthRenderer::controller(const int chan, const int control, const int value) 
{
    //qDebug() << Q_FUNC_INFO << chan << control << value;
    QByteArray ev(3, 0);
    ev[0] = MIDI_STATUS_CONTROLCHANGE | chan;
    ev[1] = 0xff & control;
    ev[2] = 0xff & value;
    writeMIDIData(ev);
}

void SynthRenderer::program(const int chan, const int program) 
{
    //qDebug() << Q_FUNC_INFO << chan << program;
    QByteArray ev(2, 0);
    ev[0] = MIDI_STATUS_PROGRAMCHANGE | chan;
    ev[1] = 0xff & program;
    writeMIDIData(ev);
}

void SynthRenderer::channelPressure(const int chan, const int value) 
{
    //qDebug() << Q_FUNC_INFO << chan << value;
    QByteArray ev(2, 0);
    ev[0] = MIDI_STATUS_CHANNELPRESSURE | chan;
    ev[1] = 0xff & value;
    writeMIDIData(ev);
}

void SynthRenderer::pitchBend(const int chan, const int v) 
{
    //qDebug() << Q_FUNC_INFO << chan << v;;
    QByteArray ev(3, 0);
    int value = 8192 + v;
    ev[0] = MIDI_STATUS_PITCHBEND | chan;
    ev[1] = MIDI_LSB(value);
    ev[2] = MIDI_MSB(value);
    writeMIDIData(ev);
}

qint64 SynthRenderer::lastBufferSize() const
{
    return m_lastBufferSize;
}

void SynthRenderer::resetLastBufferSize()
{
    m_lastBufferSize = 0;
}

void SynthRenderer::reserveBuffer(qsizetype size)
{
    //qDebug() << Q_FUNC_INFO << size;
    m_audioBuffer.reserve(size);
}

const QAudioFormat&
SynthRenderer::format() const
{
    return m_format;
}

void
SynthRenderer::writeMIDIData(QByteArray &ev)
{
    EAS_RESULT eas_res = EAS_ERROR_ALREADY_STOPPED;
    const EAS_I32 count = ev.size();

    if (m_easData != 0 && m_streamHandle != 0 && !ev.isEmpty())
    {
        if (count > 0) {
            //qDebug() << Q_FUNC_INFO << QByteArray((char *)&buffer, count).toHex();
            eas_res = EAS_WriteMIDIStream(m_easData, m_streamHandle, reinterpret_cast<EAS_U8*>(ev.data()), count);
            if (eas_res != EAS_SUCCESS) {
                qWarning() << "EAS_WriteMIDIStream error: " << eas_res;
            }
        }
    }
}

void
SynthRenderer::initReverb(int reverb_type)
{
    //qDebug() << Q_FUNC_INFO;
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
    //qDebug() << Q_FUNC_INFO;
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
    //qDebug() << Q_FUNC_INFO;
    EAS_RESULT eas_res = EAS_SetParameter(m_easData, EAS_MODULE_REVERB, EAS_PARAM_REVERB_WET, (EAS_I32) amount);
    if (eas_res != EAS_SUCCESS) {
        qWarning() << "EAS_SetParameter error:" << eas_res;
    }
}

void
SynthRenderer::setChorusLevel(int amount)
{
    //qDebug() << Q_FUNC_INFO;
    EAS_RESULT eas_res = EAS_SetParameter(m_easData, EAS_MODULE_CHORUS, EAS_PARAM_CHORUS_LEVEL, (EAS_I32) amount);
    if (eas_res != EAS_SUCCESS) {
        qWarning() << "EAS_SetParameter error:" << eas_res;
    }
}

void SynthRenderer::initSoundfont(const QString soundfont)
{
    if (m_soundfont != soundfont) {
        m_soundfont = soundfont;
        uninitEAS();
        initEAS();
    }
}

void
SynthRenderer::playFile(const QString fileName)
{
    //qDebug() << Q_FUNC_INFO << fileName;
    m_files.append(fileName);
}

void
SynthRenderer::preparePlayback()
{
    EAS_HANDLE handle;
    EAS_RESULT result;
    EAS_I32 playTime;
    if (!m_files.isEmpty()) 
    {
        m_currentFile = new FileWrapper(m_files.first());
        m_files.removeFirst();
        /* call EAS library to open file */
        if ((result = EAS_OpenFile(m_easData, m_currentFile->getLocator(), &handle)) != EAS_SUCCESS)
        {
            qWarning() << Q_FUNC_INFO << "EAS_OpenFile" << result;
            return;
        }
        /* prepare to play the file */
        if ((result = EAS_Prepare(m_easData, handle)) != EAS_SUCCESS)
        {
            qWarning() << Q_FUNC_INFO << "EAS_Prepare" << result;
            return;
        }
        /* get play length */
        if ((result = EAS_ParseMetaData(m_easData, handle, &playTime)) != EAS_SUCCESS)
        {
            qWarning() << Q_FUNC_INFO << "EAS_ParseMetaData. result=" << result;
            return;
        }
        else
        {
            qDebug() << Q_FUNC_INFO << "EAS_ParseMetaData. playTime=" << playTime;
        }
        m_fileHandle = handle;
        m_isPlaying = true;
    }
}

bool
SynthRenderer::isPlaybackCompleted()
{
    EAS_RESULT result;
    EAS_STATE state = EAS_STATE_EMPTY;
    if (m_fileHandle != 0 && (result = EAS_State(m_easData, m_fileHandle, &state)) != EAS_SUCCESS)
    {
        qWarning() << "EAS_State:" << result;
    }
    /* is playback complete */
    bool b = ((state == EAS_STATE_STOPPED) || (state == EAS_STATE_ERROR) || (state == EAS_STATE_EMPTY));
    //qDebug() << Q_FUNC_INFO << b << "state:" << state;
    return b;
}

void
SynthRenderer::closePlayback()
{
    //qDebug() << Q_FUNC_INFO;
    EAS_RESULT result = EAS_SUCCESS;
    /* close the input file */
    if (m_fileHandle != 0 && (result = EAS_CloseFile(m_easData, m_fileHandle)) != EAS_SUCCESS)
    {
        qWarning() << Q_FUNC_INFO << "EAS_CloseFile" << result;
    }
    m_fileHandle = nullptr;
    delete m_currentFile;
    m_currentFile = nullptr;
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
        qWarning() << Q_FUNC_INFO << "EAS_GetLocation" << result;
    }
    //qDebug() << Q_FUNC_INFO << playTime;
    return playTime;
}

void
SynthRenderer::startPlayback(const QString fileName)
{
    //qDebug() << Q_FUNC_INFO;
    if (!stopped())
    {
        playFile(fileName);
        preparePlayback();
    }
}

void
SynthRenderer::stopPlayback()
{
    //qDebug() << Q_FUNC_INFO;
    if (!stopped()) {
        closePlayback();
    }
}
