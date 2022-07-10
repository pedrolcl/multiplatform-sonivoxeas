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

#ifndef PROGRAMSETTINGS_H
#define PROGRAMSETTINGS_H

#include <QObject>
#include <QString>
#include <QSettings>

class ProgramSettings : public QObject
{
    Q_OBJECT

public:
    static ProgramSettings* instance();

    int bufferTime() const;
    void setBufferTime(int bufferTime);

    int reverbType() const;
    void setReverbType(int reverbType);

    int reverbWet() const;
    void setReverbWet(int reverbWet);

    int chorusType() const;
    void setChorusType(int chorusType);

    int chorusLevel() const;
    void setChorusLevel(int chorusLevel);

    int volumeLevel() const;
    void setVolumeLevel(int newVolumeLevel);

    const QString &audioDeviceName() const;
    void setAudioDeviceName(const QString &newAudioDeviceName);
    
    const QString &midiDriver() const;
    void setMidiDriver(const QString &newMidiDriver);
    
    const QString &portName() const;
    void setPortName(const QString &newPortName);

    static const QString DEFAULT_MIDI_DRIVER;
    static const QString DEFAULT_AUDIO_DEVICE;
    static const int DEFAULT_BUFFER_TIME;
    static const int DEFAULT_REVERB_TYPE;
    static const int DEFAULT_REVERB_WET;
    static const int DEFAULT_CHORUS_TYPE;
    static const int DEFAULT_CHORUS_LEVEL;
    static const int DEFAULT_VOLUME_LEVEL;

signals:
    void ValuesChanged();

public slots:
    void ResetDefaults();
    void ReadFromNativeStorage();
    void ReadFromFile(const QString &filepath);
    void SaveToNativeStorage();
    void SaveToFile(const QString &filepath);

private:
    explicit ProgramSettings(QObject *parent = nullptr);
    void internalRead(QSettings& settings);
    void internalSave(QSettings& settings);

    QString m_midiDriver;
    QString m_portName;
    int m_bufferTime;
    int m_reverbType;
    int m_reverbWet;
    int m_chorusType;
    int m_chorusLevel;
    int m_volumeLevel;
    QString m_audioDeviceName;
};

#endif // PROGRAMSETTINGS_H
