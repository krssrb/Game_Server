#include"global.h"

extern dynObjectType_t dynObjectType_waypoint;

// todo1: Waypoints can also be controlled by Bane, find the animation for that and do proper handling
// todo2: Waypoint discovery not supported yet, all waypoints are always available
// todo3: There is bug, even though there is code to avoid sending Recv_EnteredWaypoint after using another waypoint, it is still sent sometimes.

typedef struct  
{
	// used to keep info about players that already received the EnteredWaypoint message
	uint64 entityId; // entityID of player
	uint32 lastUpdate; // used to detect if the player left the waypoint
	bool teleportedAway; // used so we dont send client Recv_ExitedWaypoint
}waypointTriggerCheck_t;

typedef struct  
{
	uint32 waypointID; // id unique for all maps (from teleporter table)
	//uint64 entityId;
	uint32 nameId; // index for waypointlanguage lookup
	//uint32 contextId; // index for waypointlanguage lookup
	uint8 state;
	std::vector<waypointTriggerCheck_t> triggeredPlayers;
	uint32 updateCounter; // used to detect if players leave the waypoint
	// note: We cannot link players->waypoints in the DB by using entityIds because they are dynamically created, so we use the plain DB row IDs instead
}waypoint_t;

dynObject_t* waypoint_create(mapChannel_t *mapChannel, float x, float y, float z, float orientation, uint32 waypointID, uint32 nameId, uint32 contextId)
{
	dynObject_t *dynObject = _dynamicObject_create(25651, &dynObjectType_waypoint);
	if( !dynObject )
		return NULL;
	dynObject->stateId = 0;
	dynamicObject_setPosition(dynObject, x, y, z);
	// setup waypoint specific data
	waypoint_t *objData = (waypoint_t*)malloc(sizeof(waypoint_t));
	if (!objData)
		return 0;
	memset(objData, 0x00, sizeof(waypoint_t));
	new(objData) waypoint_t();
	objData->waypointID = waypointID;
	objData->nameId = nameId;
	dynObject->objectData = objData;
	// randomize rotation
	float randomRotY = orientation;
	float randomRotX = 0.0f;
	float randomRotZ = 0.0f;
	dynamicObject_setRotation(dynObject, randomRotY, randomRotX, randomRotZ);
	cellMgr_addToWorld(mapChannel, dynObject);
	// init periodic timer
	dynamicObject_setPeriodicUpdate(mapChannel, dynObject, 0, 400); // call about twice a second
	// add waypoint to map
	mapChannel->waypoints.push_back(dynObject);
	// return object
	return dynObject;
}

void waypoint_destroy(mapChannel_t *mapChannel, dynObject_t *dynObject)
{
	// called shortly before free()
	// todo: remove waypoint from global waypoint list
	printf("waypoint_destroy not implemented\n");
}

void waypoint_appearForPlayers(mapChannel_t *mapChannel, dynObject_t *dynObject, mapChannelClient_t **playerList, sint32 playerCount)
{
	pyMarshalString_t pms;
	pym_init(&pms);
	pym_tuple_begin(&pms);
	pym_addInt(&pms, 56);
	pym_addInt(&pms, 100); // windupTime should not be zero to avoid freezing animations?
	pym_tuple_end(&pms);
	netMgr_pythonAddMethodCallRaw(playerList, playerCount, dynObject->entityId, ForceState, pym_getData(&pms), pym_getLen(&pms));
}

void waypoint_disappearForPlayers(mapChannel_t *mapChannel, dynObject_t *dynObject, mapChannelClient_t **playerList, sint32 playerCount)
{
	// called before the object is removed from player sight
}

