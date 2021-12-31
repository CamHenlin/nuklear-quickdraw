# Nuklear QuickDraw
### Or "Nuklear for Classic Macintosh"
This repo is intended to be used as a template from which to build other Nuklear-based applications for Classic Macintosh. [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) is an immediate mode graphics toolkit. It has been modified for performance and had graphics drivers to work on Classic Macintosh systems. All development and testing was done on System 6.

You could probably refer to the `nuklear.h` file in this repo as a sort of Nuklear-lite. Nuklear initially had poor performance on Classic Macintosh systems in a "death by 1000 cuts" situation. To that end, Nuklear has been modified in the following ways to help optimize for performance on 8MHz Macintosh systems:

- all floating point math has been moved to int-based math. Some math was refactored to support this
- some coloring commands have been removed (black and white screen) -- likely more could be done here
- UTF8 font handling was partially removed -- again, likely more could be done here
- asserts were removed
- some draw command caching was implemented in conjunction with the QuickDraw-specific rendering code contained in this repo.

This has resulted in some bugs but is generally usable. `nuklear_app.c` contained in the repo is Nuklear's mostly-unmodified calculator demo. If you use this library and decide to fix any bugs, please feel free to submit PRs directly to this repo or submit issues and we can chat about them. I would love additional contributors here. I think performance and usability can be advanced pretty far beyond what I've gotten things to here.

## Using this repo as intended
If you would like to use this repo to as a base to develop your own Nuklear-based applications, here is a breakdown of the important files that you might find helpful:

#### nuklear_app.c
This is likely where you will spend the majority of your time. Right now, this contains the Nuklear calculator demo. You can implement your own cool Nuklear app here!

#### mac_main.c and other mac_main files
These files mostly contain Macintosh-application specific boilerplate and hooks into Nuklear code, but you may have to jump in and modify them sometimes. Some common cases include:

- problems with event handling. Events are passed to Nuklear through these files in `EventLoop`
- need to customize Macintosh menu bar menu handling in `DoMenuCommand`

#### nuklear_quickdraw.h
This is the QuickDraw driver for rendering Nuklear commands. If you have any issues or want to change things about how drawing works, look here or in `nuklear.h`

#### nuklear.h
This is the modified version of nuklear.h from [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) 

## Using Nuklear QuickDraw? Tell me about it!
Here's a list of software built using Nuklear QuickDraw:

- [MessagesForMacintosh](https://github.com/CamHenlin/MessagesForMacintosh)

## Demo 
For a usable demo, see [this blog post](https://henlin.net/2021/12/21/Introducing-Nuklear-for-Macintosh/) where there's an embedded version of this repo's calculator app running a Nuklear emulator. Otherwise, check out this animated gif:

