#pragma once

// Touch UI: portrait 240x320, status pages + control buttons (kickoff
// decision: status + controls; config stays on the serial menu). This is
// the module that replaces the Feather's 128x32 OLED pages + 3 buttons.
void uiInit();
void uiPoll();  // touch handling + throttled redraw; call every loop pass
