
#include "StdAfx.h"

#include "Client.h"
#include "ClientEvents.h"
#include "Config.h"

// Network access
#include "Network.h"
#include "PacketController.h"
#include "BinaryReader.h"
#include "BinaryWriter.h"

// Database access
#include "Database.h"
#include "Database2.h"
#include "DatabaseIO.h"

#include "WeenieFactory.h"

// World access
#include "World.h"

// Player access
#include "WeenieObject.h"
#include "Monster.h"
#include "Player.h"

// Command access
#include "ClientCommands.h"
#include "ChatMsgs.h"
#include "AllegianceManager.h"

// CClient - for client/server interaction
CClient::CClient(SOCKADDR_IN *peer, WORD slot, AccountInformation_t &accountInfo)
{
	memcpy(&m_vars.addr, peer, sizeof(SOCKADDR_IN));

	m_AccountInfo = accountInfo;

	m_vars.slot = slot;
	m_vars.account = accountInfo.username;
	
	m_pPC = new CPacketController(this);
	m_pEvents = new CClientEvents(this);
}

CClient::~CClient()
{
	SafeDelete(m_pEvents);
	SafeDelete(m_pPC);
}

void CClient::IncomingBlob(BlobPacket_s *blob, double recvTime)
{
	if (!IsAlive())
		return;

	m_pPC->IncomingBlob(blob, recvTime);
}

void CClient::Think()
{
	if (!IsAlive())
		return;

	WorldThink();

	m_pPC->Think();

	// packet control can signal to kill
	if (!m_pPC->IsAlive())
		Kill(__FILE__, __LINE__);
}

void CClient::ThinkOutbound()
{
	if (!IsAlive())
		return;

	if (m_pPC && m_pPC - IsAlive())
	{
		m_pPC->ThinkOutbound();
	}
}

void CClient::WorldThink()
{
	if (!m_vars.bInWorld)
	{
		if (m_vars.bNeedChars)
		{
			UpdateLoginScreen();
			m_vars.bNeedChars = false;
		}
	}

	m_pEvents->Think();
}

const AccountInformation_t &CClient::GetAccountInfo()
{
	return m_AccountInfo;
}

const std::list<CharacterDesc_t> &CClient::GetCharacters()
{
	return m_Characters;
}

bool CClient::HasCharacter(DWORD character_weenie_id)
{
	for (auto &character : m_Characters)
	{
		if (character.weenie_id == character_weenie_id)
			return true;
	}

	return false;
}

DWORD CClient::IncCharacterInstanceTS(DWORD character_weenie_id)
{
	WORD newInstanceTS = 0;

	// just make this work
	m_Characters = g_pDBIO->GetCharacterList(m_AccountInfo.id);

	for (auto &character : m_Characters)
	{
		if (character.weenie_id == character_weenie_id)
		{
			newInstanceTS = character.instance_ts + 1;
			g_pDBIO->SetCharacterInstanceTS(character.weenie_id, newInstanceTS);
		}
	}

	// just make this work
	m_Characters = g_pDBIO->GetCharacterList(m_AccountInfo.id);
	return newInstanceTS;
}

void CClient::UpdateLoginScreen()
{
	m_CharactersSent = m_Characters = g_pDBIO->GetCharacterList(m_AccountInfo.id);

	BinaryWriter CharacterList;
	CharacterList.Write<DWORD>(0xF658);
	CharacterList.Write<DWORD>(0); // check what this is
	CharacterList.Write<DWORD>(m_Characters.size());

	for (auto &character : m_Characters)
	{
		CharacterList.Write<DWORD>(character.weenie_id);
		CharacterList.WriteString(character.name);
		CharacterList.Write<DWORD>(0); // delete period, TODO
	}

	CharacterList.Write<DWORD>(0);
	CharacterList.Write<DWORD>(11); // max characters I'm assuming
	CharacterList.WriteString(m_AccountInfo.username);
	CharacterList.Write<DWORD>(1); // what are these
	CharacterList.Write<DWORD>(1); // what are these
	SendNetMessage(CharacterList.GetData(), CharacterList.GetSize(), PRIVATE_MSG);

	BinaryWriter ServerName;
	ServerName.Write<DWORD>(0xF7E1);
	ServerName.Write<DWORD>(g_pWorld->GetNumPlayers()); // num connections
	ServerName.Write<DWORD>(-1); // Max connections
	ServerName.WriteString(g_pConfig->WorldName());
	SendNetMessage(ServerName.GetData(), ServerName.GetSize(), PRIVATE_MSG);

	BinaryWriter ServerUnk;
	ServerUnk.Write<DWORD>(0xF7E5);
	ServerUnk.Write<DWORD>(1); // servers region
	ServerUnk.Write<DWORD>(1); // name rule language
	ServerUnk.Write<DWORD>(1); // product id
	ServerUnk.Write<DWORD>(2); // supports languages (2)
	ServerUnk.Write<DWORD>(0); // language #1
	ServerUnk.Write<DWORD>(1); // language #2
	SendNetMessage(ServerUnk.GetData(), ServerUnk.GetSize(), EVENT_MSG);
}

