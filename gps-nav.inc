#if defined _gpsnav_included
    #endinput
#endif
#define _gpsnav_included

#tryinclude <open.mp>
#include <GPS>

// Settings

#if !defined MAX_GPS_ZONES
    #define MAX_GPS_ZONES       600     // 600 is a good sweetspot. Sufficient for buffer, saves memory
#endif

#if !defined GPS_LINE_WIDTH
    #define GPS_LINE_WIDTH      15.0
#endif

#if !defined GPS_COLOR
    #define GPS_COLOR           0xA850E6FF
#endif

#if !defined GPS_UPDATE_INTERVAL
    #define GPS_UPDATE_INTERVAL 400     // Note: these values are in milliseconds
#endif

#if !defined GPS_MAX_DEVIATION
    #define GPS_MAX_DEVIATION   25.0
#endif

#if !defined GPS_RECALC_COOLDOWN
    #define GPS_RECALC_COOLDOWN 2000
#endif

#if !defined GPS_SMOOTHNESS
    #define GPS_SMOOTHNESS      0.55
#endif

enum E_GPS_DATA {
    bool:gps_IsActive,
    Path:gps_CurrentPath,
    gps_PathNodeCount,
    gps_LastLoadedNode,
    gps_ZoneHead,
    gps_ZoneTail,

    gps_SeqCounter,
    gps_ZoneSeq[MAX_GPS_ZONES],
    gps_ZoneIDs[MAX_GPS_ZONES],
    Float:gps_ZoneX[MAX_GPS_ZONES],
    Float:gps_ZoneY[MAX_GPS_ZONES],
    gps_Timer,
    Float:gps_DestX, 
    Float:gps_DestY, 
    Float:gps_DestZ,
    gps_LastRecalc
};

static g_GPSData[MAX_PLAYERS][E_GPS_DATA];

forward OnPlayerGPSActivate(playerid, Float:X, Float:Y, Float:Z);
forward OnPlayerGPSArrival(playerid);


/*
    Starts the GPS for a player.
*/
stock GPS_Start(playerid, Float:targetX, Float:targetY, Float:targetZ) {
    g_GPSData[playerid][gps_DestX] = targetX;
    g_GPSData[playerid][gps_DestY] = targetY;
    g_GPSData[playerid][gps_DestZ] = targetZ;
    
    GPS_Stop(playerid); 

    new Float:pX, Float:pY, Float:pZ;
    GetPlayerPos(playerid, pX, pY, pZ);

    new MapNode:startNode, MapNode:targetNode;
    if(GetClosestMapNodeToPoint(pX, pY, pZ, startNode) != 0) return 0;
    if(GetClosestMapNodeToPoint(targetX, targetY, targetZ, targetNode) != 0) return 0;

    CallLocalFunction("OnPlayerGPSActivate", "ifff", playerid, targetX, targetY, targetZ);

    FindPathThreaded(startNode, targetNode, "GPS_OnCalculated", "i", playerid);
    return 1;
}

/*
    Stops the active GPS.
*/
stock GPS_Stop(playerid) {
    if(g_GPSData[playerid][gps_IsActive]) {
        KillTimer(g_GPSData[playerid][gps_Timer]);
        if(IsValidPath(g_GPSData[playerid][gps_CurrentPath])) DestroyPath(g_GPSData[playerid][gps_CurrentPath]);

        GPS_Internal_Cleanup(playerid);
        GPS_Internal_ResetVars(playerid);
    }
    return 1;
}

/*
    Checks if GPS is active.
*/
stock bool:GPS_IsActive(playerid) {
    return g_GPSData[playerid][gps_IsActive];
}

static stock GPS_Internal_ResetVars(playerid) {
    g_GPSData[playerid][gps_IsActive] = false;
    g_GPSData[playerid][gps_ZoneHead] = 0;
    g_GPSData[playerid][gps_ZoneTail] = 0;
    for(new i = 0; i < MAX_GPS_ZONES; i++) {
        g_GPSData[playerid][gps_ZoneIDs][i] = -1;
    }
}

static stock GPS_Internal_Cleanup(playerid) {
    for(new i = 0; i < MAX_GPS_ZONES; i++) {
        if(g_GPSData[playerid][gps_ZoneIDs][i] != -1) {
            PlayerGangZoneDestroy(playerid, g_GPSData[playerid][gps_ZoneIDs][i]);
            g_GPSData[playerid][gps_ZoneIDs][i] = -1;
        }
    }
}

