#include "Global.h"

/*
	A MapChannel in general is a isolated thread of the server that is 100% responsible for one or multiple maps
	Whenever a client selects a character the clientinstance is passed to the MapChannel and is no more processed by the main thread.
	If a client changes map his instance is passed to the target MapChannel
*/

hashTable_t ht_mapChannelsByContextId;
mapChannelList_t *global_channelList; //20110827 @dennton

void mapteleporter_teleportEntity(sint32 destX,sint32 destY, sint32 destZ, sint32 mapContextId, mapChannelClient_t *player)
{
    destY += 700;
	printf("teleport to: x y z map - %d %d %d %d \n",destX,destY,destZ,mapContextId);

	//remove entity from old map - remove client from all channels
			communicator_playerExitMap(player);
			//unregister player
			//communicator_unregisterPlayer(cm);
			//remove visible entity
			Thread::LockMutex(&player->cgm->cs_general);
			cellMgr_removeFromWorld(player);
			// remove from list
			for(sint32 i=0; i<player->mapChannel->playerCount; i++)
			{
				if( player == player->mapChannel->playerList[i] )
				{
					if( i == player->mapChannel->playerCount-1 )
					{
						player->mapChannel->playerCount--;
					}
					else
					{
						player->mapChannel->playerList[i] = player->mapChannel->playerList[player->mapChannel->playerCount-1];
						player->mapChannel->playerCount--;
					}
					break;
				}
			}
			Thread::UnlockMutex(&player->cgm->cs_general);
				
			//############## map loading stuff ##############
			// send PreWonkavate (clientMethod.134)
			pyMarshalString_t pms;
			pym_init(&pms);
			pym_tuple_begin(&pms);
			pym_addInt(&pms, 0); // wonkType - actually not used by the game
			pym_tuple_end(&pms);
			netMgr_pythonAddMethodCallRaw(player->cgm, 5, 134, pym_getData(&pms), pym_getLen(&pms));
			// send Wonkavate (inputstateRouter.242)
			player->cgm->mapLoadContextId = mapContextId;
			pym_init(&pms);
			pym_tuple_begin(&pms);
			pym_addInt(&pms, mapContextId);	// gameContextId (alias mapId)
			pym_addInt(&pms, 0);	// instanceId ( not important for now )
			// find map version
			sint32 mapVersion = 0; // default = 0;
			for(sint32 i=0; i<mapInfoCount; i++)
			{
				if( mapInfoArray[i].contextId == mapContextId )
				{
					mapVersion = mapInfoArray[i].version;
					break;
				}
			}
			pym_addInt(&pms, mapVersion);	// templateVersion ( from the map file? )
			pym_tuple_begin(&pms);  // startPosition
			pym_addFloat(&pms, destX); // x (todo: send as float)
			pym_addFloat(&pms, destY); // y (todo: send as float)
			pym_addFloat(&pms, destZ); // z (todo: send as float)
			pym_tuple_end(&pms);
			pym_addInt(&pms, 0);	// startRotation (todo, read from db and send as float)
			pym_tuple_end(&pms);
			netMgr_pythonAddMethodCallRaw(player->cgm, 6, Wonkavate, pym_getData(&pms), pym_getLen(&pms));
			
			//################## player assigning ###############
			communicator_loginOk(player->mapChannel, player);
			communicator_playerEnterMap(player);
			//add entity to new map
			player->player->actor->posX = destX; 
			player->player->actor->posY = destY;
			player->player->actor->posZ = destZ;
			player->player->actor->contextId = mapContextId;
			//cm->mapChannel->mapInfo->contextId = telepos.mapContextId;
		  
			
			player->player->controllerUser->inventory = player->inventory;
			player->player->controllerUser->mission = player->mission;
			player->tempCharacterData = player->player->controllerUser->tempCharacterData;

				
			//---search new mapchannel
			for(sint32 chan=0; chan < global_channelList->mapChannelCount; chan++)
			{
               mapChannel_t *mapChannel = global_channelList->mapChannelArray+chan;
			   if(mapChannel->mapInfo->contextId == mapContextId)
			   {
				   player->mapChannel = mapChannel;
				   break;
			   }
			}

			mapChannel_t *mapChannel = player->mapChannel;
			Thread::LockMutex(&player->mapChannel->criticalSection);
			mapChannel->playerList[mapChannel->playerCount] = player;
			mapChannel->playerCount++;
			hashTable_set(&mapChannel->ht_socketToClient, (uint32)player->cgm->socket, player);
			Thread::UnlockMutex(&mapChannel->criticalSection);
			
			player->player->actor->posX = destX; 
			player->player->actor->posY = destY;
			player->player->actor->posZ = destZ;
			player->player->actor->contextId = mapContextId;
			cellMgr_addToWorld(player); //cellsint32roducing to player /from players
			// setCurrentContextId (clientMethod.362)
			pym_init(&pms);
			pym_tuple_begin(&pms);
			pym_addInt(&pms, player->mapChannel->mapInfo->contextId);
			pym_tuple_end(&pms);
			netMgr_pythonAddMethodCallRaw(player->cgm, 5, 362, pym_getData(&pms), pym_getLen(&pms));

}

void mapteleporter_checkForEntityInRange(mapChannel_t *mapChannel)
{
    return; // disabled until someone fixes this and gets rid of all the memory leaks (when using 'new', also use 'delete')
	//pyMarshalString_t pms;

	sint32 tCount =0;
	float minimumRange = 1.8f;
	float difX = 0.0f;
	float difY = 0.0f;
	float difZ = 0.0f;
	float dist = 0.0f;
	minimumRange *= minimumRange;
	//test zoneteleporters map. should be builded from db
	sint32 **porting_locs = new sint32*[4];
	// values 0-9: source-contextid, source xyz ,dest xyz , dest-contextid, cell-x, cell-z
	porting_locs[0] = new sint32 [10]; // zone teleporter #1: wilderness -> divide
	porting_locs[0][0] = 1220;
	porting_locs[0][1] = 300;
	porting_locs[0][2] = 142;
	porting_locs[0][3] = -580;
	porting_locs[0][4] = -965;
	porting_locs[0][5] = 176;
	porting_locs[0][6] = 634;
	porting_locs[0][7] = 1148;
	porting_locs[0][8] = (uint32)((porting_locs[0][1] / CELL_SIZE) + CELL_BIAS);
	porting_locs[0][9] = (uint32)((porting_locs[0][3] / CELL_SIZE) + CELL_BIAS);
	porting_locs[1] = new sint32 [10]; // zone teleporter #1: divide -> wilderness
	porting_locs[1][0] = 1148;
	porting_locs[1][1] = -1008;
	porting_locs[1][2] = 180;
	porting_locs[1][3] = 671;
	porting_locs[1][4] = 280;
	porting_locs[1][5] = 152;
	porting_locs[1][6] = -538;
	porting_locs[1][7] = 1220;
	porting_locs[1][8] = (uint32)((porting_locs[1][1] / CELL_SIZE) + CELL_BIAS);
	porting_locs[1][9] = (uint32)((porting_locs[1][3] / CELL_SIZE) + CELL_BIAS);
	porting_locs[2] = new sint32 [10]; //zone zeleporter #2: wilderness -> divide
	porting_locs[2][0] = 1220;
	porting_locs[2][1] = 891;
	porting_locs[2][2] = 268;
	porting_locs[2][3] = 32;
	porting_locs[2][4] = 436;
	porting_locs[2][5] = 173;
	porting_locs[2][6] = 1193;
	porting_locs[2][7] = 1148;
	porting_locs[2][8] = (uint32)((porting_locs[2][1] / CELL_SIZE) + CELL_BIAS);
	porting_locs[2][9] = (uint32)((porting_locs[2][3] / CELL_SIZE) + CELL_BIAS);
	porting_locs[3] = new sint32 [10]; //zone teleporter #2: divide -> wilderness
	porting_locs[3][0] = 1148;
	porting_locs[3][1] = 499;
	porting_locs[3][2] = 184;
	porting_locs[3][3] = 1202;
	porting_locs[3][4] = 905;
	porting_locs[3][5] = 273;
	porting_locs[3][6] = 65;
	porting_locs[3][7] = 1220;
	porting_locs[3][8] = (uint32)((porting_locs[3][1] / CELL_SIZE) + CELL_BIAS);
	porting_locs[3][9] = (uint32)((porting_locs[3][3] / CELL_SIZE) + CELL_BIAS);
 
	//---search through the whole teleporter list
	for (sint32 x =0; x < 4; x++)
	{
		
	    float mPosX = porting_locs[x][1]; //teleporter x-pos 
		float mPosZ = porting_locs[x][3]; //		   z-pos
		//############ get teleporter mapcell ###################################
		mapCell_t *mapCell = cellMgr_tryGetCell(mapChannel, 
			                                    porting_locs[x][8], 
												porting_locs[x][9]);
		if(mapCell == NULL) continue;
		//############ get all players in current celllocation ###################
		mapChannelClient_t **playerList = NULL;
		tCount = mapCell->ht_playerNotifyList.size();
		playerList = &mapCell->ht_playerNotifyList[0];

		// check players in range
		for(sint32 i=0; i<tCount; i++)
		{
			if( playerList == NULL) break; //no player found
			mapChannelClient_t *player = playerList[i];
			if(player->player->actor->stats.healthCurrent<=0) break;
			difX = (sint32)(player->player->actor->posX) - mPosX;
			difZ = (sint32)(player->player->actor->posZ) - mPosZ;
			dist = difX*difX + difZ*difZ;
			//player(s) in range: do teleporting
			if( (dist <= minimumRange) &&   (porting_locs[x][0] == player->mapChannel->mapInfo->contextId))
			{
			    
				 mapteleporter_teleportEntity( porting_locs[x][4],
											   porting_locs[x][5],
											   porting_locs[x][6],
											   porting_locs[x][7],
											   player);
					
			}
		}//---for: playercount
	}//---for: all teleporter locations       				
}