void CClient::EnterWorld()
{
	DWORD EnterWorld = 0xF7DF; // 0xF7C7;

	SendNetMessage(&EnterWorld, sizeof(DWORD), 9);
	LOG(Client, Normal, "Client #%u is entering the world.\n", m_vars.slot);

	m_vars.bInWorld = TRUE;

	BinaryWriter setChatChannels;
	setChatChannels.Write<DWORD>(0x295);
	setChatChannels.Write<DWORD>(Allegiance_ChatChannel);
	setChatChannels.Write<DWORD>(General_ChatChannel);
	setChatChannels.Write<DWORD>(Trade_ChatChannel);
	setChatChannels.Write<DWORD>(LFG_ChatChannel);
	setChatChannels.Write<DWORD>(Roleplay_ChatChannel);
	setChatChannels.Write<DWORD>(Olthoi_ChatChannel);
	setChatChannels.Write<DWORD>(Society_ChatChannel);
	setChatChannels.Write<DWORD>(SocietyCelHan_ChatChannel);
	setChatChannels.Write<DWORD>(SocietyEldWeb_ChatChannel);
	setChatChannels.Write<DWORD>(SocietyRadBlo_ChatChannel);
	SendNetMessage(&setChatChannels, PRIVATE_MSG, FALSE, FALSE);
}

void CClient::ExitWorld()
{
	DWORD ExitWorld = 0xF653;
	SendNetMessage(&ExitWorld, sizeof(DWORD), PRIVATE_MSG);
	LOG(Client, Normal, "Client #%u is exiting the world.\n", m_vars.slot);

	m_pPC->ResetEvent();

	UpdateLoginScreen();

	m_vars.bInWorld = FALSE;
}

void CClient::SendNetMessage(BinaryWriter* pMessage, WORD group, BOOL event, BOOL del)
{
	if (!pMessage)
		return;

	SendNetMessage(pMessage->GetData(), pMessage->GetSize(), group, event);

	if (del)
		delete pMessage;
}
void CClient::SendNetMessage(void *data, DWORD length, WORD group, BOOL game_event)
{
	if (!IsAlive())
		return;

	if (!data || !length)
		return;

	if (g_bDebugToggle)
	{
		LOG(Network, Normal, "%.03f Sending response (group %u) to %s:\n", g_pGlobals->Time(), group, (GetEvents() && GetEvents()->GetPlayer()) ? GetEvents()->GetPlayer()->GetName().c_str() : "[unknown]");
		LOG_BYTES(Network, Normal, data, length);
	}

	m_pPC->QueueNetMessage(data, length, group, game_event ? GetEvents()->GetPlayerID() : 0);
}

BOOL CClient::CheckNameValidity(const char *name, int access, std::string &resultName)
{
	resultName = "";

	if (!strncmp(name, "Contributor ", strlen("Contributor ")))
	{
		if (access >= ADVOCATE_ACCESS)
		{
			resultName = "+Contributor ";
			name += strlen("Contributor ");
		}
		else
			return FALSE;
	}

	int len = (int)strlen(name);

	if ((len < 3) || (len > 32))
		return FALSE;

	int i = 0;
	while (i < len)
	{
		char letter = name[i];
		if (!(letter >= 'A' && letter <= 'Z') && !(letter >= 'a' || letter <= 'z') && !(letter == '\'') && !(letter == ' ') && !(letter == '-'))
			break;
		i++;
	}
	if (i == len)
	{
		resultName += name;
		return TRUE;
	}

	return FALSE;
}

void CClient::DeleteCharacter(BinaryReader *pReader)
{
	std::string account_name = pReader->ReadString(); // account
	if (strcmp(account_name.c_str(), m_vars.account.c_str()))
		return;

	int slot = pReader->Read<int>();
	if (slot < 0 || slot >= m_Characters.size() || slot >= m_CharactersSent.size())
		return; // out of range

	auto entry = m_CharactersSent.begin();
	std::advance(entry, slot);

	g_pAllegianceManager->BreakAllAllegiance(entry->weenie_id);
	g_pDBIO->DeleteCharacter(entry->weenie_id);
	g_pDBIO->DeleteWeenie(entry->weenie_id);
	m_CharactersSent.erase(entry);

	BinaryWriter response;
	response.Write<DWORD>(0xF655);
	SendNetMessage(response.GetData(), response.GetSize(), PRIVATE_MSG);
}

