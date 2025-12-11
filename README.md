# gps-nav

A lightweight, circular-buffer based GPS navigation system for open.mp.

## How it works

Drawing a line from LS to LV takes way more than the 1024 gangzones SA-MP gives us per player. Most scripts just hit the limit and break.

It simply bumps the pointer. When the buffer is full (default `600`), it pauses rendering the path ahead until you move forward and free up slots in the tail. It essentially "streams" the path to the client.

## Dependencies

You need the pathfinding plugin to calculate the nodes This library just handles the rendering logic.

- **[open.mp](https://github.com/openmultiplayer/open.mp)**
- **[omp-stdlib](https://github.com/openmultiplayer/omp-stdlib)**
- **[samp-gps-plugin](https://github.com/AmyrAhmady/samp-gps-plugin)** (Handles the A* math on a separate thread so the server doesn't choke)


It should *theoretically* also work with SA-MP's standard library/server, but you will need [YSF](https://github.com/IS4Code/YSF) for player gangzones. 

I haven't tested it myself, so you're on your own. If you run into any SA-MP issues while testing, feel free to [open an issue](https://github.com/FreddieCrew/gps-nav/issues) or [submit a PR](https://github.com/FreddieCrew/gps-nav/pulls), and I'll surely have a look.

---

# READ THIS IF YOU'RE USING OPEN.MP

## The issue:
If you are using the latest **open.mp** includes, you are going to get a compiler error that looks like this: `GPS.inc(12) : error 020: invalid symbol name ""`
 The `GPS` plugin include defines a constant named [`INVALID_PATH_ID`](https://github.com/AmyrAhmady/samp-gps-plugin/blob/master/GPS.inc#L12).

The latest open.mp standard library *also* introduced a constant named [`INVALID_PATH_ID`](https://github.com/openmultiplayer/omp-stdlib/blob/f86393d50e6c242a99e2153c398cce13949ef6f0/omp_npc.inc#L52) (for the NPC component). Since Pawn has a global namespace, they conflict.

## How to fix it:
Open your `GPS.inc` file (from the plugin), find the definition, and rename it to avoid the collision:

```pawn
// Inside GPS.inc (line 10)
// Change this line:
#define INVALID_PATH_ID (0)

// To this:
#define INVALID_GPS_PATH_ID (0)
```

This library (`gps-nav`) expects the path ID to be passed around anyway, so just make sure your plugin include doesn't conflict with the stdlib.

---

## Configuration

You can tweak the settings at the top of your script before including the file.

```pawn
// 600 is a good sweetspot. Sufficient for buffer, saves memory.
// Going higher than 800 risks hitting the per-player gangzone limit if you use zones elsewhere.
#define MAX_GPS_ZONES 600 

#define GPS_LINE_WIDTH 15.0
#define GPS_COLOR 0xA850E6FF
#define GPS_UPDATE_INTERVAL 400 // Milliseconds
```

## Usage

It's plug-and-play.

```pawn
#include <gps-nav>

// Start navigation
// Returns 1 if path found, 0 if failed.
GPS_Start(playerid, targetX, targetY, targetZ);

// Stop navigation
GPS_Stop(playerid);

// Check if active
if(GPS_IsActive(playerid)) {
    // ...
}
```

## Callbacks

These are useful events to handle UI elements (like setting the map marker).

### `OnPlayerGPSActivate`
Called immediately when `GPS_Start` is initialized.

Example:

```pawn
public OnPlayerGPSActivate(playerid, Float:X, Float:Y, Float:Z) {
    // Set a global waypoint icon (ID 0) so they can see where they are going
    SetPlayerMapIcon(playerid, 0, X, Y, Z, 41, 0, MAPICON_GLOBAL); 
    return 1;
}
```

### `OnPlayerGPSArrival`
Called when the player is within range of the destination.

Example:
```pawn
public OnPlayerGPSArrival(playerid) {
    RemovePlayerMapIcon(playerid, 0); 
    GameTextForPlayer(playerid, "~g~Arrived", 3000, 3);
    return 1;
}
```

## Notes
> [!NOTE]
> The pathfinding happens on a separate thread (thanks to the plugin), but the rendering happens in the update tick. The circular buffer keeps it cheap, as long as you don't set `GPS_UPDATE_INTERVAL` to something like 10ms.