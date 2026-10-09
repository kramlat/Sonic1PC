#pragma once

// Tools menu: record a demo, play a demo (see Demo.h for how the game applies the requests).

class QMainWindow;
class QWidget;

namespace DemoTools {

void RecordDialog(QWidget *parent);  // asks which level to record, then files the request
void PlayDialog(QWidget *parent);    // asks for a demo file (and its level), then files the request
void CountdownDialog(QWidget *parent); // asks for the song and the length, then files a countdown request (Demo.h's Countdown_Request)
void StopRecording();                // saves the recording and carries on playing
bool Recording();                    // a recording is running (or about to start)

// Called once per frame: keeps the status bar and window title in step with the recording, and writes the
// metadata file when one ends (by the menu or by its frame limit).
void Poll(QMainWindow *window);

} // namespace DemoTools
