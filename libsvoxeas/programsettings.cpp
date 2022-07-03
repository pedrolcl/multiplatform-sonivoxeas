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

#include "programsettings.h"

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
    m_bufferTime = 60;
    m_reverbType = 1;
    m_reverbWet = 25800;
    m_chorusType = -1;
    m_chorusLevel = 0;
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
    m_midiDriver = settings.value("MIDIDriver", DEFAULT_DRIVER).toString();
    m_portName = settings.value("PortName", QLatin1String()).toString();
    m_bufferTime = settings.value("BufferTime", 60).toInt();
    m_reverbType = settings.value("ReverbType", 1).toInt();
    m_reverbWet = settings.value("ReverbWet", 25800).toInt();
    m_chorusType = settings.value("ChorusType", -1).toInt();
    m_chorusLevel = settings.value("ChorusLevel", 0).toInt();
    m_audioDeviceName = settings.value("AudioDevice", "default").toString();
    emit ValuesChanged();
}

void ProgramSettings::internalSave(QSettings &settings)
{
    settings.setValue("MIDIDriver", m_midiDriver);
    settings.setValue("PortName", m_portName);
    settings.setValue("BufferTime", m_bufferTime);
    settings.setValue("ReverbType", m_reverbType);
    settings.setValue("ReverbWet", m_reverbWet);
    settings.setValue("ChorusType", m_chorusType);
    settings.setValue("ChorusLevel", m_chorusLevel);
    settings.setValue("AudioDevice", m_audioDeviceName);
    settings.sync();
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