void waypoint_playerInAreaOfEffect(mapChannel_t *mapChannel, dynObject_t *dynObject, mapChannelClient_t* client)
{
	waypoint_t* objData = (waypoint_t*)dynObject->objectData;
	// objData->updateCounter
	std::vector<waypointTriggerCheck_t>::iterator itr = objData->triggeredPlayers.begin();
	while (itr != objData->triggeredPlayers.end())
	{
		if (itr->entityId == client->clientEntityId)
		{
			itr->lastUpdate = objData->updateCounter; // update counter
			return;
		}
		++itr;
	}
	// player not found, create new entry
	waypointTriggerCheck_t newTriggerCheck = {0};
	newTriggerCheck.entityId = client->clientEntityId;
	newTriggerCheck.lastUpdate = objData->updateCounter;
	objData->triggeredPlayers.push_back(newTriggerCheck);
	// send waypoint list (Recv_EnteredWaypoint)
	pyMarshalString_t pms;
	pym_init(&pms);
	pym_tuple_begin(&pms);
	pym_addInt(&pms, mapChannel->mapInfo->contextId); // currentMapId
	pym_addInt(&pms, mapChannel->mapInfo->contextId); // gameContextId (current)
	pym_list_begin(&pms); // mapWaypointInfoList
	pym_tuple_begin(&pms);
	pym_addInt(&pms, mapChannel->mapInfo->contextId); // gameContextId for waypoint
	pym_list_begin(&pms); // mapInstanceList
	pym_list_end(&pms);
	pym_list_begin(&pms); // waypoints
	//pym_tuple_begin(&pms); // waypoint 1
	//pym_addInt(&pms, 1); // waypointID
	//pym_tuple_begin(&pms); // pos
	//pym_addInt(&pms, 1);
	//pym_addInt(&pms, 1); 
	//pym_addInt(&pms, 1); 
	//pym_tuple_end(&pms);
	//pym_addInt(&pms, 0); // contested
	//pym_tuple_end(&pms);
	//pym_tuple_begin(&pms); // waypoint 2
	//pym_addInt(&pms, 2); // waypointID
	//pym_tuple_begin(&pms); // pos
	//pym_addInt(&pms, 40);
	//pym_addInt(&pms, 40);
	//pym_addInt(&pms, 40); 
	//pym_tuple_end(&pms);
	//pym_addInt(&pms, 0); // contested
	//pym_tuple_end(&pms);

	// parse through all waypoints and append data
	std::vector<dynObject_t*>::iterator wpItr = mapChannel->waypoints.begin();
	while (wpItr != mapChannel->waypoints.end())
	{
		dynObject_t* waypointObject = *wpItr;
		waypoint_t* waypointObjectData = (waypoint_t*)waypointObject->objectData;
		pym_tuple_begin(&pms); // waypoint
		pym_addInt(&pms, waypointObjectData->nameId); // waypointID (not to be confused with our DB waypointID)
		pym_tuple_begin(&pms); // pos
		pym_addInt(&pms, (sint32)waypointObject->x);
		pym_addInt(&pms, (sint32)waypointObject->y); 
		pym_addInt(&pms, (sint32)waypointObject->z); 
		pym_tuple_end(&pms);
		pym_addInt(&pms, 0); // contested 
		pym_tuple_end(&pms);
		++wpItr;
	}

	pym_list_end(&pms);
	pym_tuple_end(&pms);
	pym_list_end(&pms);
	pym_addNoneStruct(&pms); // tempWormholes
	pym_addInt(&pms, 2); // waypointTypeId (2 --> MAPWAYPOINT)
	pym_addInt(&pms, objData->waypointID); // currentWaypointId
	pym_tuple_end(&pms);
	netMgr_pythonAddMethodCallRaw(&client, 1, 5, EnteredWaypoint, pym_getData(&pms), pym_getLen(&pms));
}


bool waypoint_periodicCallback(mapChannel_t *mapChannel, dynObject_t *dynObject, uint8 timerID, sint32 timePassed)
{
	waypoint_t* objData = (waypoint_t*)dynObject->objectData;
	objData->updateCounter++;
	// check for players in range
	// calculate rect of affected cells
	sint32 minX = (sint32)(((dynObject->x-8.0f) / CELL_SIZE) + CELL_BIAS);
	sint32 minZ = (sint32)(((dynObject->z-8.0f) / CELL_SIZE) + CELL_BIAS);
	sint32 maxX = (sint32)(((dynObject->x+8.0f+(CELL_SIZE-0.0001f)) / CELL_SIZE) + CELL_BIAS);
	sint32 maxZ = (sint32)(((dynObject->z+8.0f+(CELL_SIZE-0.0001f)) / CELL_SIZE) + CELL_BIAS);
	// check all cells for players
	for(sint32 ix=minX; ix<=maxX; ix++)
	{
		for(sint32 iz=minZ; iz<=maxZ; iz++)
		{
			mapCell_t *nMapCell = cellMgr_getCell(mapChannel, ix, iz);
			if( nMapCell )
			{
				if( nMapCell->ht_playerList.empty() )
					continue;
				std::vector<mapChannelClient_t*>::iterator itr = nMapCell->ht_playerList.begin();
				while (itr != nMapCell->ht_playerList.end())
				{
					mapChannelClient_t* player = itr[0];
					++itr;
					// check distance to waypoint along xz plane
					float dX = dynObject->x - player->player->actor->posX;
					float dZ = dynObject->z - player->player->actor->posZ;
					float distance = dX*dX+dZ*dZ; // pythagoras but we optimized the sqrt() away
					if( distance >= (2.2f*2.2f) )
						continue;
					// check Y distance (rough)
					float dY = dynObject->y - player->player->actor->posY;
					if( dY < 0.0f ) dY = -dY;
					if( dY >= 10.0f )
						continue;
					waypoint_playerInAreaOfEffect(mapChannel, dynObject, player);
				}			
			}
		}
	}
	// check for not updated triggerCheck entries
	std::vector<waypointTriggerCheck_t>::iterator itr = objData->triggeredPlayers.begin();
	while (itr != objData->triggeredPlayers.end())
	{
		if (itr->lastUpdate != objData->updateCounter)
		{
			// send exit-waypoint packet
			mapChannelClient_t* client = (mapChannelClient_t*)entityMgr_get(itr->entityId);
			if( client && itr->teleportedAway == false )
			{
				// send ExitedWaypoint
				pyMarshalString_t pms;
				pym_init(&pms);
				pym_tuple_begin(&pms);
				pym_tuple_end(&pms);
				netMgr_pythonAddMethodCallRaw(&client, 1, 5, ExitedWaypoint, pym_getData(&pms), pym_getLen(&pms));
			}
			// player gone, remove entry
			itr = objData->triggeredPlayers.erase(itr);
			continue;
		}
		++itr;
	}
	return true;
}

