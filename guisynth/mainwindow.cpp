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

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>

#include "mainwindow.h"
#include "programsettings.h"
#include "ui_mainwindow.h"
#include <drumstick/pianokeybd.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_state(InitialState)
{
    m_ui->setupUi(this);
    m_synth = new SynthController(ProgramSettings::instance()->bufferTime(), this);

    m_ui->combo_Device->addItems(m_synth->availableAudioDevices());
    m_ui->combo_Device->setCurrentText(ProgramSettings::instance()->audioDeviceName());
    m_synth->setAudioDeviceName(ProgramSettings::instance()->audioDeviceName());
    m_ui->combo_MIDI->addItems(m_synth->connections());
    m_ui->combo_Reverb->addItem(QStringLiteral("Large Hall"), 0);
    m_ui->combo_Reverb->addItem(QStringLiteral("Hall"), 1);
    m_ui->combo_Reverb->addItem(QStringLiteral("Chamber"), 2);
    m_ui->combo_Reverb->addItem(QStringLiteral("Room"), 3);
    m_ui->combo_Reverb->addItem(QStringLiteral("None"), -1);
    m_ui->combo_Reverb->setCurrentIndex(4);
    m_ui->combo_Chorus->addItem(QStringLiteral("Preset 1"), 0);
    m_ui->combo_Chorus->addItem(QStringLiteral("Preset 2"), 1);
    m_ui->combo_Chorus->addItem(QStringLiteral("Preset 3"), 2);
    m_ui->combo_Chorus->addItem(QStringLiteral("Preset 4"), 3);
    m_ui->combo_Chorus->addItem(QStringLiteral("None"), -1);
    m_ui->combo_Chorus->setCurrentIndex(4);
    connect(m_ui->combo_MIDI, SIGNAL(currentIndexChanged(int)), this, SLOT(subscriptionChanged(int)));
    connect(m_ui->combo_Device, SIGNAL(currentIndexChanged(int)), this, SLOT(deviceChanged(int)));
    connect(m_ui->combo_Reverb, SIGNAL(currentIndexChanged(int)), this, SLOT(reverbTypeChanged(int)));
    connect(m_ui->combo_Chorus, SIGNAL(currentIndexChanged(int)), this, SLOT(chorusTypeChanged(int)));
    connect(m_ui->spinBuffer, SIGNAL(valueChanged(int)), this, SLOT(bufferSizeChanged(int)));
    connect(m_ui->spinOctave, SIGNAL(valueChanged(int)), this, SLOT(octaveChanged(int)));
    connect(m_ui->volumeSlider, &QSlider::valueChanged, this, &MainWindow::volumeChanged);
    connect(m_ui->dial_Reverb, &QDial::valueChanged, this, &MainWindow::reverbChanged);
    connect(m_ui->dial_Chorus, &QDial::valueChanged, this, &MainWindow::chorusChanged);
    connect(m_ui->openSMFButton, &QToolButton::clicked, this, &MainWindow::openMIDIFile);
    connect(m_ui->openDLSButton, &QToolButton::clicked, this, &MainWindow::openSoundfont);
    connect(m_ui->playButton, &QToolButton::clicked, this, &MainWindow::playSong);
    connect(m_ui->stopButton, &QToolButton::clicked, this, &MainWindow::stopSong);
    connect(m_ui->pianoKeybd, SIGNAL(noteOn(int,int)), this, SLOT(noteOn(int,int)));
    connect(m_ui->pianoKeybd, SIGNAL(noteOff(int,int)), this, SLOT(noteOff(int,int)));
    connect(m_synth, SIGNAL(midiNoteOn(int, int)), this, SLOT(showNoteOn(int, int)));
    connect(m_synth, SIGNAL(midiNoteOff(int, int)), this, SLOT(showNoteOff(int, int)));
    connect(m_synth, SIGNAL(playbackStopped()), this, SLOT(songStopped()));
    connect(m_synth, &SynthController::underrunDetected, this, &MainWindow::underrunMessage);
    connect(m_synth, &SynthController::stallDetected, this, &MainWindow::stallMessage);
    connect(m_synth, &SynthController::synthStarted, this, &MainWindow::initializeSynth);

    updateState(EmptyState);
    adjustSize();
    m_synth->restart();
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void
MainWindow::initializeSynth()
{
    qDebug() << Q_FUNC_INFO;
    m_ui->spinBuffer->setValue(ProgramSettings::instance()->bufferTime());
    int reverb = m_ui->combo_Reverb->findData(ProgramSettings::instance()->reverbType());
    m_ui->combo_Reverb->setCurrentIndex(reverb);
    m_ui->dial_Reverb->setValue(ProgramSettings::instance()->reverbWet()); //0..32765
    int chorus = m_ui->combo_Chorus->findData(ProgramSettings::instance()->chorusType());
    m_ui->combo_Chorus->setCurrentIndex(chorus);
    m_ui->dial_Chorus->setValue(ProgramSettings::instance()->chorusLevel());
    m_ui->volumeSlider->setValue(ProgramSettings::instance()->volumeLevel());
    QFileInfo dlsInfo(ProgramSettings::instance()->Soundfont());
    if (dlsInfo.exists() && dlsInfo.isReadable()) {
        readSoundfont(dlsInfo);
    } else {
        m_ui->lblSoundfont->setText("[empty]");
        m_soundFont.clear();
        ProgramSettings::instance()->setSoundfont(m_soundFont);
        m_synth->initSoundfont(m_soundFont);
    }
    QFont f = QApplication::font();
    f.setPointSize(72);
    m_ui->pianoKeybd->setFont(f);
    m_ui->pianoKeybd->setShowLabels(drumstick::widgets::LabelVisibility::ShowMinimum);
    m_ui->pianoKeybd->setNumKeys(25, 0);
    m_synth->setMidiDriver(ProgramSettings::instance()->midiDriver());
    auto midiPort = ProgramSettings::instance()->portName();
    if (m_ui->combo_MIDI->count() > 0) {
        if (midiPort.isEmpty()) {
            subscriptionChanged(0);
        } else {
            m_ui->combo_MIDI->setCurrentText(midiPort);
            m_synth->subscribe(midiPort);
        }
    }
}

void
MainWindow::showEvent(QShowEvent* ev)
{
    ev->accept();
}

void
MainWindow::closeEvent(QCloseEvent* ev)
{
    m_synth->stop();
    ProgramSettings::instance()->SaveToNativeStorage();
    ev->accept();
}

void
MainWindow::reverbTypeChanged(int index)
{
    int value = m_ui->combo_Reverb->itemData(index).toInt();
    m_synth->initReverb(value);
    ProgramSettings::instance()->setReverbType(value);
    if (value < 0) {
        m_ui->dial_Reverb->setValue(0);
        ProgramSettings::instance()->setReverbWet(0);
    }
}

void
MainWindow::reverbChanged(int value)
{
    m_synth->setReverbWet(value);
    ProgramSettings::instance()->setReverbWet(value);
}

void
MainWindow::chorusTypeChanged(int index)
{
    int value = m_ui->combo_Chorus->itemData(index).toInt();
    m_synth->initChorus(value);
    ProgramSettings::instance()->setChorusType(value);
    if (value < 0) {
        m_ui->dial_Chorus->setValue(0);
        ProgramSettings::instance()->setChorusLevel(0);
    }
}

void
MainWindow::chorusChanged(int value)
{
    m_synth->setChorusLevel(value);
    ProgramSettings::instance()->setChorusLevel(value);
}

void MainWindow::deviceChanged(int value)
{
    auto newDevice = m_ui->combo_Device->itemText(value);
    //qDebug() << Q_FUNC_INFO << newDevice;
    m_synth->setAudioDeviceName(newDevice);
    ProgramSettings::instance()->setAudioDeviceName(newDevice);
}

void MainWindow::subscriptionChanged(int value)
{
    QString portName = m_ui->combo_MIDI->itemText(value);
    qDebug() << Q_FUNC_INFO << value << portName;
    m_synth->subscribe(portName);
    ProgramSettings::instance()->setPortName(portName);
}

void MainWindow::bufferSizeChanged(int value)
{
    //qDebug() << Q_FUNC_INFO << value;
    m_synth->setBufferSize(value);
    ProgramSettings::instance()->setBufferTime(value);
}

void MainWindow::octaveChanged(int value)
{
    m_ui->pianoKeybd->setBaseOctave(value);
    //ProgramSettings::instance()->setOctave(value);
}

void MainWindow::volumeChanged(int value)
{
    //qDebug() << Q_FUNC_INFO << value;
    m_synth->setVolume(value);
    ProgramSettings::instance()->setVolumeLevel(value);
}

void
MainWindow::readMIDIFile(const QString &file)
{
    if (!file.isEmpty() && file != m_songFile) {
        QFileInfo f(file);
        if (f.exists()) {
            m_songFile = f.absoluteFilePath();
            m_ui->lblSong->setText(f.fileName());
            updateState(StoppedState);
        }
    }
}

void MainWindow::listPorts()
{
    auto avail = m_synth->connections();
    fputs("Available MIDI Ports:\n", stdout);
    foreach(const auto &p, avail) {
        if (!p.isEmpty()) {
            fputs(p.toLocal8Bit(), stdout);
            fputs("\n", stdout);
        }
    }
}

void
MainWindow::openMIDIFile()
{
    QString songFile
        = QFileDialog::getOpenFileName(this,
                                       tr("Open MIDI file"),
                                       QString(),
                                       tr("MIDI Files (*.mid *.midi *.kar *.rmi *.xmf *.mxmf)"));
    if (songFile.isEmpty()) {
        m_ui->lblSong->setText("[empty]");
        updateState(EmptyState);
    } else {
        readMIDIFile(songFile);
    }
}

void MainWindow::readSoundfont(const QFileInfo &file)
{
    if (file.exists() && file.isReadable()) {
        m_soundFont = file.absoluteFilePath();
        m_ui->lblSoundfont->setText(file.fileName());
        m_synth->initSoundfont(m_soundFont);
        ProgramSettings::instance()->setSoundfont(m_soundFont);
    }
}

void MainWindow::openSoundfont()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open DLS file"),
                                                    QString(),
                                                    tr("Soundfonts (*.dls *.sf2)"));
    if (fileName.isEmpty()) {
        m_soundFont.clear();
        m_ui->lblSoundfont->setText("[empty]");
        ProgramSettings::instance()->setSoundfont(m_soundFont);
        m_synth->initSoundfont(m_soundFont);
    } else {
        QFileInfo fInfo(fileName);
        readSoundfont(fInfo);
    }
}

