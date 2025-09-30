# 3DS Ballistica Game

This repo contains a port of the Ballistica game engine to the Nintendo 3DS platform.

This game was originally by Stephen Eddy for the IBM PC with VGA in 1994.  The 3DS version is a port of the original DOS codebase, with some enhancements and modifications to work on the 3DS hardware.

It makes use of both 3DS screens and the 3DS controls.  It contains a full level editor, so you can create your own levels and play them on the 3DS.

# Using the Release Build

You can download the latest release build from the Releases section of this repository.  Look for a file named `ballistica.3dsx`.  Copy this file to the /3ds/ballistica folder of your 3DS SD card and run it using the homebrew launcher.

# Controls

The game uses the following controls:
- Stylus - Move the paddle, select menu items, draw in the level editor
- D-Pad Up - Launch ball (when in play mode), Fire Laser (when collected)
- D-Pad Down - Activate Tilt (when indicator is shown)

If the game detects a ball may be stuck, it will flash up the Tilt indicator.  Press D-Pad Down to activate the tilt and free the ball.
