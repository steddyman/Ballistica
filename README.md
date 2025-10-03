# 3DS Ballistica Game

This repo contains a port of the Ballistica game engine to the Nintendo 3DS platform.

This game was originally developed by [Stephen Eddy](https://x.com/steddyman) for the IBM PC with VGA in 1994.  The 3DS version is a port of the original DOS codebase, with some enhancements and modifications to work on the 3DS hardware.

It makes use of both 3DS screens and the 3DS controls.  It contains a full level editor, so you can create your own levels and play them on the 3DS.

## Screenshots

<table>
	<tr>
		<td align="center">
			<a href="screenshots/main-menu.png">
				<img src="screenshots/main-menu.png" alt="Main menu" width="410"/>
			</a>
			<br/><sub>Main Menu</sub>
		</td>
		<td align="center">
			<a href="screenshots/playing1.png">
				<img src="screenshots/playing1.png" alt="Playing (1)" width="410"/>
			</a>
			<br/><sub>Gameplay</sub>
		</td>
	</tr>
	<tr>
		<td align="center">
			<a href="screenshots/level-editor1.png">
				<img src="screenshots/level-editor1.png" alt="Level editor" width="410"/>
			</a>
			<br/><sub>Level Editor</sub>
		</td>
		<td align="center">
			<a href="screenshots/playing2.png">
				<img src="screenshots/playing2.png" alt="Playing (2)" width="410"/>
			</a>
			<br/><sub>More Gameplay</sub>
		</td>
	</tr>
  
</table>

# Using the Release Build

You can download the latest release build from the Releases section of this repository.  Look for a file named `ballistica.3dsx`.  Copy this file to the **/3ds/ballistica** folder of your 3DS SD card and run it using the homebrew launcher.

# Controls

The game uses the following controls:
- Stylus - Move the paddle, select menu items, draw in the level editor
- D-Pad Up - Launch ball (when in play mode), Fire Laser (when collected)
- D-Pad Down - Activate Tilt (when indicator is shown)

If the game detects a ball may be stuck, it will flash up the Tilt indicator.  Press D-Pad Down to activate the tilt and free the ball.

# Contributing

The levels are stored in the `levels` folder.  You can create your own levels using the in-game level editor, or by editing the text files directly, then copying the .DAT files from the **/ballistica/levels** folder on your 3DS SD card to your computer.

I encourage you to create your own levels and share them with others, by raising a PR against this repository with a description of your levels.

If this repo is useful to you, please consider starring it to help others find it and tell your friends about the game.
** **

## Local CIA Build (Testing Without GitHub Actions)

You can build the CIA locally to iterate on `assets/cia/cia.rsf` without waiting for the release pipeline.

Prerequisites:
1. devkitPro / devkitARM installed (environment variables `DEVKITPRO` and `DEVKITARM` set or available via your shell profile) so that `make` produces `Ballistica.3dsx` / `Ballistica.elf`.
2. Platform tools included in this repo:
	- macOS (Apple Silicon): `bin/mac-arm64/bannertool`, `bin/mac-arm64/makerom`
	- macOS (Intel / Rosetta): `bin/mac-x86_64/bannertool`, `bin/mac-x86_64/makerom`
	- Linux (x86_64): `bin/linux-x86_64/bannertool`, `bin/linux-x86_64/makerom`
3. Assets present:
	- `assets/cia/icon.png`
	- `assets/cia/banner.png`
	- `assets/cia/banner.wav` (e.g. generated with: `ffmpeg -i music.wav -ss 0 -t 3 -ar 32000 -ac 2 -acodec pcm_s16le banner.wav`)
	- `assets/cia/cia.rsf`

Run:
```bash
chmod +x scripts/build_cia_local.sh
./scripts/build_cia_local.sh
```

Artifacts will appear in `dist/` (`Ballistica.cia`, plus `banner.bnr` and `Ballistica.smdh`).

If makerom fails with an ExHeader save size error, the script auto-retries by injecting a small `SaveDataSize` (256KB) into a temporary RSF so you can decide whether to add it permanently.

Current `UniqueId` in `cia.rsf` is a placeholder (`0x12345`). Replace with a stable unique value before public distribution to avoid collisions with other homebrew titles installed on the same system.