void
MainWindow::playSong()
{
    if (m_state == StoppedState) {
        m_synth->startPlayback(m_songFile);
        updateState(PlayingState);
    }
}

void
MainWindow::stopSong()
{
    if (m_state == PlayingState) {
        m_synth->stopPlayback();
        updateState(StoppedState);
    }
}

void
MainWindow::songStopped()
{
    if (m_state != StoppedState) {
        updateState(StoppedState);
    }
}

void
MainWindow::updateState(PlayerState newState)
{
    //qDebug() << Q_FUNC_INFO << newState;
    if (m_state != newState) {
        switch (newState) {
        case EmptyState:
            m_ui->playButton->setEnabled(false);
            m_ui->stopButton->setEnabled(false);
            m_ui->openSMFButton->setEnabled(true);
            break;
        case PlayingState:
            m_ui->playButton->setEnabled(false);
            m_ui->stopButton->setEnabled(true);
            m_ui->openSMFButton->setEnabled(false);
            break;
        case StoppedState:
            m_ui->stopButton->setEnabled(true);
            m_ui->playButton->setEnabled(true);
            m_ui->playButton->setChecked(false);
            m_ui->openSMFButton->setEnabled(true);
            break;
        default:
            break;
        }
        m_state = newState;
    }
}

void MainWindow::underrunMessage()
{
    static bool showing = false;
    if (!showing) {
        showing = true;
        QMessageBox::warning( this, "Underrun Error",
                              "Audio buffer underrun errors have been detected."
                              " Please increase the buffer time to avoid this problem.");
        showing = false;
    }
}

