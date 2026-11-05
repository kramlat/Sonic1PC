#pragma once

// Only exists in SPLASH builds (Premier/Showcase) -- see Game.h's cli_countdown comment. Sonic 2 has it in them too.
void GM_Countdown(void);

// Tools > Countdown (the engine's Demo.h declares Countdown_Available and Countdown_Request): the request waits for a screen that can be left
// (the same ones a demo request does), and then the countdown starts, handing back to the title screen when it ends.
void Countdown_ServiceRequest(void); // called once per frame from VBlank
