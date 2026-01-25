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

#ifndef SYNTHRENDERER_H_
#define SYNTHRENDERER_H_

#include <QObject>
#include <QIODevice>
#include <QAudioFormat>

#include <drumstick/backendmanager.h>
#include <drumstick/rtmidiinput.h>

#include "mp_svoxeas_visibility.h"
#include "eas.h"
#include "filewrapper.h"

class MP_SVOXEAS_PUBLIC SynthRenderer : public QIODevice
{
    Q_OBJECT

public:
    explicit SynthRenderer(QObject *parent = 0);
    virtual ~SynthRenderer();

    /* QIODevice */
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
	qint64 size() const override;
	qint64 bytesAvailable() const override;

    /* Drumstick::RT */
    const QString midiDriver() const;
    void setMidiDriver(const QString newMidiDriver);
    QStringList connections() const;
    QString subscription() const;
    void subscribe(const QString& portName);
    void start();
    void stop();
    bool stopped();

    /* Sonivox EAS*/
    void initReverb(int reverb_type);
    void initChorus(int chorus_type);
    void initSoundLib(int sound_lib);
    void setReverbWet(int amount);
    void setChorusLevel(int amount);
    void initSoundfont(const QString soundfont);
    void playFile(const QString fileName);
    void startPlayback(const QString fileName);
    void stopPlayback();

    /* Qt Multimedia */
    const QAudioFormat &format() const;
    qint64 lastBufferSize() const;
    void resetLastBufferSize();
    void reserveBuffer(qsizetype size);

    void uninitEAS();

public slots:
    void noteOn(int chan, int note, int vel);
    void noteOff(int chan, int note, int vel);
    void keyPressure(const int chan, const int note, const int value);
    void controller(const int chan, const int control, const int value);
    void program(const int chan, const int program);
    void channelPressure(const int chan, const int value);
    void pitchBend(const int chan, const int value);

private:
    void initMIDI();
    void initEAS();
    void writeMIDIData(QByteArray &ev);

    void preparePlayback();
    bool isPlaybackCompleted();
    void closePlayback();
    int getPlaybackLocation();

signals:
    void midiNoteOn(const int note, const int vel);
    void midiNoteOff(const int note, const int vel);
    void playbackStopped();
    void playbackTime(int time);

private:
    bool m_isPlaying;

    /* Drumstick RT*/
    QString m_midiDriver;
    QString m_portName;
    drumstick::rt::BackendManager m_man;
    drumstick::rt::MIDIInput *m_input;

    /* SONiVOX EAS */
    int m_sampleRate, m_renderFrames, m_channels, m_sample_size;
    EAS_DATA_HANDLE m_easData;
    EAS_HANDLE m_streamHandle;
    EAS_HANDLE m_fileHandle;
    FileWrapper *m_currentFile;
    QStringList m_files;
    QString m_soundfont;
    E_EAS_SNDLIB_TYPE m_soundLib;

    // Qt Multimedia
    QAudioFormat m_format;
    qint64 m_lastBufferSize;
    QByteArray m_audioBuffer;
};

#endif /*SYNTHRENDERER_H_*/