static stock GPS_Internal_Rasterize(playerid, Float:x1, Float:y1, Float:x2, Float:y2) {
    new Float:dist = VectorSize(x2 - x1, y2 - y1, 0.0);
    new Float:stepDist = GPS_LINE_WIDTH * GPS_SMOOTHNESS; 
    new steps = floatround(dist / stepDist);
    if(steps < 1) steps = 1;

    new Float:stepX = (x2 - x1) / steps;
    new Float:stepY = (y2 - y1) / steps;
    new Float:curX = x1;
    new Float:curY = y1;
    new Float:halfW = GPS_LINE_WIDTH / 2.0;

    for(new i = 0; i < steps; i++) {
        new head = g_GPSData[playerid][gps_ZoneHead];
        new nextHead = (head + 1) % MAX_GPS_ZONES;
        
        if(nextHead == g_GPSData[playerid][gps_ZoneTail]) {
            return 0;
        }

        // Clean slot if dirty (shouldn't happen, but I don't trust my code)
        if(g_GPSData[playerid][gps_ZoneIDs][head] != -1) {
            PlayerGangZoneDestroy(playerid, g_GPSData[playerid][gps_ZoneIDs][head]);
            g_GPSData[playerid][gps_ZoneIDs][head] = -1;
        }

        new zId = CreatePlayerGangZone(playerid, 
            curX - halfW, curY - halfW, 
            curX + halfW, curY + halfW
        );
        
        if(zId != -1) {
            PlayerGangZoneShow(playerid, zId, GPS_COLOR);
            
            g_GPSData[playerid][gps_ZoneIDs][head] = zId;
            g_GPSData[playerid][gps_ZoneX][head] = curX;
            g_GPSData[playerid][gps_ZoneY][head] = curY;
            g_GPSData[playerid][gps_ZoneSeq][head] = g_GPSData[playerid][gps_SeqCounter]++;
            g_GPSData[playerid][gps_ZoneHead] = nextHead;
        }
        
        curX += stepX;
        curY += stepY;
    }
    return 1;
}

// Wibbly Wobbly Math Wimey Stuff
static stock GPS_Internal_Stream(playerid) {
    if(!g_GPSData[playerid][gps_IsActive]) return;

    new Path:pathid = g_GPSData[playerid][gps_CurrentPath];
    new totalNodes = g_GPSData[playerid][gps_PathNodeCount];
    new currentNode = g_GPSData[playerid][gps_LastLoadedNode];

    if(currentNode >= totalNodes - 1) return;

    // Head is just behind Tail
    new nextHead = (g_GPSData[playerid][gps_ZoneHead] + 1) % MAX_GPS_ZONES;
    if(nextHead == g_GPSData[playerid][gps_ZoneTail]) return; 

    new MapNode:nodeA, MapNode:nodeB;
    new Float:xA, Float:yA, Float:zA;
    new Float:xB, Float:yB, Float:zB;
    
    for(new i = currentNode; i < totalNodes - 1; i++) {
        
        GetPathNode(pathid, i, nodeA);
        GetPathNode(pathid, i + 1, nodeB);

        GetMapNodePos(nodeA, xA, yA, zA);
        GetMapNodePos(nodeB, xB, yB, zB);

        if(!GPS_Internal_Rasterize(playerid, xA, yA, xB, yB)) {
            break; 
        }

        g_GPSData[playerid][gps_LastLoadedNode] = i + 1;
    }
}

forward GPS_OnCalculated(Path:pathid, playerid);
public GPS_OnCalculated(Path:pathid, playerid) {
    if(!IsPlayerConnected(playerid)) {
        if(IsValidPath(pathid)) DestroyPath(pathid);
        return 0;
    }
    
    if(!IsValidPath(pathid)) {
        print("Route calculation failed (No path found).");
        return 0;
    }

    g_GPSData[playerid][gps_IsActive] = true;
    g_GPSData[playerid][gps_CurrentPath] = pathid;
    
    new size;
    GetPathSize(pathid, size);
    g_GPSData[playerid][gps_PathNodeCount] = size;
    g_GPSData[playerid][gps_LastLoadedNode] = 0;
    
    g_GPSData[playerid][gps_ZoneHead] = 0;
    g_GPSData[playerid][gps_ZoneTail] = 0;
    g_GPSData[playerid][gps_SeqCounter] = 0;

    GPS_Internal_Stream(playerid);

    if(g_GPSData[playerid][gps_Timer]) KillTimer(g_GPSData[playerid][gps_Timer]);
    g_GPSData[playerid][gps_Timer] = SetTimerEx("GPS_Internal_Update", GPS_UPDATE_INTERVAL, true, "i", playerid);
    return 1;
}