void CClient::CreateCharacter(BinaryReader *pReader)
{
	DWORD errorCode = CG_VERIFICATION_RESPONSE_CORRUPT;

	std::string account_name = pReader->ReadString();
	if (strcmp(account_name.c_str(), m_vars.account.c_str()))
		return;

	ACCharGenResult cg;
	cg.UnPack(pReader);

	std::string resultName;

	if (pReader->GetLastError())
		goto BadData;
	
	cg.strength = max(min(cg.strength, 100), 10);
	cg.endurance = max(min(cg.endurance, 100), 10);
	cg.coordination = max(min(cg.coordination, 100), 10);
	cg.quickness = max(min(cg.quickness, 100), 10);
	cg.focus = max(min(cg.focus, 100), 10);
	cg.self = max(min(cg.self, 100), 10);
	
	int totalAttribs = cg.strength + cg.endurance + cg.coordination + cg.quickness + cg.focus + cg.self;
	if (totalAttribs > 330)
	{
		goto BadData;
	}

	if (cg.heritageGroup <= 0 || cg.heritageGroup >= 14)
	{
		goto BadData;
	}

	if (cg.gender < 0 || cg.gender >= 3) // allow invalid gender? not sure
	{
		goto BadData;
	}

	SkillTable *pSkillTable = SkillSystem::GetSkillTable();
	int numCreditsUsed = 0;
	cg.numSkills = max(0, min(100, cg.numSkills));

	for (DWORD i = 0; i < cg.numSkills; i++)
	{
		switch (cg.skillAdvancementClasses[i])
		{
		case SKILL_ADVANCEMENT_CLASS::UNDEF_SKILL_ADVANCEMENT_CLASS:
		case SKILL_ADVANCEMENT_CLASS::UNTRAINED_SKILL_ADVANCEMENT_CLASS:
		case SKILL_ADVANCEMENT_CLASS::TRAINED_SKILL_ADVANCEMENT_CLASS:
		case SKILL_ADVANCEMENT_CLASS::SPECIALIZED_SKILL_ADVANCEMENT_CLASS:
			break;
			
		default:
			goto BadData;
		}

		const SkillBase *pSkillBase = pSkillTable->GetSkillBaseRaw((STypeSkill) i);
		if (pSkillBase != NULL)
		{
			if (cg.skillAdvancementClasses[i] == SKILL_ADVANCEMENT_CLASS::TRAINED_SKILL_ADVANCEMENT_CLASS)
			{
				numCreditsUsed += pSkillBase->_trained_cost;
			}
			else if (cg.skillAdvancementClasses[i] == SKILL_ADVANCEMENT_CLASS::SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
			{
				numCreditsUsed += pSkillBase->_specialized_cost;
			}
		}
		else
		{
			if (cg.skillAdvancementClasses[i] == SKILL_ADVANCEMENT_CLASS::TRAINED_SKILL_ADVANCEMENT_CLASS ||
				cg.skillAdvancementClasses[i] == SKILL_ADVANCEMENT_CLASS::SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
			{
				goto BadData;
			}
		}
	}

	if (numCreditsUsed > 56)
	{
		goto BadData;
	}

	// need to reformat name here...
	if (!CheckNameValidity(cg.name.c_str(), GetAccessLevel(), resultName))
	{
		errorCode = CG_VERIFICATION_RESPONSE_NAME_BANNED;
		goto BadData;
	}

	// should check variables to make sure everythings within restriction

	// wHairTextures[m_wGender][m_wHairStyle];
	// WORD wHairTextures[2][4] = { { 0x10B8, 0x10B8, 0x10B8, 0x10B7 }, { 0x11FD, 0x11FD, 0x11FD, 0x10B7 } };

	// group 4, 0x0000F643, 0x00000001, <guid>, <char name[str]>, 0x00000000
	{
		// check if name exists
		if (g_pDBIO->IsCharacterNameOpen(resultName.c_str()))
		{
			const int MIN_PLAYER_GUID = 0x50000000;
			const int MAX_PLAYER_GUID = 0x6FFFFFFF;

			unsigned int newCharacterGUID = g_pDBIO->GetHighestWeenieID(MIN_PLAYER_GUID, MAX_PLAYER_GUID) + 1;

			if (g_pDBIO->CreateCharacter(m_AccountInfo.id, newCharacterGUID, resultName.c_str()))
			{
				m_Characters = g_pDBIO->GetCharacterList(m_AccountInfo.id);
				
				// start pReader Zaikhil
				Position startPos;
				// startPos = Position(0x7D64001D, Vector(74.748657f, 97.934601f, 12.004999f), Quaternion(0.926898f, 0.000000f, 0.000000f, -0.375314f)); // zaikhil
				// startPos = Position(0x9722003A, Vector(168.354004f, 24.618000f, 102.005005f), Quaternion(-0.922790f, 0.000000f, 0.000000f, -0.385302f));
				startPos = Position(0xD3380005, Vector(5.500000f, 109.800003f, 168.463333f), Quaternion(1.000000f, 0.000000, 0.000000, 0.000000));

				// WCID "1" is always a player weenie
				CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(1, &startPos, false);

				weenie->SetID(newCharacterGUID);

				// alter parameters for character creation here
				weenie->SetName(resultName.c_str());

				weenie->m_Qualities.SetInt(HERITAGE_GROUP_INT, cg.heritageGroup);
				weenie->m_Qualities.SetInt(GENDER_INT, cg.gender);
				weenie->m_Qualities.SetAttribute(STRENGTH_ATTRIBUTE, cg.strength);
				weenie->m_Qualities.SetAttribute(ENDURANCE_ATTRIBUTE, cg.endurance);
				weenie->m_Qualities.SetAttribute(COORDINATION_ATTRIBUTE, cg.coordination);
				weenie->m_Qualities.SetAttribute(QUICKNESS_ATTRIBUTE, cg.quickness);
				weenie->m_Qualities.SetAttribute(FOCUS_ATTRIBUTE, cg.focus);
				weenie->m_Qualities.SetAttribute(SELF_ATTRIBUTE, cg.self);

				ACCharGenData *cgd = CachedCharGenData;
				assert(cgd != NULL);

				HeritageGroup_CG *heritageGroup = cgd->mHeritageGroupList.lookup(cg.heritageGroup);
				if (heritageGroup)
				{
					weenie->m_Qualities.SetString(HERITAGE_GROUP_STRING, heritageGroup->name.c_str());

					if (cg.templateNum >= 0 && cg.templateNum < heritageGroup->mTemplateList.num_used)
						weenie->m_Qualities.SetString(TEMPLATE_STRING, heritageGroup->mTemplateList.array_data[cg.templateNum].name.c_str());

					Sex_CG *scg = heritageGroup->mGenderList.lookup(cg.gender);
					if (scg)
					{
						weenie->m_Qualities.SetDataID(PALETTE_BASE_DID, scg->basePalette);
						weenie->m_Qualities.SetDataID(ICON_DID, scg->iconImage);
						weenie->m_Qualities.SetDataID(SOUND_TABLE_DID, scg->soundTable);
						weenie->m_Qualities.SetDataID(MOTION_TABLE_DID, scg->motionTable);
						weenie->m_Qualities.SetDataID(COMBAT_TABLE_DID, scg->combatTable);
						weenie->m_Qualities.SetDataID(PHYSICS_EFFECT_TABLE_DID, scg->physicsTable);
						weenie->m_Qualities.SetDataID(SETUP_DID, scg->setup);
						//weenie->m_ObjDesc = scg->objDesc;
						//weenie->m_ObjDesc.paletteID = scg->basePalette;

						cg.hairStyle = max(0, min(scg->mHairStyleList.num_used - 1, cg.hairStyle));
						HairStyle_CG *hairStyle = &scg->mHairStyleList.array_data[cg.hairStyle];
						//weenie->m_ObjDesc += hairStyle->objDesc;

						if (hairStyle->objDesc.firstAPChange)
							weenie->m_Qualities.SetDataID(HEAD_OBJECT_DID, hairStyle->objDesc.firstAPChange->part_id);

						if (cg.eyesStrip != -1)
						{
							cg.eyesStrip = max(0, min(scg->mEyeStripList.num_used - 1, cg.eyesStrip));
							EyesStrip_CG *eyesStrip = &scg->mEyeStripList.array_data[cg.eyesStrip];
							if (eyesStrip)
							{
								if (hairStyle->bald)
								{
									//weenie->m_ObjDesc += eyesStrip->objDesc_Bald;
									weenie->m_Qualities.SetDataID(DEFAULT_EYES_TEXTURE_DID, eyesStrip->objDesc_Bald.firstTMChange->old_tex_id);
									weenie->m_Qualities.SetDataID(EYES_TEXTURE_DID, eyesStrip->objDesc_Bald.firstTMChange->new_tex_id);
								}
								else
								{
									//weenie->m_ObjDesc += eyesStrip->objDesc;
									weenie->m_Qualities.SetDataID(DEFAULT_EYES_TEXTURE_DID, eyesStrip->objDesc.firstTMChange->old_tex_id);
									weenie->m_Qualities.SetDataID(EYES_TEXTURE_DID, eyesStrip->objDesc.firstTMChange->new_tex_id);
								}
							}
						}
						if (cg.noseStrip != -1)
						{
							cg.noseStrip = max(0, min(scg->mNoseStripList.num_used - 1, cg.noseStrip));
							FaceStrip_CG *faceStrip = &scg->mNoseStripList.array_data[cg.noseStrip];
							if (faceStrip)
							{
								//weenie->m_ObjDesc += faceStrip->objDesc;
								weenie->m_Qualities.SetDataID(DEFAULT_NOSE_TEXTURE_DID, faceStrip->objDesc.firstTMChange->old_tex_id);
								weenie->m_Qualities.SetDataID(NOSE_TEXTURE_DID, faceStrip->objDesc.firstTMChange->new_tex_id);
							}
						}
						if (cg.mouthStrip != -1)
						{
							cg.mouthStrip = max(0, min(scg->mMouthStripList.num_used - 1, cg.mouthStrip));
							FaceStrip_CG *faceStrip = &scg->mMouthStripList.array_data[cg.mouthStrip];
							if (faceStrip)
							{
								// weenie->m_ObjDesc += faceStrip->objDesc;
								weenie->m_Qualities.SetDataID(DEFAULT_MOUTH_TEXTURE_DID, faceStrip->objDesc.firstTMChange->old_tex_id);
								weenie->m_Qualities.SetDataID(MOUTH_TEXTURE_DID, faceStrip->objDesc.firstTMChange->new_tex_id);
							}
						}
						
						PalSet *ps;
						if (ps = PalSet::Get(scg->skinPalSet))
						{
							DWORD skinPalette = ps->GetPaletteID(cg.skinShade);
							weenie->m_Qualities.SetDataID(SKIN_PALETTE_DID, skinPalette);
							//weenie->m_ObjDesc.AddSubpalette(new Subpalette(skinPalette, 0 << 3, 0x18 << 3));
							PalSet::Release(ps);
						}

						cg.hairColor = max(0, min(scg->mHairColorList.num_used - 1, cg.hairColor));
						if (ps = PalSet::Get(scg->mHairColorList.array_data[cg.hairColor]))
						{
							DWORD hairPalette = ps->GetPaletteID(cg.hairShade);
							weenie->m_Qualities.SetDataID(HAIR_PALETTE_DID, hairPalette);
							//weenie->m_ObjDesc.AddSubpalette(new Subpalette(hairPalette, 0x18 << 3, 0x8 << 3));
							PalSet::Release(ps);
						}					
												
						if (cg.eyeColor != -1)
						{
							cg.eyeColor = max(0, min(scg->mEyeColorList.num_used - 1, cg.eyeColor));
							DWORD eyesPalette = scg->mEyeColorList.array_data[cg.eyeColor];
							weenie->m_Qualities.SetDataID(EYES_PALETTE_DID, eyesPalette);
							//weenie->m_ObjDesc.AddSubpalette(new Subpalette(eyesPalette, 0x20 << 3, 0x8 << 3));
						}

						/*
						EYES_TEXTURE_DID,
						NOSE_TEXTURE_DID,
						MOUTH_TEXTURE_DID,
						DEFAULT_EYES_TEXTURE_DID,
						DEFAULT_NOSE_TEXTURE_DID,
						DEFAULT_MOUTH_TEXTURE_DID,
						HAIR_PALETTE_DID,
						EYES_PALETTE_DID,
						SKIN_PALETTE_DID,
						HEAD_OBJECT_DID,
						*/
					}
				}

				for (DWORD i = 0; i < cg.numSkills; i++)
				{
					weenie->m_Qualities.SetSkillAdvancementClass((STypeSkill)i, cg.skillAdvancementClasses[i]);
				}
				
				weenie->m_Qualities.SetInt(AGE_INT, 0);
				
				weenie->m_Qualities.SetInt(COIN_VALUE_INT, GetAccessLevel() >= ADMIN_ACCESS ? 500000000 : 10000);

				/*
				cg.startArea = max(0, min(cgd->mStartAreaList.num_used - 1, cg.startArea));

				assert(cgd->mStartAreaList.array_data[cg.startArea].mPositionList.num_used >= 1);

				startPos = cgd->mStartAreaList.array_data[cg.startArea].mPositionList.array_data[0];
				weenie->SetInitialPosition(startPos);
				*/
				
				startPos = Position(0xA9B00006, Vector(24.258204f, 123.777000f, 63.060749f), Quaternion(1, 0, 0, 0)); // holtburg

				switch (cg.startArea)
				{
				default:
				case 0:
					// holtburg
					startPos = Position(g_pConfig->HoltburgStartPosition());

					if (!startPos.objcell_id)
					{
						startPos = Position(0xA9B00006, Vector(24.258204f, 123.777000f, 63.060749f), Quaternion(1, 0, 0, 0));
					}

					break;

				case 1:
					// shoushi?
					startPos = Position(g_pConfig->ShoushiStartPosition());

					if (!startPos.objcell_id)
					{
						startPos = Position(0xDE51000C, Vector(26.712753f, 89.279999f, 17.778936f), Quaternion(0.931082f, 0.0, 0.0, -0.364811f));
					}

					break;

				case 2:
					// yaraq?
					startPos = Position(g_pConfig->YaraqStartPosition());

					if (!startPos.objcell_id)
					{
						startPos = Position(0x7D680019, Vector(79.102280f, 19.573767f, 12.821287f), Quaternion(-0.656578f, 0.0, 0.0, 0.754258f));
					}

					break;

				case 3:
					// sanamar???
					startPos = Position(g_pConfig->SanamarStartPosition());

					if (!startPos.objcell_id)
					{
						startPos = Position(0xA9B00006, Vector(24.258204f, 123.777000f, 63.060749f), Quaternion(1, 0, 0, 0));
					}

					break;
				}

				weenie->SetInitialPosition(startPos);
				weenie->m_Qualities.SetPosition(SANCTUARY_POSITION, startPos);
				weenie->m_Qualities.SetInt(AVAILABLE_SKILL_CREDITS_INT, 56 - max(0, min(56, numCreditsUsed)));
				weenie->m_Qualities.SetInt(CONTAINERS_CAPACITY_INT, 7);

				weenie->SetMaxVitals(false);

				weenie->Save();

				delete weenie;

				BinaryWriter response;
				response.Write<DWORD>(0xF643);
				response.Write<DWORD>(1);
				response.Write<DWORD>(newCharacterGUID);
				response.WriteString(cg.name.c_str());
				response.Write<DWORD>(0);
				SendNetMessage(response.GetData(), response.GetSize(), PRIVATE_MSG);
			}
			else
			{
				LOG(Client, Error, "Failed to create character.\n");
				BinaryWriter response;
				response.Write<DWORD>(0xF643);
				response.Write<DWORD>(CG_VERIFICATION_RESPONSE_DATABASE_DOWN); // update this error number
				SendNetMessage(response.GetData(), response.GetSize(), PRIVATE_MSG);
			}
		}
		else
		{		
			LOG(Client, Normal, "Character name already exists.\n");
			BinaryWriter response;
			response.Write<DWORD>(0xF643);
			response.Write<DWORD>(CG_VERIFICATION_RESPONSE_NAME_IN_USE); // name already exists
			SendNetMessage(response.GetData(), response.GetSize(), PRIVATE_MSG);
		}

	}
	return;

BadData:
	{
		BinaryWriter response;
		response.Write<DWORD>(0xF643);
		response.Write<DWORD>(errorCode);
		SendNetMessage(response.GetData(), response.GetSize(), PRIVATE_MSG);
	}
}

void CClient::SendLandblock(DWORD dwFileID)
{
	TURBINEFILE* pLandData = g_pCell->GetFile(dwFileID);
	if (!pLandData)
	{
		if (m_pEvents)
		{
			m_pEvents->SendText(csprintf("Your client is requesting cell data (0x%08X) that this server does not have!", dwFileID), LTT_DEFAULT);
			m_pEvents->SendText("If you are stuck in portal mode, type /render radius 5 to escape. The server administrator should reconfigure the server with a FULL cell.dat file!", LTT_DEFAULT);
		}
		return;
	}

	if ((dwFileID & 0xFFFF) != 0xFFFF)
	{
		LOG(Client, Warning, "Client requested Landblock 0x%08X - should end pReader 0xFFFF\n", dwFileID);
	}

	if (pLandData)
	{
		BinaryWriter BlockPackage;

		BlockPackage.Write<DWORD>(0xF7E2);

		DWORD dwFileSize = pLandData->GetLength();
		BYTE* pbFileData = pLandData->GetData();

		DWORD dwPackageSize = (DWORD)((dwFileSize * 1.02f) + 12 + 1);
		BYTE* pbPackageData = new BYTE[dwPackageSize];

		if (Z_OK != compress2(pbPackageData, &dwPackageSize, pbFileData, dwFileSize, Z_BEST_COMPRESSION))
		{
			LOG(Client, Error, "Error compressing LandBlock package!\n");
		}

		BlockPackage.Write<DWORD>(1);
		BlockPackage.Write<DWORD>(2);
		BlockPackage.Write<DWORD>(1);
		BlockPackage.Write<DWORD>(dwFileID);
		BlockPackage.Write<DWORD>(1);
		BlockPackage.Write<BYTE>(1); // Compressed
		BlockPackage.Write<DWORD>(2);
		BlockPackage.Write<DWORD>(dwPackageSize + sizeof(DWORD) * 2);
		BlockPackage.Write<DWORD>(dwFileSize);
		BlockPackage.Write(pbPackageData, dwPackageSize);
		BlockPackage.Align();

		delete[] pbPackageData;
		delete pLandData;

		SendNetMessage(BlockPackage.GetData(), BlockPackage.GetSize(), EVENT_MSG, FALSE);
	}
}


void CClient::SendLandblockInfo(DWORD dwFileID)
{
	if ((dwFileID & 0xFFFF) != 0xFFFE)
	{
		LOG(Client, Warning, "Client requested LandblockInfo 0x%08X - should end pReader 0xFFFE\n", dwFileID);
		return;
	}

	TURBINEFILE *pObjData = g_pCell->GetFile(dwFileID);
	if (!pObjData)
	{
		return;
	}

	if (pObjData)
	{
		BinaryWriter BlockInfoPackage;
		BlockInfoPackage.Write<DWORD>(0xF7E2);

		DWORD dwFileSize = pObjData->GetLength();
		BYTE* pbFileData = pObjData->GetData();

		DWORD dwPackageSize = (DWORD)((dwFileSize * 1.02f) + 12 + 1);
		BYTE* pbPackageData = new BYTE[dwPackageSize];

		if (Z_OK != compress2(pbPackageData, &dwPackageSize, pbFileData, dwFileSize, Z_BEST_COMPRESSION))
		{
			LOG(Client, Error, "Error compressing LandBlockInfo package!\n");
		}

		BlockInfoPackage.Write<DWORD>(1);
		BlockInfoPackage.Write<DWORD>(2);
		BlockInfoPackage.Write<DWORD>(2); // 1 for 0xFFFF, 2 for 0xFFFE
		BlockInfoPackage.Write<DWORD>(dwFileID);
		BlockInfoPackage.Write<DWORD>(1);
		BlockInfoPackage.Write<BYTE>(1);
		BlockInfoPackage.Write<DWORD>(2);
		BlockInfoPackage.Write<DWORD>(dwPackageSize + sizeof(DWORD) * 2);
		BlockInfoPackage.Write<DWORD>(dwFileSize);
		BlockInfoPackage.Write(pbPackageData, dwPackageSize);
		BlockInfoPackage.Align();

		delete[] pbPackageData;
		delete pObjData;

		SendNetMessage(BlockInfoPackage.GetData(), BlockInfoPackage.GetSize(), EVENT_MSG, FALSE);
	}
}

void CClient::SendLandcell(DWORD dwFileID)
{
	TURBINEFILE* pCellData = g_pCell->GetFile(dwFileID);
	if (!pCellData)
	{
		if (m_pEvents)
		{
			m_pEvents->SendText(csprintf("Your client is requesting cell data (#%08X) that this server does not have!", dwFileID), LTT_DEFAULT);
			m_pEvents->SendText("If you are stuck in portal mode, type /render radius 5 to escape. The server administrator should reconfigure the server with a FULL cell.dat file!", LTT_DEFAULT);
		}
		return;
	}

	if (pCellData)
	{
		BinaryWriter CellPackage;

		CellPackage.Write<DWORD>(0xF7E2);

		DWORD dwFileSize = pCellData->GetLength();
		BYTE* pbFileData = pCellData->GetData();

		DWORD dwPackageSize = (DWORD)((dwFileSize * 1.02f) + 12 + 1);
		BYTE* pbPackageData = new BYTE[dwPackageSize];

		if (Z_OK != compress2(pbPackageData, &dwPackageSize, pbFileData, dwFileSize, Z_BEST_COMPRESSION))
		{
			// These are CEnvCell if I recall correctly
			LOG(Client, Error, "Error compressing landcell package!\n");
		}

		CellPackage.Write<DWORD>(1);
		CellPackage.Write<DWORD>(2);
		CellPackage.Write<DWORD>(3);
		CellPackage.Write<DWORD>(dwFileID);
		CellPackage.Write<DWORD>(1);
		CellPackage.Write<BYTE>(1);
		CellPackage.Write<DWORD>(2);
		CellPackage.Write<DWORD>(dwPackageSize + sizeof(DWORD) * 2);
		CellPackage.Write<DWORD>(dwFileSize);
		CellPackage.Write(pbPackageData, dwPackageSize);
		CellPackage.Align();

		delete[] pbPackageData;
		delete pCellData;

		//LOG(Temp, Normal, "Sent cell %08X ..\n", dwFileID);

		SendNetMessage(CellPackage.GetData(), CellPackage.GetSize(), EVENT_MSG, FALSE);
	}

	//if (m_pEvents)
	//	m_pEvents->SendText(csprintf("The server has sent you cell #%04X!", dwFileID >> 16), LTT_DEFAULT);
}

void CClient::ProcessMessage(BYTE *data, DWORD length, WORD group)
{
	if (g_bDebugToggle)
	{
		LOG(Network, Normal, "%.03f Received response (group %u):\n", g_pGlobals->Time(), group);
		LOG_BYTES(Network, Normal, data, length);
	}

	BinaryReader in(data, length);

	DWORD dwMessageCode = in.ReadDWORD();

#ifdef _DEBUG
	// LOG(Client, Normal, "Processing response 0x%X (size %d):\n", dwMessageCode, length);
#endif

	if (in.GetLastError())
	{
		LOG(Client, Warning, "Error processing response.\n");
		return;
	}
	switch (dwMessageCode)
	{
		case 0xF653:
		{
			if (m_vars.bInWorld)
			{
				m_pEvents->BeginLogout();
			}

			break;
		}

		case 0xF655: // Delete Character
			if (!m_vars.bInWorld)
			{
				DeleteCharacter(&in);
			}

			break;

		case 0xF656: // Create Character
		{
			if (!m_vars.bInWorld)
			{
				CreateCharacter(&in);
			}

			break;
		}

		case 0xF6EA: // Request Object
		{
			if (m_vars.bInWorld)
			{
				DWORD dwEID = in.ReadDWORD();
				if (in.GetLastError()) break;

				CPlayerWeenie *pPlayer;

				if ((m_pEvents) && (pPlayer = m_pEvents->GetPlayer()))
				{
					CWeenieObject *pTarget = g_pWorld->FindWithinPVS(pPlayer, dwEID);
									
					if (pTarget)
						pPlayer->MakeAware(pTarget);
					else
					{
						pTarget = g_pWorld->FindObject(dwEID);

						if (pTarget)
						{
							// LOG(Temp, Normal, "Player %s is requesting info on %s it shouldn't know about.\n", pPlayer->GetName().c_str(), pTarget->GetName().c_str());
						}
					}
				}
			}
			break;
		}

		case 0xF7E6:
		{
			BinaryWriter ServerUnk2;
			ServerUnk2.Write<long>(0xF7EA);
			SendNetMessage(ServerUnk2.GetData(), ServerUnk2.GetSize(), 5);
			break;
		}

		case 0xF7EA:
		{
			//BinaryWriter EndDDD;
			//EndDDD.Write<DWORD>(0xF7EA);
			//SendNetMessage(EndDDD.GetData(), EndDDD.GetSize(), EVENT_MSG);

			break;
		}

		case 0xF7E3: // Request File Data was 0xF7A9
		{
			// This doesn't work for some reason.
			if (m_vars.bInWorld)
			{
				DWORD dwFileClass = in.ReadDWORD();
				DWORD dwFileID = in.ReadDWORD();				
				if (in.GetLastError()) break;

				switch (dwFileClass)
				{
				default:
					LOG(Client, Warning, "Unknown download request: %08X %d\n", dwFileID, dwFileClass);
					break;

				case 1:
					SendLandblock(dwFileID); // 1 is landblock 0xFFFF
					SendLandblockInfo((dwFileID & 0xFFFF0000) | 0xFFFE);
					break;

				case 2:
					SendLandblockInfo(dwFileID); // 2 is landblock environemnt 0xFFFE
					break;

				case 3:
					SendLandcell(dwFileID); // 0x100+
					break;
				}
			}

			break;
		}

		case 0xF7B1:
		{
			//should check the sequence
			if (m_vars.bInWorld)
			{
				m_pEvents->ProcessEvent(&in);
			}

			break;
		}
		case 0xF7C8:
		{
			if (!m_vars.bInWorld)
			{
				EnterWorld();
			}

			break;
		}

		case 0xF657:
		{
			if (m_vars.bInWorld)
			{
				DWORD dwGUID = in.ReadDWORD();
				char* szName = in.ReadString();
				if (in.GetLastError()) break;

				m_pEvents->LoginCharacter(dwGUID, /*szName*/GetAccount());
			}

			break;
		}

		// modified cmoski code
		case 0xF7DE:
		{ //Gets data from /general here to rebroadcast as a world message below \o/
			DWORD size = in.ReadDWORD();
			DWORD TurbineChatType = in.ReadDWORD(); //0x1 Inbound, 0x3 Outbound, 0x5 Outbound Ack
			DWORD unk1 = in.ReadDWORD();
			DWORD unk2 = in.ReadDWORD();
			DWORD unk3 = in.ReadDWORD();
			DWORD unk4 = in.ReadDWORD();
			DWORD unk5 = in.ReadDWORD();
			DWORD unk6 = in.ReadDWORD();
			DWORD payload = in.ReadDWORD();

			if (in.GetLastError())
				break;

			if (TurbineChatType == 0x3) //0x3 Outbound
			{
				if (!m_pEvents->GetPlayer() || !m_pEvents->CheckForChatSpam())
					break;
				
				DWORD serial = in.ReadDWORD(); // serial of this character's chat
				DWORD channel_unk = in.ReadDWORD();
				DWORD channel_unk2 = in.ReadDWORD();
				DWORD listening_channel = in.ReadDWORD(); // ListeningChannel in SetTurbineChatChannels (0x000BEEF0-9)
				std::string message = in.ReadWStringToString();
				DWORD payloadSize = in.ReadDWORD();
				DWORD playerGUID = in.ReadDWORD();
				DWORD ob_unknown = in.ReadDWORD(); //Always 0?
				DWORD ob_unknown2 = in.ReadDWORD();

				std::string filteredText = FilterBadChatCharacters(message.c_str());

				if (in.GetLastError() || !filteredText.size())
					break;

				BinaryWriter chatAck;
				chatAck.Write<DWORD>(0xF7DE);
				chatAck.Write<DWORD>(0x30); // Size
				chatAck.Write<DWORD>(0x05); // Type 05 - ACK
				chatAck.Write<DWORD>(0x02); // ??
				chatAck.Write<DWORD>(0x01); // ??
				chatAck.Write<DWORD>(0xB0045); // bitfield?
				chatAck.Write<DWORD>(0x01);
				chatAck.Write<DWORD>(0xB0045); // bitfield?
				chatAck.Write<DWORD>(0x00);
				chatAck.Write<DWORD>(0x10);   // Payload size
				chatAck.Write<DWORD>(serial); // serial
				chatAck.Write<DWORD>(0x02);
				chatAck.Write<DWORD>(0x02);
				chatAck.Write<DWORD>(0x00);
				SendNetMessage(&chatAck, 0x04, FALSE, FALSE);

				if (filteredText.c_str()[0] == '!' || filteredText.c_str()[0] == '@' || filteredText.c_str()[0] == '/')
				{
					CommandBase::Execute((char*)filteredText.erase(0, 1).c_str(), m_pEvents->GetPlayer()->GetClient());
				}
				else
				{
					switch (listening_channel)
					{
					case General_ChatChannel:
					case Trade_ChatChannel:
					case LFG_ChatChannel:
					case Roleplay_ChatChannel:
					case Allegiance_ChatChannel:
					// case Olthoi_ChatChannel:
					// case Society_ChatChannel:
						g_pWorld->BroadcastChatChannel(listening_channel, m_pEvents->GetPlayer(), filteredText);
						break;
					}
				}
			}

			break;
		}
		default:
#ifdef _DEBUG
			LOG(Client, Warning, "Unhandled response %08X from the client.\n", dwMessageCode);
#endif
			break;
	}

	//if ( dwMessageCode != 0xF7B1 )
	//	LOG(Temp, Normal, "Received response %04X\n", dwMessageCode);
}

BOOL CClient::CheckAccount(const char* cmp)
{
	return !_stricmp(m_vars.account.c_str(), cmp);
}

int CClient::GetAccessLevel()
{
	return m_AccountInfo.access;
}

void CClient::SetAccessLevel(unsigned int access)
{
	m_AccountInfo.access = access;
}

BOOL CClient::CheckAddress(SOCKADDR_IN *peer)
{
	return !memcmp(GetHostAddress(), peer, sizeof(SOCKADDR_IN));
}

WORD CClient::GetSlot()
{
	return m_vars.slot;
}

const char *CClient::GetAccount()
{
	return m_vars.account.c_str();
}

const char *CClient::GetDescription()
{
	// Must lead with index or the kick/ban feature won't function.
	return csprintf("#%u %s \"%s\"", m_vars.slot, inet_ntoa(m_vars.addr.sin_addr), m_vars.account.c_str());
}

void CClient::SetLoginData(DWORD dwUnixTime, DWORD dwPortalStamp, DWORD dwCellStamp)
{
	m_vars.fLoginTime = g_pGlobals->Time();

	m_vars.ClientLoginUnixTime = dwUnixTime;
	m_vars.PortalStamp = dwPortalStamp;
	m_vars.CellStamp = dwCellStamp;

	m_vars.bInitDats = FALSE;
}

SOCKADDR_IN *CClient::GetHostAddress()
{
	return &m_vars.addr;
}
