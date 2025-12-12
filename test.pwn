#include <open.mp>

// Settings
#define GPS_LINE_WIDTH 25.0

#include <gps-nav>
#include <izcmd>
#include <sscanf2>

main() {
    print("\n----------------------------------");
    print(" GPS Navigation System - Test Script");
    print("----------------------------------\n");
}

public OnGameModeInit()
{
    AddPlayerClass(0, 1481.0 + 50.0, -1771.0 + 50.0, 13.5, 0.0);
    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    SetPlayerPos(playerid, 1481.0, -1771.0, 13.5);
    SetPlayerCameraPos(playerid, 1481.0, -1771.0, 50.0);
    SetPlayerCameraLookAt(playerid, 1481.0, -1771.0, 13.5);
    return 1;
}

public OnPlayerSpawn(playerid)
{
    SendClientMessage(playerid, -1, "{FFFF00}GPS TEST:{FFFFFF} Type /v to get a car.");
    SendClientMessage(playerid, -1, "{FFFF00}GPS TEST:{FFFFFF} Right-click anywhere on the map (Esc) to start GPS.");
    return 1;
}

public OnPlayerClickMap(playerid, Float:fX, Float:fY, Float:fZ)
{
    new ret = GPS_Start(playerid, fX, fY, fZ);

    if(ret) {
        SendClientMessage(playerid, 0x00FF00FF, "GPS: Calculating route to marker...");
    } else {
        SendClientMessage(playerid, 0xFF0000FF, "GPS: Could not find a path to that location.");
    }
    return 1;
}

public OnPlayerGPSActivate(playerid, Float:X, Float:Y, Float:Z) {
    SetPlayerMapIcon(playerid, 12, X, Y, Z, 41, 0, MAPICON_GLOBAL);
    return 1;
}

public OnPlayerGPSArrival(playerid)
{
    GameTextForPlayer(playerid, "~g~DESTINATION REACHED!", 5000, 3);
    PlayerPlaySound(playerid, 1185, 0.0, 0.0, 0.0); // Success sound
    SendClientMessage(playerid, 0x00FF00FF, "GPS: You have arrived at your destination.");
    RemovePlayerMapIcon(playerid, 12);
    return 1;
}

CMD:stopgps(playerid, params[])
{
    if(GPS_IsActive(playerid)) {
        GPS_Stop(playerid);
        SendClientMessage(playerid, -1, "GPS: Navigation stopped.");
        RemovePlayerMapIcon(playerid, 12);
    } else {
        SendClientMessage(playerid, -1, "GPS: No active navigation.");
    }
    return 1;
}

CMD:v(playerid, params[])
{
    new Float:x, Float:y, Float:z, Float:a;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, a);
    
    new veh = CreateVehicle(411, x, y, z, a, 1, 1, -1); // Infernus
    PutPlayerInVehicle(playerid, veh, 0);
    SendClientMessage(playerid, -1, "Vehicle spawned!");
    return 1;
}

CMD:longroute(playerid, params[])
{
    GPS_Start(playerid, 1699.0, 1435.0, 10.0); 
    SendClientMessage(playerid, -1, "GPS: Calculating long distance route to LV Airport...");
    return 1;
}

CMD:gps(playerid, params[]) {
    new Float:X, Float:Y, Float:Z;

    if(sscanf(params, "p<,>fff", X, Y, Z)) return SendClientMessage(playerid, -1, "/gps [X] [Y] [Z]");

    GPS_Start(playerid, X, Y, Z);
    return 1;
}