void _cb_mapChannel_addNewPlayer(void *param, diJob_characterData_t *jobData)
{
	if( jobData->outCharacterData == NULL )
	{
		// todo: add error handling
		return;
	}
	mapChannelClient_t *mc = (mapChannelClient_t*)param;
	mapChannel_t *mapChannel = mc->mapChannel;
	// save character data
	mc->tempCharacterData = (di_characterData_t*)malloc(sizeof(di_characterData_t));
	memcpy(mc->tempCharacterData, jobData->outCharacterData, sizeof(di_characterData_t));
	// save seperate mission data (if any)
	if( mc->tempCharacterData->missionStateCount )
	{
		mc->tempCharacterData->missionStateData = (di_CharacterMissionData*)malloc(sizeof(di_CharacterMissionData) * mc->tempCharacterData->missionStateCount);
		memcpy(mc->tempCharacterData->missionStateData, jobData->outCharacterData->missionStateData, sizeof(di_CharacterMissionData) * mc->tempCharacterData->missionStateCount);
	}
	else
	{
		mc->tempCharacterData->missionStateData = NULL;
	}
	// add to player to mapChannel (synced)
	Thread::LockMutex(&mapChannel->criticalSection);
	mapChannel->playerList[mapChannel->playerCount] = mc;
	mapChannel->playerCount++;
	hashTable_set(&mapChannel->ht_socketToClient, (uint32)mc->cgm->socket, mc);
	Thread::UnlockMutex(&mapChannel->criticalSection);
}

void mapChannel_addNewPlayer(mapChannel_t *mapChannel, clientGamemain_t *cgm)
{
	mapChannelClient_t *mc = (mapChannelClient_t*)malloc(sizeof(mapChannelClient_t));
	memset((void*)mc, 0x00, sizeof(mapChannelClient_t));
	mc->cgm = cgm;
	mc->clientEntityId = entityMgr_getFreeEntityIdForClient(); // generate a entityId for the client instance
	mc->mapChannel = mapChannel;
	mc->player = NULL;
	DataInterface_Character_getCharacterData(cgm->userID, cgm->mapLoadSlotId, _cb_mapChannel_addNewPlayer, mc);
	// register mapChannelClient
	entityMgr_registerEntity(mc->clientEntityId, mc);

	//// add to the serverlist
	//if( mapChannel->playerCount == mapChannel->playerLimit )
	//{
	//	printf("TODO#addNewPlayer\n");
	//	return;
	//}
	//mapChannel->playerList[mapChannel->playerCount] = mc;
	//mapChannel->playerCount++;
	//hashTable_set(&mapChannel->ht_socketToClient, (uint32)cgm->socket, mc);
	//// create new actor...
	//
	//// void DataInterface_Character_getCharacterData(unsigned long long userID, uint32 slotIndex, void (*cb)(void *param, diJob_characterData_t *jobData), void *param)
}

void mapChannel_removePlayer(mapChannelClient_t *client)
{
	// unregister mapChannelClient
	entityMgr_unregisterEntity(client->clientEntityId);

	communicator_unregisterPlayer(client);
	Thread::LockMutex(&client->cgm->cs_general);
	cellMgr_removeFromWorld(client);
	manifestation_removePlayerCharacter(client->mapChannel, client);
	if( client->disconnected == false )
		GameMain_PassClientToCharacterSelection(client->cgm);
	// remove from list
	for(sint32 i=0; i<client->mapChannel->playerCount; i++)
	{
		if( client == client->mapChannel->playerList[i] )
		{
			if( i == client->mapChannel->playerCount-1 )
			{
				client->mapChannel->playerCount--;
			}
			else
			{
				client->mapChannel->playerList[i] = client->mapChannel->playerList[client->mapChannel->playerCount-1];
				client->mapChannel->playerCount--;
			}
			break;
		}
	}
	// delete data
	Thread::UnlockMutex(&client->cgm->cs_general);
	free(client->cgm);
	free(client);
}

void mapChannel_registerTimer(mapChannel_t *mapChannel, sint32 period, void *param, bool (*cb)(mapChannel_t *mapChannel, void *param, sint32 timePassed))
{
	mapChannelTimer_t *timer = (mapChannelTimer_t*)malloc(sizeof(mapChannelTimer_t));
	timer->period = period;
	timer->timeLeft = period;
	timer->param = param;
	timer->cb = cb;
	mapChannel->timerList.push_back(timer);
}

//void mapChannel_launchMissileForWeapon(mapChannelClient_t* client, item_t* weapon)
//{
//	sint32 damageRange = weapon->itemTemplate->weapon.maxDamage-weapon->itemTemplate->weapon.minDamage;
//	damageRange = max(damageRange, 1); // to avoid division by zero in the next line
//	
//	if( weapon->itemTemplate->weapon.altActionId == 1 )
//	{
//		// weapon range attack
//		if( weapon->itemTemplate->weapon.altActionArg == 133 )
//		{
//			// normal pistol shot (physical)
//			missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, weapon->itemTemplate->weapon.minDamage+(rand()%damageRange), weapon->itemTemplate->weapon.altActionId, weapon->itemTemplate->weapon.altActionArg); 
//		}
//		else if( weapon->itemTemplate->weapon.altActionArg == 67 )
//		{
//			// laser pistol shot
//			missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, weapon->itemTemplate->weapon.minDamage+(rand()%damageRange), weapon->itemTemplate->weapon.altActionId, weapon->itemTemplate->weapon.altActionArg); 
//		}
//
//	}
//	
//		
//	//switch(weapon->itemTemplate->weapon.toolType)
//	//{
//	//case 9:
//	//	if(weapon->itemTemplate->item.classId == 29395)
//	//		missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 120, 1, 287); 
//	//	else
//	//		missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 40, 1, 121); 
//	//	break;
//	//case 10:
//	//	missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 90, 1, 6); 
//	//	break;
//	//case 8:
//	//	missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 15, 1, 133); 
//	//	break;
//	//case 7:
//	//	missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 25, 1, 134); 
//	//	break;
//	//case 15:
//	//	if(weapon->itemTemplate->item.classId == 29757)
//	//		missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 20, 149, 7); 
//	//	else
//	//		missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 20, 1, 133);//149, 1); 
//	//	break;
//	//case 22:
//	//	missile_launch(client->mapChannel, client->player->actor, client->player->targetEntityId, 20, 1, 3); 
//	//	break;
//	//default:
//	//	printf("mapChannel_launchMissileForWeapon(): unknown weapontype\n");
//	//	return;
//	//}
//}


void mapChannel_registerAutoFireTimer(mapChannelClient_t* cm)
{
	// get player current weapon
	item_t* itemWeapon = inventory_CurrentWeapon(cm);
	if( itemWeapon == NULL || itemWeapon->itemTemplate->item.type != ITEMTYPE_WEAPON )
		return; // invalid entity or incorrect item type
	//itemWeapon->itemTemplate->weapon.refireTime
	sint32 refireTime = 250; // we get this info from the duration of the action?

	mapChannelAutoFireTimer_t timer;
	timer.delay = refireTime;
	timer.timeLeft = refireTime;
	//timer.origin = NULL;
	//timer.weapon = NULL;
	timer.client = cm;
	cm->mapChannel->autoFire_timers.push_back(timer);
	// launch missile
	missile_playerTryFireWeapon(cm);
	//mapChannel_launchMissileForWeapon(origin->actor->owner, weapon);
}

void mapChannel_removeAutoFireTimer(mapChannelClient_t* cm)
{
	mapChannel_t* mapChannel = cm->mapChannel;
	std::vector<mapChannelAutoFireTimer_t>::iterator timer = mapChannel->autoFire_timers.begin();
	while (timer != mapChannel->autoFire_timers.end())
	{
		if (timer->client == cm)
		{
			timer = mapChannel->autoFire_timers.erase(timer);
		}
		else { ++timer; }
	}
}