forward GPS_Internal_Update(playerid);
public GPS_Internal_Update(playerid) {
    if(!g_GPSData[playerid][gps_IsActive]) return;

    new Float:pX, Float:pY, Float:pZ;
    GetPlayerPos(playerid, pX, pY, pZ);

    if(g_GPSData[playerid][gps_LastLoadedNode] >= g_GPSData[playerid][gps_PathNodeCount]-1) {
         if(VectorSize(pX - g_GPSData[playerid][gps_DestX], pY - g_GPSData[playerid][gps_DestY], 0.0) < 25.0) {
            GPS_Stop(playerid);
            CallLocalFunction("OnPlayerGPSArrival", "i", playerid);
            return;
         }
    }

    new Float:minDistSq = 999999.0;
    new closestSequenceID = -1; 
    
    new head = g_GPSData[playerid][gps_ZoneHead];
    new tail = g_GPSData[playerid][gps_ZoneTail];
    new i = tail;

    while(i != head) {
        if(g_GPSData[playerid][gps_ZoneIDs][i] != -1) {
            new Float:dx = pX - g_GPSData[playerid][gps_ZoneX][i];
            new Float:dy = pY - g_GPSData[playerid][gps_ZoneY][i];
            
            // Quick AABB check. Not needed for just distance sq.
            new Float:dSq = (dx * dx) + (dy * dy);

            if(dSq < minDistSq) {
                minDistSq = dSq;
                closestSequenceID = g_GPSData[playerid][gps_ZoneSeq][i];
            }
        }
        i = (i + 1) % MAX_GPS_ZONES;
    }

    if(minDistSq > (GPS_MAX_DEVIATION * GPS_MAX_DEVIATION)) {
        if(GetTickCount() - g_GPSData[playerid][gps_LastRecalc] > GPS_RECALC_COOLDOWN) {
            if(VectorSize(pX - g_GPSData[playerid][gps_DestX], pY - g_GPSData[playerid][gps_DestY], 0.0) > 50.0) {
                g_GPSData[playerid][gps_LastRecalc] = GetTickCount();
                GPS_Start(playerid, g_GPSData[playerid][gps_DestX], g_GPSData[playerid][gps_DestY], g_GPSData[playerid][gps_DestZ]);
                return;
            }
        }
    }

    // Time to clean up, PacMan
    if(closestSequenceID != -1 && minDistSq < (30.0 * 30.0)) {
        new cutoffSequence = closestSequenceID - 3;

        // While the Tail is old, delete it and move Tail forward.
        while(g_GPSData[playerid][gps_ZoneIDs][g_GPSData[playerid][gps_ZoneTail]] != -1) {
            new currentTail = g_GPSData[playerid][gps_ZoneTail];
            
            if(g_GPSData[playerid][gps_ZoneSeq][currentTail] >= cutoffSequence) break;

            PlayerGangZoneDestroy(playerid, g_GPSData[playerid][gps_ZoneIDs][currentTail]);
            g_GPSData[playerid][gps_ZoneIDs][currentTail] = -1;
            
            g_GPSData[playerid][gps_ZoneTail] = (currentTail + 1) % MAX_GPS_ZONES;
            
            if(g_GPSData[playerid][gps_ZoneTail] == g_GPSData[playerid][gps_ZoneHead]) break;
        }
    }

    GPS_Internal_Stream(playerid);
}

public OnPlayerConnect(playerid)
{
    GPS_Internal_ResetVars(playerid);

    #if defined GPS_OnPlayerConnect
        return GPS_OnPlayerConnect(playerid);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerConnect
    #undef OnPlayerConnect
#else
    #define _ALS_OnPlayerConnect
#endif
#define OnPlayerConnect GPS_OnPlayerConnect
#if defined GPS_OnPlayerConnect
    forward GPS_OnPlayerConnect(playerid);
#endif

public OnPlayerDisconnect(playerid, reason)
{
    GPS_Stop(playerid);

    #if defined GPS_OnPlayerDisconnect
        return GPS_OnPlayerDisconnect(playerid, reason);
    #else
        return 1;
    #endif
}
#if defined _ALS_OnPlayerDisconnect
    #undef OnPlayerDisconnect
#else
    #define _ALS_OnPlayerDisconnect
#endif
#define OnPlayerDisconnect GPS_OnPlayerDisconnect
#if defined GPS_OnPlayerDisconnect
    forward GPS_OnPlayerDisconnect(playerid, reason);
#endif
