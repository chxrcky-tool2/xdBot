Credits to Zilko, and to whoever Zilko credited in their ReadMe if they have.

all rights reserved©

Lowk, I wanna make it xdDroid since I wanted to fork xdBot for Android, cuz I have no windows
but will keep xdBot since it still sounds cool.

What I changed:
- Ported to Gd 2.2091, and geode v6.0.0 from gd 2.2074.
- Changed platform support from win and android32/64 to only Android64/32
- Completely removed rendering since the build always fail with rendering.
- UI changed from cocos panel to Megahack styled ImGui Ui interface.
- floating xd button, can be draggable anywhere.
- Legacy .xd format came back from .gdr2, so it looks cool.

Plans:
- Compression of format saving, 20 bytes per input and release and more.
- Labels {
• CPS, style is like 0/0/0, (CPS/Highest CPS reached/total clicks)
• Recording label/Inputs overlay, style is "Recording 0/0" Click/release. Done! ✓
}
- Labels tab to put the features above.

- Rendering again {
- Add new tab, called *"Internal Renderer"*
- Codec change, resolution and fps change included.
pressing codec will reveal like 2-6 Codec to choose from.
- built-in, not requiring FFmpeg Api dependency
- Help button next to Resolution, which picks your phone's supported resolution.
}
- ImGui themes to choose from. can make one via adjusting colors in  a new tab called *Themes* which will contain all things for theme choosing.

- Headless physics loop, when 6k tps, your icon player will experience physics of 6k tps, but your screen will show your native Hz/FPS.

Note: If you're gonna fork ZiLko's xdBot or this fork, remind to you to credit ZiLko always.