void mapChannel_check_AutoFireTimers(mapChannel_t* mapChannel)
{
	std::vector<mapChannelAutoFireTimer_t>::iterator timer;
	for(timer = mapChannel->autoFire_timers.begin(); timer < mapChannel->autoFire_timers.end(); timer++)
	{
		timer->timeLeft -= 100;
		if (timer->timeLeft <= 0)
		{
			//if (timer->origin->actor->inCombatMode == false)
			//{ continue; /* TODO: delete timer here */ }
			//if (timer->origin->targetEntityId)
			//{
				//mapChannel_launchMissileForWeapon(timer->origin->actor->owner, timer->weapon);
			//}
			missile_playerTryFireWeapon(timer->client);
			timer->timeLeft = timer->delay;
		}
	}
}

//20110827 @dennton
bool CheckTempCharacter(di_characterData_t *tcd)
{
   bool valid = true;   
   if(tcd == NULL) valid = false;
   if(tcd->missionStateData == NULL) valid = false;
   return valid;
}


void mapChannel_recv_mapLoaded(mapChannelClient_t *cm, uint8 *pyString, sint32 pyStringLen)
{
	manifestation_createPlayerCharacter(cm->mapChannel, cm, cm->tempCharacterData);
	communicator_registerPlayer(cm);
	communicator_playerEnterMap(cm);
	inventory_initForClient(cm);
	mission_initForClient(cm);
	// free temporary character data	
	if( CheckTempCharacter(cm->tempCharacterData) != 0 )// 20110827 @dennton
	{
		if( cm->tempCharacterData->missionStateData )
			free(cm->tempCharacterData->missionStateData);
		free(cm->tempCharacterData);
	}
}

void mapChannel_recv_LogoutRequest(mapChannelClient_t *cm, uint8 *pyString, sint32 pyStringLen)
{
	pyMarshalString_t pms;
	// send time remaining to logout
	pym_init(&pms);
	pym_tuple_begin(&pms);
	pym_addInt(&pms, 0*1000); // milliseconds
	pym_tuple_end(&pms);
	netMgr_pythonAddMethodCallRaw(cm->cgm, 5, LogoutTimeRemaining, pym_getData(&pms), pym_getLen(&pms));
	cm->logoutRequestedLast = GetTickCount64();
	cm->logoutActive = true;
}

void mapChannel_recv_CharacterLogout(mapChannelClient_t *cm, uint8 *pyString, sint32 pyStringLen)
{
	//pyMarshalString_t pms;
	// pass to character selection
	if( cm->logoutActive == false )
		return;
	cm->removeFromMap = true;
}

void mapChannel_recv_ClearTrackingTarget(mapChannelClient_t *cm, uint8 *pyString, sint32 pyStringLen)
{
	pyMarshalString_t pms;
	// send new tracking target
	pym_init(&pms);
	pym_tuple_begin(&pms);
	pym_addLong(&pms, 0); // tracking target - none
	pym_tuple_end(&pms);
	netMgr_pythonAddMethodCallRaw(cm->cgm, cm->player->actor->entityId, SetTrackingTarget, pym_getData(&pms), pym_getLen(&pms));
}

void mapChannel_recv_SetTrackingTarget(mapChannelClient_t *cm, uint8 *pyString, sint32 pyStringLen)
{
	// unpack new tracking target
	pyUnmarshalString_t pums;
	pym_init(&pums, pyString, pyStringLen);
	if( !pym_unpackTuple_begin(&pums) )
		return;
	long long trackingTargetEntityId = pym_unpackLongLong(&pums);
	// send new tracking target
	pyMarshalString_t pms;
	pym_init(&pms);
	pym_tuple_begin(&pms);
	pym_addLong(&pms, trackingTargetEntityId); // tracking target
	pym_tuple_end(&pms);
	netMgr_pythonAddMethodCallRaw(cm->cgm, cm->player->actor->entityId, SetTrackingTarget, pym_getData(&pms), pym_getLen(&pms));
}

