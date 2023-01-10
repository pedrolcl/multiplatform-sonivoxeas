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

#include "programsettings.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("SonivoxEAS");
    QApplication::setApplicationName("mp_GUISynth");
    QApplication::setApplicationVersion(QT_STRINGIFY(VERSION));
    QApplication::setDesktopFileName("sonivoxeas");
    QApplication::setWindowIcon(QIcon(":/icon.png"));
    QCommandLineParser parser;
    parser.setApplicationDescription("GUI MIDI Synthesizer and Player");
    parser.addVersionOption();
    parser.addHelpOption();
    QCommandLineOption driverOption({"d", "driver"}, "MIDI Driver.", "driver");
    QCommandLineOption portOption({"p", "port"}, "MIDI Port.", "port");
    QCommandLineOption listOption({"s", "subs"}, "List available MIDI Ports.");
    QCommandLineOption bufferOption({"b", "buffer"}, "Audio buffer time in milliseconds", "bufer_time", "100");
    QCommandLineOption deviceOption({"a", "audiodevice"}, "Audio Device Name", "device_name", "default");
    parser.addOption(driverOption);
    parser.addOption(portOption);
    parser.addOption(listOption);
    parser.addOption(bufferOption);
    parser.addOption(deviceOption);
    parser.addPositionalArgument("file", "MIDI File (*.mid; *.kar)");
    parser.process(app);
    ProgramSettings::instance()->ReadFromNativeStorage();
    if (parser.isSet(driverOption)) {
        QString driverName = parser.value(driverOption);
        if (!driverName.isEmpty()) {
            ProgramSettings::instance()->setMidiDriver(driverName);
        }
    }
    if(parser.isSet(portOption)) {
        QString portName = parser.value(portOption);
        if (!portName.isEmpty()) {
            ProgramSettings::instance()->setPortName(portName);
        }
    }
    if (parser.isSet(bufferOption)) {
        int n = parser.value(bufferOption).toInt();
        if (n > 0)
            ProgramSettings::instance()->setBufferTime(n);
        else {
            fputs("Wrong buffer time.\n", stderr);
            parser.showHelp(1);
        }
    }
    if (parser.isSet(deviceOption)) {
        QString s = parser.value(deviceOption);
        if (!s.isEmpty()) {
            ProgramSettings::instance()->setAudioDeviceName(s);
        } else {
            fputs("Wrong Device Name.\n", stderr);
            parser.showHelp(1);
        }
    }
    MainWindow w;
    if (parser.isSet(listOption)) {
        w.listPorts();
        return EXIT_SUCCESS;
    }
    QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
        w.readFile(args.first());
    w.show();
    return app.exec();
}
