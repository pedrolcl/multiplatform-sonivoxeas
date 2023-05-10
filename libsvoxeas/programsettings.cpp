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
#include "programsettings.h"

const QString ProgramSettings::DEFAULT_MIDI_DRIVER =
#if defined(Q_OS_LINUX)
    QLatin1String("ALSA");
#elif defined(Q_OS_WINDOWS)
    QLatin1String("Windows MM");
#elif defined(Q_OS_MACOS)
    QLatin1String("CoreMIDI");
#elif defined(Q_OS_UNIX)
    QLatin1String("OSS");
#else
    QLatin1String("Network");
#endif
const QString ProgramSettings::DEFAULT_AUDIO_DEVICE = QLatin1String("default");
const int ProgramSettings::DEFAULT_BUFFER_TIME = 100;
const int ProgramSettings::DEFAULT_REVERB_TYPE = 1;
const int ProgramSettings::DEFAULT_REVERB_WET = 25800;
const int ProgramSettings::DEFAULT_CHORUS_TYPE = -1;
const int ProgramSettings::DEFAULT_CHORUS_LEVEL = 0;
const int ProgramSettings::DEFAULT_VOLUME_LEVEL = 90;

ProgramSettings::ProgramSettings(QObject *parent) : QObject(parent)
{
    ResetDefaults();
}

ProgramSettings* ProgramSettings::instance()
{
    static ProgramSettings inst;
    return &inst;
}

void ProgramSettings::ResetDefaults()
{
    qDebug() << Q_FUNC_INFO;
    m_bufferTime = DEFAULT_BUFFER_TIME;
    m_reverbType = DEFAULT_REVERB_TYPE;
    m_reverbWet = DEFAULT_REVERB_WET;
    m_chorusType = DEFAULT_CHORUS_TYPE;
    m_chorusLevel = DEFAULT_CHORUS_LEVEL;
    m_volumeLevel = DEFAULT_VOLUME_LEVEL;
    m_dlsFile.clear();
    emit ValuesChanged();
}

void ProgramSettings::ReadFromNativeStorage()
{
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    QSettings settings;
    internalRead(settings);
}

void ProgramSettings::SaveToNativeStorage()
{
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    QSettings settings;
    internalSave(settings);
}

void ProgramSettings::ReadFromFile(const QString& filepath)
{
    QSettings settings(filepath, QSettings::IniFormat);
    internalRead(settings);
}

void ProgramSettings::SaveToFile(const QString& filepath)
{
    QSettings settings(filepath, QSettings::IniFormat);
    internalSave(settings);
}

void ProgramSettings::internalRead(QSettings &settings)
{
    qDebug() << Q_FUNC_INFO;
    m_midiDriver = settings.value("MIDIDriver", DEFAULT_MIDI_DRIVER).toString();
    m_portName = settings.value("PortName", QString()).toString();
    m_bufferTime = settings.value("BufferTime", DEFAULT_BUFFER_TIME).toInt();
    m_reverbType = settings.value("ReverbType", DEFAULT_REVERB_TYPE).toInt();
    m_reverbWet = settings.value("ReverbWet", DEFAULT_REVERB_WET).toInt();
    m_chorusType = settings.value("ChorusType", DEFAULT_CHORUS_TYPE).toInt();
    m_chorusLevel = settings.value("ChorusLevel", DEFAULT_CHORUS_LEVEL).toInt();
    m_audioDeviceName = settings.value("AudioDevice", DEFAULT_AUDIO_DEVICE).toString();
    m_volumeLevel = settings.value("VolumeLevel", DEFAULT_VOLUME_LEVEL).toInt();
    m_dlsFile = settings.value("DLSfile", QString()).toString();
    emit ValuesChanged();
}

void ProgramSettings::internalSave(QSettings &settings)
{
    qDebug() << Q_FUNC_INFO;
    settings.setValue("MIDIDriver", m_midiDriver);
    settings.setValue("PortName", m_portName);
    settings.setValue("BufferTime", m_bufferTime);
    settings.setValue("ReverbType", m_reverbType);
    settings.setValue("ReverbWet", m_reverbWet);
    settings.setValue("ChorusType", m_chorusType);
    settings.setValue("ChorusLevel", m_chorusLevel);
    settings.setValue("AudioDevice", m_audioDeviceName);
    settings.setValue("VolumeLevel", m_volumeLevel);
    settings.setValue("DLSfile", m_dlsFile);
    settings.sync();
}

QString ProgramSettings::dlsFile() const
{
    return m_dlsFile;
}

void ProgramSettings::setDlsFile(const QString &newDlsFile)
{
    m_dlsFile = newDlsFile;
}

int ProgramSettings::volumeLevel() const
{
    return m_volumeLevel;
}

void ProgramSettings::setVolumeLevel(int newVolumeLevel)
{
    m_volumeLevel = newVolumeLevel;
}

const QString &ProgramSettings::portName() const
{
    return m_portName;
}

void ProgramSettings::setPortName(const QString &newPortName)
{
    m_portName = newPortName;
}

const QString &ProgramSettings::midiDriver() const
{
    return m_midiDriver;
}

void ProgramSettings::setMidiDriver(const QString &newMidiDriver)
{
    m_midiDriver = newMidiDriver;
}

const QString &ProgramSettings::audioDeviceName() const
{
    return m_audioDeviceName;
}

void ProgramSettings::setAudioDeviceName(const QString &newAudioDeviceName)
{
    m_audioDeviceName = newAudioDeviceName;
}

int ProgramSettings::chorusLevel() const
{
    return m_chorusLevel;
}

void ProgramSettings::setChorusLevel(int chorusLevel)
{
    m_chorusLevel = chorusLevel;
}

int ProgramSettings::chorusType() const
{
    return m_chorusType;
}

void ProgramSettings::setChorusType(int chorusType)
{
    m_chorusType = chorusType;
}

int ProgramSettings::reverbWet() const
{
    return m_reverbWet;
}

void ProgramSettings::setReverbWet(int reverbWet)
{
    m_reverbWet = reverbWet;
}

int ProgramSettings::reverbType() const
{
    return m_reverbType;
}

void ProgramSettings::setReverbType(int reverbType)
{
    m_reverbType = reverbType;
}

int ProgramSettings::bufferTime() const
{
    return m_bufferTime;
}

void ProgramSettings::setBufferTime(int bufferTime)
{
    m_bufferTime = bufferTime;
}