void mapChannel_processPythonRPC(mapChannelClient_t *cm, uint32 MethodID, uint8 *pyString, sint32 pyStringLen)
{
	// check if 'O'
	if( *pyString != 'O' )
		__debugbreak(); // oh shit...
	pyString++; pyStringLen--;

	switch( MethodID )
	{
	case RequestEquip:// todo 1
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Whisper:// MethodID 2
		communicator_recv_whisper(cm, pyString, pyStringLen);
		return;
	case GlobalChat:// todo 3
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyChat:// todo 4
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GuildChat:// todo 5
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddFriend:// todo 6
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveFriend:// todo 7
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddIgnore:// todo 8
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveIgnore:// todo 9
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Abilities:// todo 10
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AbortMission:// todo 11
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AckPing:// todo 12
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ActionFailed:// todo 13
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ActorControllerInfo:// todo 14
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ActorInfo:// todo 15
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ActorName:// todo 16
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddFriendAck:// todo 17
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddIgnoreAck:// todo 18
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddPartyMember:// todo 19
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddRandomInventoryItemHack:// todo 20
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddSquadMember:// todo 21
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddToLauncher:// todo 22
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AdminChat:// todo 23
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AdminMessage:// todo 24
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AdvancementStats:// todo 25
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AllocateAttributePoints:// MethodID 26
		manifestation_recv_AllocateAttributePoints(cm, pyString, pyStringLen);
		return;
	case AppearanceData:// todo 27
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AttachedGameEffects:// todo 28
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AttributeInfo:// todo 29
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AvailableAllocationPoints:// todo 30
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AvailableCharacterClasses:// todo 31
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BeginTeleport:// todo 32
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BeginUserCreation:// todo 33
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BodyAttributes:// todo 34
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BuryMe:// todo 35
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CallActorMethod:// todo 36
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CallGameEffectMethod:// todo 37
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CannotInvite:// todo 38
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CharacterChanged:// todo 39
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CharacterClass:// todo 40
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CharacterInfo:// todo 41
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ClearShortcut:// todo 42
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ClearTargetId:// MethodID 43
		manifestation_recv_ClearTargetId(cm, pyString, pyStringLen);
		return;
	case CloseDebrief:// todo 44
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CombatMode:// todo 45
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateAndGoToMapInstance:// todo 46
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateNewCharacterFailed:// todo 47
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateNewCharacterOK:// todo 48
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreatePhysicalEntity:// todo 49
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateUser:// todo 50
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CurrentCharacterId:// todo 51
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DeleteCharacterFailed:// todo 52
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DeleteCharacterOK:// todo 53
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DeleteSavePositionFailed:// todo 54
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DeleteSavePositionOK:// todo 55
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DestroyPhysicalEntity:// todo 56
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Died:// todo 57
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisableMovement:// todo 58
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisableWaypointList:// todo 59
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayDemoHackPlayerMessageDemoHack:// todo 60
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayPartyMessage:// todo 61
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplaySystemMessage:// todo 62
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DoSwitch:// todo 63
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EnableMovement:// todo 64
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EnableWaypointList:// todo 65
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EquipmentInfo:// todo 66
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ExamineHack:// todo 67
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ExamineResults:// todo 68
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ExperienceIncreased:// todo 69
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case FatalError:// todo 70
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case FriendList:// todo 71
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Funds:// todo 72
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GameEffectApplied:// todo 73
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GameEffectAttached:// todo 74
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GameEffectDetached:// todo 75
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GameEffectDurationChange:// todo 76
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GeneratedCharacterName:// todo 77
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GotLoot:// todo 78
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HomeInventory_Close:// todo 79
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HomeInventory_DestroyItem:// todo 80
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HomeInventory_MoveItem:// todo 81
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HomeInventory_Open:// todo 82
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HoveringChanged:// todo 83
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case IgnoreList:// todo 84
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryAddItem:// todo 85
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryCreate:// todo 86
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryDestroy:// todo 87
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryRemoveItem:// todo 88
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryTransfer:// todo 89
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InvitationCancelled:// todo 90
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InvitationDeclined:// todo 91
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InviteFriendToJoin:// todo 92
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InviteToParty:// todo 93
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InviteUserToParty:// todo 94
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InvitedToJoinFriend:// todo 95
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case IsRunning:// todo 96
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case KickUserFromParty:// todo 97
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LauncherEntered:// todo 98
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LauncherExited:// todo 99
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LauncherReady:// todo 100
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LauncherUnready:// todo 101
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LeaveParty:// todo 102
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Level:// todo 103
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LevelIncreased:// todo 104
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LoginOk:// todo 105
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MapInstanceList:// todo 106
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MapLoaded:// MethodID 107
		mapChannel_recv_mapLoaded(cm, pyString, pyStringLen);
		return;
	case MethodX:// todo 108
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Module2Info:// todo 109
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ModuleCondition:// todo 110
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ModuleInfo:// todo 111
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	// MethodID 112 do not exist
	case MoveAbilityShortcut:// todo 113
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MoveEntityShortcut:// todo 114
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MoveGestureShortcut:// todo 115
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case NonFatalError:// todo 116
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Notification:// todo 117
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case OpenDebrief:// todo 118
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyInvitationResponse:// todo 119
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberList:// todo 120
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberLogin:// todo 121
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberLogout:// todo 122
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberLoot:// todo 123
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PerformActionRequest:// todo 124
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PerformRecovery:// todo 125
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PerformWindup:// todo 126
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PersonalInventory_DestroyItem:// MethodID 127
		item_recv_PersonalInventoryDestroyItem(cm, pyString, pyStringLen);
		return;
	case PersonalInventory_MoveItems:// todo 128
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Ping:// todo 129
		//printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		//HexOut(pyString, pyStringLen);
		//printf("\n\n");
		return;
	case PlayerLogin:// todo 130
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerLogout:// todo 131
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PostTeleport:// todo 132
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PreTeleport:// todo 133
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PreWonkavate:// todo 134
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case QAKnowledgeQuery:// todo 135
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RadialChat:// MethodID 136
		communicator_recv_radialChat(cm, pyString, pyStringLen);
		return;
	case RemoveFriendAck:// todo 137
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveFromLauncher:// todo 138
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveIgnoreAck:// todo 139
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemovePartyMember:// todo 140
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveSelf:// todo 141
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveSquadMember:// todo 142
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestAddItemToTrade:// todo 143
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestAttachModuleToWeapon:// todo 144
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestBind:// todo 145
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCancelTrade:// todo 146
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestChangeEnergyUnitAmount:// todo 147
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCharacterInfo:// todo 148
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCharacterName:// todo 149
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestConfirmTrade:// todo 150
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCraftItem:// todo 151
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCraftingStatus:// todo 152
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCreateNewCharacter:// todo 153
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCreateSingleUseRecipe:// todo 154
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestDeleteCharacter:// todo 155
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestDeleteSavePosition:// todo 156
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestMapInstanceList:// todo 157
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
	case RequestNPCVendorPurchase:// todo 158
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestNPCVendorSale:// todo 159
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestRemoveItemFromTrade:// todo 160
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestSaveCurrentCharacterToPosition:// todo 161
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestSavePositionInfo:// todo 162
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestSwitchToCharacter:// todo 163
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestToggleRun:// MethodID 164
		manifestation_recv_ToggleRun(cm, pyString, pyStringLen);
		return;
	case RequestTrade:// todo 165
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestUnconfirmTrade:// todo 166
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestUnequip:// todo 167
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestWaypointList:// todo 168
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RespondToJoinFriend:// todo 169
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ReviveMe:// MethodID 170
		manifestation_recv_Revive(cm, pyString, pyStringLen);
		return;
	case Revived:// todo 171
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RevokeInvitation:// todo 172
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SaveCharacterSnapshotFailed:// todo 173
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SaveCharacterSnapshotOK:// todo 174
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SavePositionInfo:// todo 175
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SelectAbilitiesAndPumps:// todo 176
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SelectNewCharacterClass:// todo 177
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SelectWaypoint:// MethodID 178
		waypoint_recv_SelectWaypoint(cm, pyString, pyStringLen);
		wormhole_recv_SelectWormhole(cm, pyString, pyStringLen);// probobly we dont need this, all should be under select waypoint
		return;
	case SendMeToMapHack:// todo 179
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SendMeToMyApartment:// todo 180
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SendPlayerToMapInstance:// todo 181
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ServerPerformanceMetrics:// todo 182
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetAbilityShortcut:// todo 183
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetAdventureBrief:// todo 184
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetAdventureFailureDebrief:// todo 185
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetAdventureSuccessDebrief:// todo 186
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetAutoDefend:// todo 187
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetConfig:// todo 188
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetConsumable:// todo 189
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetControlledActorId:// todo 190
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetCurrentPartyId:// todo 191
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetEntityShortcut:// todo 192
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetGestureShortcut:// todo 193
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetHoverAltitude:// todo 194
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetPartyLeader:// todo 195
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetObjectiveBrief:// todo 196
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetShortcuts:// todo 197
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetSkyTime:// todo 198
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetStackCount:// todo 199
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetTarget:// todo 200
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetTargetId:// MethodID 201
		manifestation_recv_SetTargetId(cm, pyString, pyStringLen);
		return;
	case SetTrackingTarget:// MethodID 202
		mapChannel_recv_SetTrackingTarget(cm, pyString, pyStringLen);
		return;
	case SetUsable:// todo 203
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Shout:// MethodID 204;
		communicator_recv_shout(cm, pyString, pyStringLen);
		return;
	case SquadMemberList:// todo 205
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case StateChange:// todo 206
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case StateChangeRequest:// todo 207
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case StateCorrection:// todo 208
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SwitchToCharacterFailed:// todo 209
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SwitchToCharacterOK:// todo 210
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TargetCategory:// todo 211
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Teleport:// todo 212
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TeleportAcknowledge:// todo 213
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TierAdvancementInfo:// todo 214
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeAddItem:// todo 215
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeConfirmChange:// todo 216
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeCreate:// todo 217
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeDestroy:// todo 218
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeEnergyUnitsChange:// todo 219
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeRemoveItem:// todo 220
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Update:// todo 221
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateAbilityCostModifiers:// todo 222
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateAttribute:// todo 223
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateAttributeRefresh:// todo 224
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateChiAttribute:// todo 225
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateLauncherState:// todo 226
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateMovement:// todo 227
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdatePartyMemberState:// todo 228
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UsableInfo:// todo 229
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Use:// todo 230
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UserCreationFailed:// todo 231
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UserJoinedParty:// todo 232
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UserLeftParty:// todo 233
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	// MethodID 234 do not exist
	case WaypointAdded:// todo 235
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponBindCount:// todo 236
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponInfo:// todo 237
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponState:// todo 238
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WhisperAck:// todo 239
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WhisperFailAck:// todo 240
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WhisperSelf:// todo 241
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Wonkavate:// todo 242
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WorldLocationDescriptor:// todo 243
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WorldPlacementDescriptor:// todo 244
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ActionInterrupt:// todo 245
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddOverflowItem:// todo 246
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AdventureHistory:// todo 247
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AdventureStatus:// todo 248
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CanDishonorUser:// todo 249
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CanDishonorUserAck:// todo 250
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CanHonorUser:// todo 251
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CanHonorUserAck:// todo 252
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CannotJoin:// todo 253
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ChangeGuildMaster:// todo 254
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ClearTrackingTarget:// MethodID 255
		mapChannel_recv_ClearTrackingTarget(cm, pyString, pyStringLen);
		return;
	case CommandFollowAck:// todo 256
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CommandStopFollowAck:// todo 257
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreatureCommandFollow:// todo 258
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreatureCommandStopFollow:// todo 259
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DamageInfo:// todo 260
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DeadOnArrival:// todo 261
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DetachGameEffect:// todo 262
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Detonate:// todo 263
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisbandGuild:// todo 264
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisbandResponse:// todo 265
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Disbanded:// todo 266
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DishonorUser:// todo 267
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DishonorUserFailed:// todo 268
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DishonorUserSucceeded:// todo 269
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DishonoredByUser:// todo 270
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayGuildMessage:// todo 271
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Dying:// todo 272
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EnableFollow:// todo 273
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EnteredWaypoint:// todo 274
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ExitedWaypoint:// todo 275
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Fire:// todo 276
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case FundsUpdate:// todo 277
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GameEffectTick:// todo 278
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GameEffects:// todo 279
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GetServerCollisionData:// todo 280
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GetServerSkeleton:// todo 281
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GuildAbbreviation:// todo 282
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GuildAbbreviationAndTitle:// todo 283
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GuildInvitationResponse:// todo 284
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HitPointChange:// todo 285
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Honor:// todo 286
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HonorUser:// todo 287
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HonorUserFailed:// todo 288
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HonorUserSucceeded:// todo 289
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HonoredByUser:// todo 290
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryDisabled:// todo 291
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Invitation:// todo 292
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InvitationResult:// todo 293
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InviteUserIntoGuild:// todo 294
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case JoinFriendCancelled:// todo 295
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case JoinFriendDeclined:// todo 296
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LeaveGuild:// todo 297
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LeftGuild:// todo 298
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LevelUp:// todo 299
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MarketPlaceSearchResults:// todo 300
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MasterChangeResponse:// todo 301
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MasterChanged:// todo 302
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MemberJoined:// todo 303
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MemberLeft:// todo 304
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MemberLogin:// todo 305
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MemberLogout:// todo 306
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ModuleDead:// todo 307
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case OverflowTransfer:// todo 308
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberNameList:// todo 309
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberVoiceId:// todo 310
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PerformObjectAbility:// todo 311
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerJoinedTeam:// todo 312
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerLeftTeam:// todo 313
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerTeamInfo:// todo 314
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerVendorAddItem:// todo 315
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerVendorCreate:// todo 316
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerVendorDestroy:// todo 317
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerVendorRemoveItem:// todo 318
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerVendorUpdateSaleInfo:// todo 319
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveMember:// todo 320
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveOverflowItem:// todo 321
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveUserFromGuild:// todo 322
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveUserResponse:// todo 323
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Removed:// todo 324
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestAddItemToPlayerVendor:// todo 325
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestAdventureHistory:// todo 326
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestAdventureStatus:// todo 327
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCampaignStatus:// todo 328
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCancelNPCVendor:// todo 329
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestChiStrike:// todo 330
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestClosePlayerVendor:// todo 331
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCustomization:// todo 332
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestDepositGuildFunds:// todo 333
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestDetach:// todo 334
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestDynamicAdventureUpdates:// todo 335
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestGesture:// todo 336
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestInvitationToJoin:// todo 337
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestIssueHTMLInteraction:// todo 338
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestJoinVoiceChannel:// todo 339
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestLeaveVoiceChannel:// todo 340
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestModifyPlayerVendorItemInfo:// todo 341
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestNPCVendorCreateGuild:// todo 342
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestPerformAbility:// MethodID 343
		manifestation_recv_RequestPerformAbility(cm, pyString, pyStringLen);
		return;
	case RequestPlaceObject:// todo 344
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestPlayerVendorItemsByCategory:// todo 345
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestPurchasePlayerVendorItem:// todo 346
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestRemoveItemFromPlayerVendor:// todo 347
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestRepair:// todo 348
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestReturnItemToInventory:// todo 349
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestToJoin:// todo 350
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestUnstick:// todo 351
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestUseObject:// MethodID 352
		dynamicObject_recv_RequestUseObject(cm, pyString, pyStringLen);
		return;
	case RequestViewPlayerVendor:// todo 353
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestWeaponAttack:// MethodID 354
		missile_playerTryFireWeapon(cm);
		return;
	case RequestWithdrawGuildFunds:// todo 355
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ResetOverflowInventory:// todo 356
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RespondToRequestToJoin:// todo 357
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RunCameraScript:// todo 358
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SalaryPaid:// todo 359
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ServerCollisionData:// todo 360
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ServerSkeleton:// todo 361
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetCurrentContextId:// todo 362
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetGuildTitle:// todo 363
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetInfo:// todo 364
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetIsContextOwner:// todo 365
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetIsGM:// todo 366
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetKillStreak:// todo 367
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetMaster:// todo 368
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetReuseTimeModifier:// todo 369
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetServerLanguages:// todo 370
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Swap:// todo 371
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TargetId:// todo 372
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TeleportArrival:// todo 373
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TeleportFailed:// todo 374
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TitleChangeResponse:// todo 375
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateAdventureList:// todo 376
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateCampaignStatus:// todo 377
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateChi:// todo 378
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateConsumables:// todo 379
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateHealth:// todo 380
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdatePhysicalEntity:// todo 381
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UserTitheUpdate:// todo 382
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UserTitleChanged:// todo 383
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case VoiceChatAvailable:// todo 384
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case VoiceChatConnectInfo:// todo 385
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WaypointList:// todo 386
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponBroken:// todo 387
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Who:// todo 388
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WhoAck:// todo 389
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WhoFailAck:// todo 390
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WhoUserCountAck:// todo 391
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AbandonMission:// todo 392
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AbilityDrawer:// todo 393
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AbilityDrawerSlot:// todo 394
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AcceptPartyInvitesChanged:// todo 395
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddBuybackItem:// todo 396
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddToPetition:// todo 397
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AddToPetitionAck:// todo 398
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AnnounceDamage:// todo 399
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AnnounceLeech:// todo 400
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AnnounceMapDamage:// todo 401
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AppearanceMeshId:// todo 402
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ArmAbilityFailed:// todo 403
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ArmWeaponFailed:// todo 404
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ArmorBroken:// todo 405
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ArmorInfo:// todo 406
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AssignNPCMission:// MethodID 407
		npc_recv_AssignNPCMission(cm, pyString, pyStringLen);
		return;
	case AssignRadioMission:// todo 408
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AssignSharedMission:// todo 409
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AutoFireKeepAlive:// MethodID 410
		manifestation_recv_AutoFireKeepAlive(cm, pyString, pyStringLen);
		return;
	case Bark:// todo 411
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BattlecryNotification:// todo 412
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BeginCharacterSelection:// todo 413
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotClearInventory:// todo 414
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotFillInventory:// todo 415
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotGimmeAmmo:// todo 416
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotPrepareForCombat:// todo 417
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotRequestStartGroupList:// todo 418
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotTeleportToLocalStartGroup:// todo 419
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotTeleportToLocation:// todo 420
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotTeleportToSpawner:// todo 421
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CancelPetition:// todo 422
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CancelPetitionAck:// todo 423
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ChangePartyLootMethod:// todo 424
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ChangePartyLootThreshold:// todo 425
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CharacterCreateSuccess:// todo 426
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CharacterName:// todo 427
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CharacterSelect:// todo 428
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ClassTooltipInfo:// todo 429
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CompleteNPCMission:// MethodID 430
		npc_recv_CompleteNPCMission(cm, pyString, pyStringLen);
		return;
	case CompleteNPCObjective:// MethodID 431
		npc_recv_CompleteNPCObjective(cm, pyString, pyStringLen);
		return;
	case CompleteRadioMission:// todo 432
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Converse:// todo 433
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CraftingStatus:// todo 434
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateBugReport:// todo 435
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateCharacter:// todo 436
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreateHelpRequest:// todo 437
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreatePetitionAck:// todo 438
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case CreatureInfo:// todo 439
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DeclineSharedMission:// todo 440
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DevCommand:// todo 441
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DevStartMapAck:// todo 442
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisbandParty:// todo 443
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DispenseRadioMission:// todo 444
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DispenseSharedMission:// todo 445
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayDestinationContextNotification:// todo 446
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayMapErrors:// todo 447
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayPlayerNotification:// todo 448
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case DisplayPlayerTutorialNotification:// todo 449
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EmitterInfo:// todo 450
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case EnableDevCommands:// todo 451
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ExperienceChanged:// todo 452
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case FinishedCameraScript:// todo 453
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ForceState:// todo 454
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GMCommand:// todo 455
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GeneratedFamilyName:// todo 456
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GmCmd_ForceCompleteObjective:// todo 457
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GmGotoMapAck:// todo 458
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GmGotoStartGroupAck:// todo 459
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GmKillMapAck:// todo 460
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GmShowUserMissionsAck:// todo 461
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case GraveyardGained:// todo 462
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case HackCommand:// todo 463
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryMoveFailed:// todo 464
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InventoryMoveItemFailed:// todo 465
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InviteUserToPartyByName:// todo 466
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case InvitedToAddAndJoinFriend:// todo 467
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case IsLootable:// todo 468
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ItemInfo:// todo 469
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ItemStatus:// todo 470
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LevelChanged:// todo 471
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LevelSkills:// MethodID 472
		manifestation_recv_LevelSkills(cm, pyString, pyStringLen);
		return;
	case LockInfo:// todo 473
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LockToActor:// todo 474
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LogosStoneAdded:// todo 475
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LogosStoneRemoved:// todo 476
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LogosStoneTabula:// todo 477
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LootClassInfo:// todo 478
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LootQuantity:// todo 479
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MakeUserPartyLeader:// todo 480
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionCompleteable:// todo 481
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionCompleted:// todo 482
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionDiscarded:// todo 483
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionFailed:// todo 484
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionGained:// todo 485
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionRewarded:// todo 486
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MissionStatusInfo:// todo 487
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ModuleTooltipInfo:// todo 488
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case NPCConversationStatus:// todo 489
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case NPCInfo:// todo 490
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ObjectiveActivated:// todo 491
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ObjectiveCompleted:// todo 492
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ObjectiveFailed:// todo 493
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ObjectiveRevealed:// todo 494
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyDisbanded:// todo 495
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberRoll:// todo 496
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PerformNPCChoice:// todo 497
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PersonalInventory_MoveItem:// MethodID 498
		item_recv_PersonalInventoryMoveItem(cm, pyString, pyStringLen);
		return;
	case PersonalInventory_SplitItem:// todo 499
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerCount:// todo 500
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerCountAck:// todo 501
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case QACommand:// todo 502
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case QAGiveMissionAck:// todo 503
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RemoveBuybackItem:// todo 504
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestAddLogosStoneToTabula:// todo 505
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestArmAbility:// MethodID 506
		manifestation_recv_RequestArmAbility(cm, pyString, pyStringLen);
		return;
	case RequestArmWeapon:// MethodID 507
		item_recv_RequestArmWeapon(cm, pyString, pyStringLen);
		return;
	case RequestAttachModuleToArmor:// todo 508
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCancelVendor:// todo 509
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestChargedWeaponAttack:// todo 510
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCloneCharacterToSlot:// todo 511
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestCreateCharacterInSlot:// todo 512
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestDeleteCharacterInSlot:// todo 513
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestEquipArmor:// MethodID 514
		item_recv_RequestEquipArmor(cm, pyString, pyStringLen);
		return;
	case RequestEquipWeapon:// MethodID 515
		item_recv_RequestEquipWeapon(cm, pyString, pyStringLen);
		return;
	case RequestFamilyName:// todo 516
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestMovementBlock:// todo 517
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestNPCConverse:// MethodID 518
		npc_recv_RequestNPCConverse(cm, pyString, pyStringLen);
		return;
	case RequestNPCVending:// MethodID 519
		npc_recv_RequestNPCVending(cm, pyString, pyStringLen);
		return;
	case RequestPerformChargedAbility:// todo 520
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestRetrieveFinishedCraftItem:// todo 521
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestSetAbilitySlot:// MethodID 522
		manifestation_recv_RequestSetAbilitySlot(cm, pyString, pyStringLen);
		return;
	case RequestSwitchToCharacterInSlot:// todo 523
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestToolAction:// todo 524
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestTooltipForClassId:// todo 525
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestTooltipForModuleId:// todo 526
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestVendorBuyback:// todo 527
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestVendorPurchase:// MethodID 528
		npc_recv_RequestVendorPurchase(cm, pyString, pyStringLen);
		return;
	case RequestVendorSale:// MethodID 529
		npc_recv_RequestVendorSale(cm, pyString, pyStringLen);
		return;
	case RequestWeaponDraw:// MethodID 530
		item_recv_RequestWeaponDraw(cm, pyString, pyStringLen);
		return;
	case RequestWeaponReload:// MethodID 531
		item_recv_RequestWeaponReload(cm, pyString, pyStringLen, false);
		return;
	case RequestWeaponStow:// MethodID 532
		item_recv_RequestWeaponStow(cm, pyString, pyStringLen);
		return;
	case ResetBuybackInventory:// todo 533
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ResistanceData:// todo 534
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RespondToAddAndJoinFriend:// todo 535
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RetrieveKBArticle:// todo 536
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RetrieveKBArticleAck:// todo 537
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RetrievePetition:// todo 538
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RetrievePetitionAck:// todo 539
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RewardNPCMission:// todo 540
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RewardRadioMission:// todo 541
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SearchKB:// todo 542
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SearchKBAck:// todo 543
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SearchPetitions:// todo 544
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SearchPetitionsAck:// todo 545
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SetAutoLootThreshold:// todo 546
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ShareMission:// todo 547
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Skills:// todo 548
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case StartAutoFire:// MethodID 549
		manifestation_recv_StartAutoFire(cm, pyString, pyStringLen);
		return;
	case StopAutoFire:// MethodID 550
		manifestation_recv_StopAutoFire(cm, pyString, pyStringLen);
		return;
	case SwapMeshDeath:// todo 551
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SwapMeshRevive:// todo 552
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case SystemMessage:// todo 553
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TradeCompleted:// todo 554
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Train:// todo 555
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TrainSkill:// todo 556
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TranslateLogos:// todo 557
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TranslateLogosResult:// todo 558
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TurnOff:// todo 559
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TurnOn:// todo 560
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UnrequestMovementBlock:// todo 561
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateArmor:// todo 562
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateAttributes:// todo 563
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateHitPoints:// todo 564
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateObjectiveCounter:// todo 565
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateObjectiveItemCounter:// todo 566
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdatePower:// todo 567
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdateRegions:// todo 568
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case Vend:// todo 569
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WaypointGained:// todo 570
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponAmmoInfo:// todo 571
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponDrawerInventory_DestroyItem:// todo 572
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponDrawerInventory_MoveItem:// todo 573
		printf("todo: WeaponDrawerInventory_MoveItem\nMapChannel_RPCPacket - Size: %d\n", pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponDrawerSlot:// todo 574
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponReady:// todo 575
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case AnnounceCounterAttack:// todo 576
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ReflectDamage:// todo 577
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestTooltipForItemTemplateId:// MethodID 578
		item_recv_RequestTooltipForItemTemplateId(cm, pyString, pyStringLen);
		return;
	case ItemTemplateTooltipInfo:// todo 579
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case MovementModChange:// todo 580
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PartyMemberVoiceIds:// todo 581
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestMoveItemToHomeInventory:// todo 582
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestTakeItemFromHomeInventory:// todo 583
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case UpdatePartyMemberClassAndLevel:// todo 584
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case WeaponJammed:// todo 585
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotAssignNPCMission:// todo 586
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotCompleteNPCMission:// todo 587
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotCompleteNPCObjective:// todo 588
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotGimmeMoney:// todo 589
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotGimmePower:// todo 590
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotGimmeWeapon:// todo 591
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotRequestCreateMap:// todo 592
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case BotRequestMapInstanceList:// todo 593
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case IsTargetable:// todo 594
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case PlayerDead:// todo 595
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case TransferCreditToLockbox:// todo 596
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	// MethodID 597 do not exist
	// MethodID 598 do not exist
	case IsUsable:// todo 599
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case LockboxFunds:// todo 600
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestSwapAbilitySlots:// todo 601
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case ChallengeUserToWargameDuelByName:// todo 602
		printf("todo: MethodID: %d called\nMapChannel_RPCPacket - Size: %d\n", MethodID, pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
	case RequestVisualCombatMode: // RequestVisualCombatMode
		manifestation_recv_RequestVisualCombatMode(cm, pyString, pyStringLen);
		return;
	case RequestActionInterrupt: // RequestActionInterrupt
		dynamicObject_recv_RequestActionInterrupt(cm, pyString, pyStringLen);
		return;
	case RequestLogout: // RequestLogout
		mapChannel_recv_LogoutRequest(cm, pyString, pyStringLen);
		return;
	case ChannelChat: // ChannelChat
		communicator_recv_channelChat(cm, pyString, pyStringLen);
		return;
	case CharacterLogout: // CharacterLogout
		mapChannel_recv_CharacterLogout(cm, pyString, pyStringLen);
		return;
	case RequestLootAllFromCorpse: // player auto-loot full corpse
		lootdispenser_recv_RequestLootAllFromCorpse(cm, pyString, pyStringLen);
		return;
	default:
		printf("MapChannel_UnknownMethodID: %d\n", MethodID);
		printf("MapChannel_RPCPacket - Size: %d\n", pyStringLen);
		HexOut(pyString, pyStringLen);
		printf("\n\n");
		return;
		// no handler for that
	};

	// 149
	// 00001AA7     64 - LOAD_CONST          'RequestCharacterName'


	return;
}

#pragma pack(1)
typedef struct  
{
	sint32 contextId;
	sint32 pX;
	sint32 pY;
	sint32 pZ;
}movementLogEntry_t;
#pragma pack()

HANDLE hMovementLogFile = NULL;
void mapChannel_logMovement(sint32 contextId, sint32 x, sint32 y, sint32 z)
{
	return;
	if( hMovementLogFile == NULL )
	{
		hMovementLogFile = CreateFile("movementlog.bin", FILE_ALL_ACCESS, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
		SetFilePointer(hMovementLogFile, 0, 0, FILE_END);
	}
	// setup entry
	movementLogEntry_t entry;
	entry.contextId = contextId;
	entry.pX = x;
	entry.pY = y;
	entry.pZ = z;
	// write entry
	DWORD bytesWritten;
	WriteFile(hMovementLogFile, (LPCVOID)&entry, sizeof(movementLogEntry_t), &bytesWritten, NULL);
}


void mapChannel_decodeMovementPacket(mapChannelClient_t *mc, uint8 *data, uint32 len)
{
	if( mc->removeFromMap )
		return;
	if( mc->player == NULL )
		return;
	netCompressedMovement_t netMovement;
	uint32 pIdx = 0;
	uint32 counterA = *(uint32*)(data+pIdx); pIdx += 4;
	uint32 ukn1 = *(uint32*)(data+pIdx); pIdx += 4;
	uint32 counterB = *(uint32*)(data+pIdx); pIdx += 4;
	if( data[pIdx] != 2 )
		__debugbreak();
	pIdx++;
	if( data[pIdx] != 6 ) // subOpcode?
		__debugbreak();
	pIdx++;
	// skip unknown
	pIdx += 10;
	// for performance we actually dont parse the packet
	// posX
	if( data[pIdx] != 0x29 )
		__debugbreak();
	if( data[pIdx+7] != 0x2A )
		__debugbreak();
	sint32 val24b = (data[pIdx+2]<<16) | (data[pIdx+4]<<8) | (data[pIdx+6]);
	if( val24b&0x00800000 )
		val24b |= 0xFF000000;
	netMovement.posX24b = val24b;
	float posX = (float)val24b / 256.0f;
	sint32 vLogX = val24b;
	mc->player->actor->posX = posX;
	pIdx += 8;
	// posY
	if( data[pIdx] != 0x29 )
		__debugbreak();
	if( data[pIdx+7] != 0x2A )
		__debugbreak();
	val24b = (data[pIdx+2]<<16) | (data[pIdx+4]<<8) | (data[pIdx+6]);
	if( val24b&0x00800000 )
		val24b |= 0xFF000000;
	netMovement.posY24b = val24b;
	float posY = (float)val24b / 256.0f;
	sint32 vLogY = val24b;
	mc->player->actor->posY = posY;
	pIdx += 8;
	// posZ
	if( data[pIdx] != 0x29 )
		__debugbreak();
	if( data[pIdx+7] != 0x2A )
		__debugbreak();
	val24b = (data[pIdx+2]<<16) | (data[pIdx+4]<<8) | (data[pIdx+6]);
	if( val24b&0x00800000 )
		val24b |= 0xFF000000;
	netMovement.posZ24b = val24b;
	float posZ = (float)val24b / 256.0f;
	sint32 vLogZ = val24b;
	mc->player->actor->posZ = posZ;
	pIdx += 8;
	// read velocity
	//29 05 00 1A 2A velocity?	/1024.0
	if( data[pIdx] != 0x29 )
		__debugbreak();
	val24b = *(sint16*)(data+pIdx+2);
	netMovement.velocity = val24b;
	float velocity = (float)val24b / 1024.0f;
	if( data[pIdx+4] != 0x2A )
		__debugbreak();
	pIdx += 5;

	mapChannel_logMovement(mc->mapChannel->mapInfo->contextId, vLogX, vLogY, vLogZ);
	// read flag
	netMovement.flag = *(uint8*)(data+pIdx+1);
	pIdx += 2;
	// read viewX, viewY
	if( data[pIdx] != 0x29 )
		__debugbreak();
	val24b = *(sint16*)(data+pIdx+2);
	netMovement.viewX = val24b;
	float viewX = (float)val24b / 1024.0f; // factor guessed ??? find real
	val24b = *(sint16*)(data+pIdx+5);
	netMovement.viewY = val24b;
	float viewY = (float)val24b / 1024.0f; // factor guessed ???
	/*
	03 08
	29 05 04 66 05 62 08 2A 	???
	2A
	2A 31
	*/

	netMovement.entityId = mc->player->actor->entityId;

	//netMgr_broadcastEntityMovement(mc->mapChannel, &netMovement, true);
	netMgr_cellDomain_sendEntityMovement(mc, &netMovement, true);
	// void netMgr_broadcastEntityMovement(mapChannel_t *broadCastChannel, netCompressedMovement_t *movement, bool skipOwner)
	// prsint32 info
	//printf("move %f %f %f v: %f rXY: %f %f\n", posX, posY, posZ, velocity, viewX, viewY);
}

sint32 mapChannel_decodePacket(mapChannelClient_t *mc, uint8 *data, uint32 len)
{
	if( mc->removeFromMap )
		return 1;
	if( len >= 0xFFFF )
		__debugbreak();
	if( len < 4 )
		return 0;


	if( len >= 0xFFFF )
		__debugbreak();
	if( len < 4 )
		return 0;

	sint32 pIdx = 0;
	// read subSize
	uint32 subSize = *(uint16*)(data+pIdx); pIdx += 2; // redundancy with param len
	// read major opcode
	uint32 majorOpc = *(uint16*)(data+pIdx); pIdx += 2;
	if( majorOpc == 1 )
	{
		mapChannel_decodeMovementPacket(mc, data+pIdx, subSize-pIdx);
		return 1;
	}
	else if( majorOpc )
	{
		return 1; // ignore the packet
	}
	// read header A
	uint8 ukn1 = *(uint8*)(data+pIdx); pIdx +=1;
	if( ukn1 != 2 )
		__debugbreak();

	uint8 opcode = *(uint8*)(data+pIdx); pIdx +=1; // not 100% sure
	uint8 ukn2 = *(uint8*)(data+pIdx); pIdx +=1;
	if( ukn2 != 0 )
		__debugbreak();
	uint8 xorCheckA = *(uint8*)(data+pIdx); pIdx +=1;
	if( xorCheckA != 3 ) // we only know headerA length of 3 for now
		__debugbreak();

	uint32 hdrB_start = pIdx;
	uint8 ukn3 = *(uint8*)(data+pIdx); pIdx +=1;
	if( ukn3 != 3 )
		__debugbreak();
	// different handling now (dont support subOpc 2 here anymore)
	if( opcode == 0x0C )
	{
		// expect header B part 1 (0x29)
		if( *(uint8*)(data+pIdx) == 0x00 )
			return 1; // empty packet?
		if( *(uint8*)(data+pIdx) != 0x29 )
			__debugbreak(); // wrong
		pIdx++;
		uint8 ukn0C_1 = *(uint8*)(data+pIdx); pIdx++;
		if( ukn0C_1 != 3 ) __debugbreak();
		uint8 ukn0C_2 = *(uint8*)(data+pIdx); pIdx++;
		//if( ukn0C_2 != 1 && ukn0C_2 != 3 && ukn0C_2 != 9 ) __debugbdfdsfreak(); // server entityId?
		if( ukn0C_2 == 0 || ukn0C_2 > 0x10 ) __debugbreak(); // server entityId?
		uint8 preffix0C_1 = *(uint8*)(data+pIdx); pIdx++;
		if( preffix0C_1 != 7 ) __debugbreak(); // 7 --> 32-bit sint32
		uint32 methodID = *(uint32*)(data+pIdx); pIdx += 4;
		uint8 ukn0C_3 = *(uint8*)(data+pIdx); pIdx++; // entityID?
		if( ukn0C_3 != 1 ) __debugbreak();
		// part 2 (0xCB)
		if( *(uint8*)(data+pIdx) != 0xCB )
			__debugbreak(); // wrong
		pIdx++;
		uint32 dataLen = 0;
		uint32 lenMask = *(uint8*)(data+pIdx); pIdx++;
		if( (lenMask>>6) == 0 )
		{
			// 6 bit length
			dataLen = lenMask&0x3F;
		}
		else if( (lenMask>>6) == 1 )
		{
			// 14 bit length
			dataLen = (lenMask&0x3F);
			dataLen |= ((*(uint8*)(data+pIdx))<<6);
			pIdx++;
		}
		else
			__debugbreak();
		mapChannel_processPythonRPC(mc, methodID, data+pIdx, dataLen);
		pIdx += dataLen;
		// xor check...
	}
	else
		return 1;
	return 1;
}

void mapChannel_readData(mapChannelClient_t *mc)
{
	// todo: disconnect client on error...
	clientGamemain_t *cgm = mc->cgm;
	if( cgm->RecvState < 4 )
	{
		sint32 r = recv(cgm->socket, (char*)cgm->RecvBuffer+cgm->RecvState, 4-cgm->RecvState, 0);
		if( r == 0 || r == SOCKET_ERROR )
		{
			mc->removeFromMap = true;
			mc->disconnected = true;
			return;
		}
		cgm->RecvState += r;
		if( cgm->RecvState == 4 )
			cgm->RecvSize = *(uint32*)cgm->RecvBuffer + 4;
		return;
	}
	sint32 r = recv(cgm->socket, (char*)cgm->RecvBuffer+cgm->RecvState, cgm->RecvSize-cgm->RecvState, 0);
	if( r == 0 || r == SOCKET_ERROR )
	{
		mc->removeFromMap = true;
		mc->disconnected = true;
		return;
	}
	cgm->RecvState += r;

	if( cgm->RecvState == cgm->RecvSize )
	{
		// full packet received
		// everything is encrypted, so do decryption job here
		Tabula_Decrypt2(&cgm->tbc2, (uint32*)(cgm->RecvBuffer+4), cgm->RecvSize);
		sint32 r = 0;
		sint32 AlignBytes = cgm->RecvBuffer[4]%9;

		uint8 *Buffer = cgm->RecvBuffer + 4 + AlignBytes;
		sint32 Size = cgm->RecvSize - 4 - AlignBytes;
		do{
			uint16 Subsize = *(uint16*)Buffer;
			mapChannel_decodePacket(mc, Buffer, Subsize);
			Buffer += Subsize;
			Size -= Subsize;
		}while(Size > 0);
		cgm->RecvState = 0;
		return;
	}
	return;
}

sint32 mapChannel_worker(mapChannelList_t *channelList)
{
	
	FD_SET fd;
	timeval sTimeout;
	sTimeout.tv_sec = 0;
	sTimeout.tv_usec = 10000;
	global_channelList = channelList; //20110827 @dennton

	// init mapchannel (map instance)
	printf("Initializing MapChannel...\n");
	for(sint32 chan=0; chan<channelList->mapChannelCount; chan++)
	{
		mapChannel_t *mapChannel = channelList->mapChannelArray+chan;
		if( cellMgr_initForMapChannel(mapChannel) == false )
		{
			printf("Error on map-cell init in mapContextId %d\n", mapChannel->mapInfo->contextId);
			Sleep(5000);
			ExitThread(-1);
		}
		navmesh_initForMapChannel(mapChannel);
		dynamicObject_init(mapChannel);
		mission_initForChannel(mapChannel);
		missile_initForMapchannel(mapChannel);
		spawnPool_initForMapChannel(mapChannel); //---todo:db use -done
		controller_initForMapChannel(mapChannel);
		teleporter_initForMapChannel(mapChannel); //---load teleporters
		logos_initForMapChannel(mapChannel); // logos world objects
	}

	printf("MapChannel started...\n");

	while( true )
	{
		for(sint32 chan=0; chan<channelList->mapChannelCount; chan++)
		{
			mapChannel_t *mapChannel = channelList->mapChannelArray+chan;
			// check for new players in queue (one per round)
			if( mapChannel->rb_playerQueueReadIndex != mapChannel->rb_playerQueueWriteIndex )
			{
				mapChannel_addNewPlayer(mapChannel, mapChannel->rb_playerQueue[mapChannel->rb_playerQueueReadIndex]);
				mapChannel->rb_playerQueueReadIndex = (mapChannel->rb_playerQueueReadIndex+1)%MAPCHANNEL_PLAYERQUEUE;
			}
			// recv client data
			FD_ZERO(&fd);
			for(sint32 i=0; i<mapChannel->playerCount; i++)
			{
				FD_SET(mapChannel->playerList[i]->cgm->socket, &fd);
			}
			sint32 r = select(0, &fd, 0, 0, &sTimeout);
			if( r )
			{
				for(sint32 i=0; i<fd.fd_count; i++)
				{
					mapChannelClient_t *mc = (mapChannelClient_t*)hashTable_get(&mapChannel->ht_socketToClient, (uint32)fd.fd_array[i]);
					if( mc )
						mapChannel_readData(mc);
					else
					{
						continue;
					}
					if( mc->removeFromMap )
					{
						communicator_playerExitMap(mc);
						mapChannel_removePlayer(mc);
					}
				}
			}
			
			if (mapChannel->playerCount > 0)
			{
				// do other work
				cellMgr_doWork(mapChannel);
				// check timers
				uint32 currentTime = GetTickCount64();
				if( (currentTime - mapChannel->timer_clientEffectUpdate) >= 500 )
				{
					gameEffect_checkForPlayers(mapChannel->playerList, mapChannel->playerCount, 500);
					mapChannel->timer_clientEffectUpdate += 500;
				}
				//if (mapChannel->cp_trigger.cb != NULL)
				//{
				//	if ((currentTime - mapChannel->cp_trigger.period) >= 100)
				//	{
				//		mapChannel->cp_trigger.timeLeft -= 100;
				//		mapChannel->cp_trigger.period = currentTime;
				//		if (mapChannel->cp_trigger.timeLeft <= 0)
				//		{
				//			mapChannel->cp_trigger.cb(mapChannel, mapChannel->cp_trigger.param, 1);
				//			mapChannel->cp_trigger.cb = NULL;
				//		}
				//	}
				//}
				if( (currentTime - mapChannel->timer_missileUpdate) >= 100 )
				{
					missile_check(mapChannel, 100);
					mapChannel->timer_missileUpdate += 100;
				}
				if( (currentTime - mapChannel->timer_dynObjUpdate) >= 100 )
				{
					dynamicObject_check(mapChannel, 100);
					mapChannel->timer_dynObjUpdate += 100;
				}
				if( (currentTime - mapChannel->timer_controller) >= 250 )
				{
					mapteleporter_checkForEntityInRange(mapChannel);
					controller_mapChannelThink(mapChannel);
					mapChannel->timer_controller += 250;
				}
				if( (currentTime - mapChannel->timer_playerUpdate) >= 1000 )
				{
					uint32 playerUpdateTick = currentTime - mapChannel->timer_playerUpdate;
					mapChannel->timer_playerUpdate = currentTime;
					for(uint32 i=0; i<mapChannel->playerCount; i++)
					{
						manifestation_updatePlayer(mapChannel->playerList[i], playerUpdateTick);
					}
				}
				if( (currentTime - mapChannel->timer_generalTimer) >= 100 )
				{
					sint32 timePassed = 100;
					// queue for deleting map timers
					std::vector<mapChannelTimer_t*> queue_timerDeletion;
					// parse through all timers
					mapChannel_check_AutoFireTimers(mapChannel);
					std::vector<mapChannelTimer_t*>::iterator timer = mapChannel->timerList.begin();
					while (timer != mapChannel->timerList.end())
					{
						(*timer)->timeLeft -= timePassed;
						if( (*timer)->timeLeft <= 0 )
						{
							sint32 objTimePassed = (*timer)->period - (*timer)->timeLeft;
							(*timer)->timeLeft += (*timer)->period;
							// trigger object
							bool remove = (*timer)->cb(mapChannel, (*timer)->param, objTimePassed);
							if( remove == false )
								queue_timerDeletion.push_back(*timer);
						}
						timer++;
					}
					// parse deletion queue
					if( queue_timerDeletion.empty() != true )
					{
						mapChannelTimer_t **timerList = &queue_timerDeletion[0];
						sint32 timerCount = queue_timerDeletion.size();
						for(sint32 f=0; f<timerCount; f++)
						{
							mapChannelTimer_t* toBeDeletedTimer = timerList[f];
							// remove from timer list
							std::vector<mapChannelTimer_t*>::iterator itr = mapChannel->timerList.begin();
							while (itr != mapChannel->timerList.end())
							{
								if ((*itr) == toBeDeletedTimer)
								{
									mapChannel->timerList.erase(itr);
									break;
								}
								++itr;
							}
							// free timer
							free(toBeDeletedTimer);
						}
					}
				
					//sint32 count = hashTable_getCount(&mapChannel->list_timerList);
					//mapChannelTimer_t **timerList = (mapChannelTimer_t**)hashTable_getValueArray(&mapChannel->list_timerList);
					//for(sint32 i=0; i<count; i++)
					//{
					//	mapChannelTimer_t *entry = timerList[i];
					//	entry->timeLeft -= timePassed;
					//	if( entry->timeLeft <= 0 )
					//	{
					//		sint32 objTimePassed = entry->period - entry->timeLeft;
					//		entry->timeLeft += entry->period;
					//		// trigger object
					//		bool remove = entry->cb(mapChannel, entry->param, objTimePassed);//dynamicObject_process(mapChannel, dynObjectWorkEntry->object, objTimePassed);
					//		if( remove == false )
					//			__debugbreak(); // todo!
					//	}
					//}
					mapChannel->timer_generalTimer += 100;
				}
			} // (mapChannel->playerCount > 0)
		}
		Sleep(1); // eventually remove/replace this (dont sleep when too busy)
	}
	return 0;
}

void mapChannel_start(sint32 *contextIdList, sint32 contextCount)
{
	mapChannelList_t *mapList = (mapChannelList_t*)malloc(sizeof(mapChannelList_t));
	mapList->mapChannelArray = (mapChannel_t*)malloc(sizeof(mapChannel_t)*contextCount);
	mapList->mapChannelCount = 0;
	RtlZeroMemory(mapList->mapChannelArray, sizeof(mapChannel_t)*contextCount);
	for(sint32 i=0; i<contextCount; i++)
	{
		// call constructor to init std::vectors
		new(&mapList->mapChannelArray[i]) mapChannel_t();

		sint32 f = -1;
		// find by context
		for(sint32 m=0; m<mapInfoCount; m++)
		{
			if( mapInfoArray[m].contextId == contextIdList[i] )
			{
				f = m;
				break;
			}
		}
		if( f == -1 )
		{
			printf("context %d not found in mapInfo.txt\n", contextIdList[i]);
		}
		// load all maps
		mapList->mapChannelArray[i].mapInfo = &mapInfoArray[f];
		hashTable_init(&mapList->mapChannelArray[i].ht_socketToClient, 128);
		mapList->mapChannelArray[i].timer_clientEffectUpdate = GetTickCount64();
		mapList->mapChannelArray[i].timer_missileUpdate = GetTickCount64();
		mapList->mapChannelArray[i].timer_dynObjUpdate = GetTickCount64();
		mapList->mapChannelArray[i].timer_generalTimer = GetTickCount64();
		mapList->mapChannelArray[i].timer_controller = GetTickCount64();
		mapList->mapChannelArray[i].timer_playerUpdate = GetTickCount64();
		mapList->mapChannelArray[i].playerCount = 0;
		mapList->mapChannelArray[i].playerLimit = 128;
		mapList->mapChannelArray[i].playerList = (mapChannelClient_t**)malloc(sizeof(mapChannelClient_t*)*mapList->mapChannelArray[i].playerLimit);
		Thread::InitMutex(&mapList->mapChannelArray[i].criticalSection);
		mapList->mapChannelCount++;
		// register mapChannel
		hashTable_set(&ht_mapChannelsByContextId, contextIdList[i], &mapList->mapChannelArray[i]);
	}
	// start the thread!
	//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mapChannel_worker, (LPVOID)mapList, 0, NULL);
	Thread::New(NULL, (THREAD_ROUTINE)mapChannel_worker, mapList);
}

void mapChannel_init()
{
	hashTable_init(&ht_mapChannelsByContextId, 8);
}

mapChannel_t *mapChannel_findByContextId(sint32 contextId)
{
	return (mapChannel_t*)hashTable_get(&ht_mapChannelsByContextId, contextId);
}

bool mapChannel_pass(mapChannel_t *mapChannel, clientGamemain_t *cgm)
{
	sint32 newWriteIndex = ((mapChannel->rb_playerQueueWriteIndex+1)%MAPCHANNEL_PLAYERQUEUE);
	if( newWriteIndex == mapChannel->rb_playerQueueReadIndex )
		return false; // error queue full
	mapChannel->rb_playerQueue[mapChannel->rb_playerQueueWriteIndex] = cgm;
	mapChannel->rb_playerQueueWriteIndex = newWriteIndex;
	return true;
}
