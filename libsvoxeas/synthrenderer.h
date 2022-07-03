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

#ifndef SYNTHRENDERER_H_
#define SYNTHRENDERER_H_

#include <QObject>
#include <QReadWriteLock>
#include <QScopedPointer>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QAudioOutput>
#else
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#endif
#include <drumstick/backendmanager.h>
#include <drumstick/rtmidiinput.h>
#include "eas.h"
#include "filewrapper.h"

class SynthRenderer : public QObject
{
    Q_OBJECT

public:
    explicit SynthRenderer(int bufTime, QObject *parent = 0);
    virtual ~SynthRenderer();

    QStringList connections() const;
    QString subscription() const;
    void subscribe(const QString& portName);
    void stop();
    bool stopped();

    void initReverb(int reverb_type);
    void initChorus(int chorus_type);
    void setReverbWet(int amount);
    void setChorusLevel(int amount);

    void playFile(const QString fileName);
    void startPlayback(const QString fileName);
    void stopPlayback();

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const QAudioDeviceInfo &audioDevice() const;
    void setAudioDevice(const QAudioDeviceInfo &newAudioDevice);
#else
    const QAudioDevice &audioDevice() const;
    void setAudioDevice(const QAudioDevice &newAudioDevice);
#endif
    QStringList availableAudioDevices() const;
    
    QString audioDeviceName() const;
    void setAudioDeviceName(const QString newName);

    const QString midiDriver() const;
    void setMidiDriver(const QString newMidiDriver);

private:
    void initMIDI();
    void initEAS();
    void initAudio();
    void initAudioDevices();
    void writeMIDIData(QByteArray &ev);

    void preparePlayback();
    bool playbackCompleted();
    void closePlayback();
    int getPlaybackLocation();

public slots:
    void noteOn(int chan, int note, int vel);
    void noteOff(int chan, int note, int vel);
    void keyPressure(const int chan, const int note, const int value);
    void controller(const int chan, const int control, const int value);
    void program(const int chan, const int program);
    void channelPressure(const int chan, const int value);
    void pitchBend(const int chan, const int value);
    void run();

signals:
    void finished();
    void playbackStopped();
    void playbackTime(int time);

private:
    bool m_Stopped;
    bool m_isPlaying;

    QReadWriteLock m_mutex;
    QStringList m_files;

    /* Drumstick RT*/
    QString m_midiDriver;
    QString m_portName;
    drumstick::rt::BackendManager m_man;
    drumstick::rt::MIDIInput *m_input;

    /* SONiVOX EAS */
    int m_sampleRate, m_bufferSize, m_channels, m_sample_size;
    EAS_DATA_HANDLE m_easData;
    EAS_HANDLE m_streamHandle;
    EAS_HANDLE m_fileHandle;
    FileWrapper *m_currentFile;

    /* audio */
    int m_requestedBufferTime;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QScopedPointer<QAudioOutput> m_audioOutput;
    QList<QAudioDeviceInfo> m_availableDevices;
    QAudioDeviceInfo m_audioDevice;
#else
    QScopedPointer<QAudioSink> m_audioOutput;
    QList<QAudioDevice> m_availableDevices;
    QAudioDevice m_audioDevice;
#endif
};

#endif /*SYNTHRENDERER_H_*/