void waypoint_useObject(mapChannel_t *mapChannel, dynObject_t *dynObject, mapChannelClient_t* client, sint32 actionID, sint32 actionArg)
{
	// not used
}

void waypoint_interruptUse(mapChannel_t *mapChannel, dynObject_t *dynObject, mapChannelClient_t* client, sint32 actionID, sint32 actionArg)
{
	// not used
}


void waypoint_recv_SelectWaypoint(mapChannelClient_t *client, uint8 *pyString, sint32 pyStringLen)
{
	pyUnmarshalString_t pums;
	pym_init(&pums, pyString, pyStringLen);
	if( !pym_unpackTuple_begin(&pums) )
		return;
	sint32 mapInstanceId;
	if( pym_unpack_isNoneStruct(&pums) )
		mapInstanceId = pym_unpackNoneStruct(&pums); // should not happen, seems to be a bug due to wrong Waypoint data we send
	else
		mapInstanceId = pym_unpackInt(&pums);
	sint32 waypointId = pym_unpackInt(&pums);

	// todo1: Check if player is actually standing on a waypoint
	// todo2: Check if the player has discovered the waypoint

	// find the waypoint
	mapChannel_t* mapChannel = client->mapChannel;
	dynObject_t* waypointObject = NULL;
	std::vector<dynObject_t*>::iterator itr = mapChannel->waypoints.begin();
	while (itr != mapChannel->waypoints.end())
	{
		dynObject_t* dynObject = *itr;
		waypoint_t* dynObjectData = (waypoint_t*)dynObject->objectData; // dont need to check type
		if (dynObjectData->nameId == waypointId)
		{
			waypointObject = dynObject;
			break;
		}
		++itr;
	}	
	if( waypointObject == NULL )
	{
		printf("Recv_SelectWaypoint: Waypoint not found, cannot teleport player.\n");
		return;
	}
	// mark player as teleportedAway to avoid sending ExitedWaypoint
	// todo3: add the player to the destination waypoint triggerPlayer list (so we don't send EnteredWaypoint again)
	waypoint_t* waypointObjectData = (waypoint_t*)waypointObject->objectData; 
	waypointTriggerCheck_t newTriggerCheck = {0};
	newTriggerCheck.entityId = client->clientEntityId;
	newTriggerCheck.lastUpdate = waypointObjectData->updateCounter;
	waypointObjectData->triggeredPlayers.push_back(newTriggerCheck);
	// teleport the player (on the same map)
	// the packet is only sent to the teleporting player
	// this could cause problems when the destination is near enough
	// and the player does not leave the sight range of other players
	netCompressedMovement_t netMovement = {0};
	client->player->actor->posX = waypointObject->x;
	client->player->actor->posY = waypointObject->y+0.5f;
	client->player->actor->posZ = waypointObject->z;
	netMovement.entityId = client->player->actor->entityId;
	netMovement.posX24b = client->player->actor->posX * 256.0f;
	netMovement.posY24b = client->player->actor->posY * 256.0f;
	netMovement.posZ24b = client->player->actor->posZ * 256.0f;
	netMgr_sendEntityMovement(client->cgm, &netMovement);
}

dynObjectType_t dynObjectType_waypoint = 
{
	waypoint_destroy, // destroy
	waypoint_appearForPlayers, // appearForPlayers
	waypoint_disappearForPlayers, // disappearForPlayer
	waypoint_periodicCallback, // periodicCallback
	waypoint_useObject, // useObject
	waypoint_interruptUse
};