void MainWindow::stallMessage()
{
    QMessageBox::critical( this, "Audio Output Stalled",
                           "Audio output is stalled right now. Sound cannot be produced."
                           " Please increase the buffer time to avoid this problem.");
    m_synth->stop();
}

void MainWindow::noteOn(int midiNote, int vel)
{
    m_synth->noteOn(0, midiNote, vel);
}

void MainWindow::noteOff(int midiNote, int vel)
{
    m_synth->noteOff(0, midiNote, vel);
}

void MainWindow::showNoteOn(int midiNote, int vel)
{
    m_ui->pianoKeybd->showNoteOn(midiNote, vel);
}

void MainWindow::showNoteOff(int midiNote, int vel)
{
    m_ui->pianoKeybd->showNoteOff(midiNote, vel);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QString path;
        const QList<QUrl> urlList = mimeData->urls();
        if (urlList.count() > 0) {
            path = urlList.first().toLocalFile();
        }

        if (!path.isEmpty()) {
            QFileInfo fInfo(path);
            if (fInfo.exists()) {
                const auto ext = fInfo.suffix().toLower();
                if (ext == "mid" || ext == "midi" || ext == "kar" || ext == "rmi" || ext == "xmf"
                    || ext == "mxmf") {
                    readMIDIFile(fInfo.absoluteFilePath());
                } else if (ext == "dls" || ext == "sf2") {
                    readSoundfont(fInfo);
                }
            }
        }
    }
}
