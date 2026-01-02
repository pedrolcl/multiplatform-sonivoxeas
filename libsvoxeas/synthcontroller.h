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

#ifndef SYNTHCONTROLLER_H
#define SYNTHCONTROLLER_H

#include <QObject>
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QAudioOutput>
#else
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#endif

#include "mp_svoxeas_visibility.h"
#include "synthrenderer.h"

class MP_SVOXEAS_PUBLIC SynthController : public QObject
{
    Q_OBJECT
public:
    explicit SynthController(int bufTime, QObject *parent = 0);
    virtual ~SynthController();

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const QAudioDeviceInfo &audioDevice() const;
    void setAudioDevice(const QAudioDeviceInfo &newAudioDevice);
#else
    const QAudioDevice &audioDevice() const;
    void setAudioDevice(const QAudioDevice &newAudioDevice);
#endif
    QStringList availableAudioDevices();
    QString audioDeviceName() const;
    void setAudioDeviceName(const QString newName);
    void setBufferSize(int milliseconds);
    void setVolume(int volume);
    void restart();

    const QString midiDriver() const;
    void setMidiDriver(const QString newMidiDriver);
    QStringList connections() const;
    QString subscription() const;
    void subscribe(const QString &portName);

    void initReverb(int reverb_type);
    void initChorus(int chorus_type);
    void setReverbWet(int amount);
    void setChorusLevel(int amount);
    void initSoundfont(const QString soundfont);
    void playFile(const QString fileName);
    void startPlayback(const QString fileName);
    void stopPlayback();

public slots:
    void noteOn(int chan, int note, int vel);
    void noteOff(int chan, int note, int vel);
    void program(int chan, int pgm);
    void start();
    void stop();

signals:
    void finished();
    void underrunDetected();
    void stallDetected();
    void midiNoteOn(const int note, const int vel);
    void midiNoteOff(const int note, const int vel);
    void playbackStopped();
    void playbackTime(int time);
    void synthStarted();

private:
    void initAudio();
    void updateAudioDevices();
    void connectRendererSignals();

private:
    SynthRenderer *m_renderer{nullptr};
    QTimer m_stallDetector;
    int m_requestedBufferTime;
    bool m_running;
    QAudioFormat m_format;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QAudioOutput *m_audioOutput{nullptr};
    QMap<QString,QAudioDeviceInfo> m_availableDevices;
    QAudioDeviceInfo m_audioDevice;
#else
    QAudioSink *m_audioOutput{nullptr};
    QMap<QString,QAudioDevice> m_availableDevices;
    QAudioDevice m_audioDevice;
    QMediaDevices *m_devices;
#endif
    QString m_midiDriver;
    QString m_portName;
};

#endif // SYNTHCONTROLLER_H
