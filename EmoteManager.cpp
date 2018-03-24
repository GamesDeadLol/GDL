
#include "StdAfx.h"
#include "EmoteManager.h"
#include "WeenieObject.h"
#include "ChatMsgs.h"
#include "World.h"
#include "WeenieFactory.h"
#include "Player.h"
#include "SpellcastingManager.h"
#include "Config.h"
#include "GameEventManager.h"

EmoteManager::EmoteManager(CWeenieObject *weenie)
{
	_weenie = weenie;
}

void EmoteManager::ExecuteEmoteSet(const EmoteSet &emoteSet, DWORD target_id)
{
	if (_emoteEndTime < Timer::cur_time)
		_emoteEndTime = Timer::cur_time;

	double totalEmoteSetTime = 0.0;
	for (auto &emote : emoteSet.emotes)
	{
		totalEmoteSetTime += emote.delay;

		QueuedEmote qe;
		qe._data = emote;
		qe._target_id = target_id;
		qe._executeTime = Timer::cur_time + totalEmoteSetTime;
		_emoteQueue.push_back(qe);
	}
}

std::string EmoteManager::ReplaceEmoteText(const std::string &text, DWORD target_id, DWORD source_id)
{
	std::string result = text;

	if (result.find("%s") != std::string::npos)
	{
		std::string targetName;
		if (!g_pWorld->FindObjectName(target_id, targetName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%s", targetName));
	}

	if (result.find("%tn") != std::string::npos)
	{
		std::string targetName;
		if (!g_pWorld->FindObjectName(target_id, targetName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%tn", targetName));
	}

	if (result.find("%n") != std::string::npos)
	{
		std::string sourceName;
		if (!g_pWorld->FindObjectName(source_id, sourceName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%n", sourceName));
	}

	if (result.find("%mn") != std::string::npos)
	{
		std::string sourceName;
		if (!g_pWorld->FindObjectName(source_id, sourceName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%mn", sourceName));
	}

	if (result.find("%tqt") != std::string::npos)
	{
		while (ReplaceString(result, "%tqt", "some amount of time"));
	}

	return result;
}

void EmoteManager::ExecuteEmote(const Emote &emote, DWORD target_id)
{
	switch (emote.type)
	{
	default:
		{
#ifndef PUBLIC_BUILD
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target || !target->IsAdmin())
				break;

			target->SendText(csprintf("Unhandled emote %s (%u)", Emote::EmoteTypeToName(emote.type), emote.type), LTT_DEFAULT);
#endif
			break;
		}
	case LocalBroadcast_EmoteType:
		{
			std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

			if (!text.empty())
			{
				BinaryWriter *textMsg = ServerText(text.c_str(), LTT_DEFAULT);
				g_pWorld->BroadcastPVS(_weenie, textMsg->GetData(), textMsg->GetSize(), PRIVATE_MSG, 0, false);
				delete textMsg;
			}

			break;
		}
	case WorldBroadcast_EmoteType:
		{
			std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

			if (!text.empty())
			{
				BinaryWriter *textMsg = ServerText(text.c_str(), LTT_DEFAULT);
				g_pWorld->BroadcastGlobal(textMsg->GetData(), textMsg->GetSize(), PRIVATE_MSG, 0, false);
				delete textMsg;
			}

			break;
		}
	case Activate_EmoteType:
		{
			if (DWORD activation_target_id = _weenie->InqIIDQuality(ACTIVATION_TARGET_IID, 0))
			{
				CWeenieObject *target = g_pWorld->FindObject(activation_target_id);
				if (target)
					target->Activate(target_id);
			}

			break;
		}
	case CastSpellInstant_EmoteType:
		{
			_weenie->MakeSpellcastingManager()->CastSpellInstant(target_id, emote.spellid);
			break;
		}
	case CastSpell_EmoteType:
		{
			_weenie->MakeSpellcastingManager()->CreatureBeginCast(target_id, emote.spellid);
			break;
		}
	case AwardXP_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			long long amount = emote.amount64;

			if (g_pConfig->RewardXPMultiplier() != 1.0)
			{
				amount = (int)(amount * g_pConfig->RewardXPMultiplier());
			}

			if (amount < 0)
				amount = 0;

			target->GiveSharedXP(amount, true);
			break;
		}
	case AwardNoShareXP_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			long long amount = emote.amount64;

			if (g_pConfig->RewardXPMultiplier() != 1.0)
			{
				amount = (int)(amount * g_pConfig->RewardXPMultiplier());
			}

			if (amount < 0)
				amount = 0;

			target->GiveXP(amount, true);
			break;
		}
	case AwardSkillXP_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			target->GiveSkillXP((STypeSkill) emote.stat, emote.amount);
			break;
		}
	case AwardSkillPoints_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			target->SendText("Sorry, awarding skill points from NPC is not finished.", LTT_DEFAULT);
			break;
		}
	case AwardTrainingCredits_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			target->GiveSkillCredits(emote.amount, true);
			break;
		}
	case AwardLevelProportionalXP_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			int current_level = target->m_Qualities.GetInt(LEVEL_INT, 1);
			DWORD64 xp_to_next_level = ExperienceSystem::ExperienceToRaiseLevel(current_level, current_level + 1);
			DWORD64 xp_to_give = min((DWORD64)(xp_to_next_level * (emote.percent)), emote.max64);
			target->GiveXP(xp_to_give, true, false);
			break;
		}
	case Give_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(emote.cprof.wcid, NULL, false);			
			if (!weenie)
				break;

			weenie->SetID(g_pWorld->GenerateGUID(eDynamicGUID));

			if (!g_pWorld->CreateEntity(weenie, false))
				break;

			int error = _weenie->SimulateGiveObject(target, weenie);

			if (error != WERROR_NONE)
			{
				g_pWorld->RemoveEntity(weenie);
			}

			break;
		}
	case Motion_EmoteType:
		{
			_weenie->DoAutonomousMotion(OldToNewCommandID(emote.motion));
			break;
		}
	case ForceMotion_EmoteType:
		{
			_weenie->DoForcedMotion(OldToNewCommandID(emote.motion));
			break;
		}
	case Say_EmoteType:
		{
			std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

			if (!text.empty())
				_weenie->SpeakLocal(text.c_str(), LTT_EMOTE);

			break;
		}
	case Tell_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

				if (!text.empty())
					target->SendNetMessage(DirectChat(text.c_str(), _weenie->GetName().c_str(), _weenie->GetID(), target->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
			}

			break;
		}
	case InflictVitaePenalty_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				target->UpdateVitaePool(0);
				target->ReduceVitae(0.05f);
				target->UpdateVitaeEnchantment();
			}

			break;
		}
	case TellFellow_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

				if (text.empty())
					break;

				Fellowship *fellow = target->GetFellowship();

				if (!fellow)
				{
					target->SendNetMessage(DirectChat(text.c_str(), _weenie->GetName().c_str(), _weenie->GetID(), target->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
				}
				else
				{
					for (auto &entry : fellow->_fellowship_table)
					{
						if (CWeenieObject *member = g_pWorld->FindPlayer(entry.first))
						{
							member->SendNetMessage(DirectChat(text.c_str(), _weenie->GetName().c_str(), _weenie->GetID(), target->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
						}
					}
				}
			}

			break;
		}
	case TextDirect_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

				if (!text.empty())
					target->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
			}

			break;
		}
	case DirectBroadcast_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

				if (!text.empty())
					target->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
			}

			break;
		}
	case FellowBroadcast_EmoteType:
		{
			CPlayerWeenie *target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, _weenie->GetID());

				if (text.empty())
					break;

				Fellowship *fellow = target->GetFellowship();

				if (!fellow)
				{
					target->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
				}
				else
				{
					for (auto &entry : fellow->_fellowship_table)
					{
						if (CWeenieObject *member = g_pWorld->FindPlayer(entry.first))
						{
							member->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
						}
					}
				}
			}

			break;
		}
	case TurnToTarget_EmoteType:
		{
			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				MovementParameters params;
				_weenie->TurnToObject(target_id, &params);
			}
			break;
		}
	case Turn_EmoteType:
		{
			MovementParameters params;
			params.desired_heading = emote.frame.get_heading();
			params.speed = 1.0f;
			params.action_stamp = ++_weenie->m_wAnimSequence;
			params.modify_interpreted_state = 0;
			_weenie->last_move_was_autonomous = false;

			_weenie->cancel_moveto();
			_weenie->TurnToHeading(&params);
			break;
		}
	case TeachSpell_EmoteType:
		{
			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->LearnSpell(emote.spellid, true);
			}
			break;
		}
	case InqEvent_EmoteType:
		{
			bool success = g_pGameEventManager->IsEventStarted(emote.msg.c_str());

			PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? EventSuccess_EmoteCategory : EventFailure_EmoteCategory);
			if (!emoteCategory)
				break;

			for (auto &entry : *emoteCategory)
			{
				if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
				{
					// match
					ExecuteEmoteSet(entry, target_id);
					break;
				}
			}

			break;
		}

	case StartEvent_EmoteType:
		{
			g_pGameEventManager->StartEvent(emote.msg.c_str());
			break;
		}

	case StopEvent_EmoteType:
		{
			g_pGameEventManager->StopEvent(emote.msg.c_str());
			break;
		}

	case InqQuest_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = target->InqQuest(emote.msg.c_str());

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case InqFellowNum_EmoteType:
		{
			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->SendText("Unsupported quest logic, please report to Pea how you received this.", LTT_DEFAULT);
			}
			break;
		}
	case InqFellowQuest_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
			{
				break;
			}

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				Fellowship *fellow = target->GetFellowship();

				if (!fellow)
				{
					PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(QuestNoFellow_EmoteCategory);
					if (!emoteCategory)
						break;

					for (auto &entry : *emoteCategory)
					{
						if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
						{
							// match
							ExecuteEmoteSet(entry, target_id);
							break;
						}
					}
				}
				else
				{
					bool success = false;

					for (auto &entry : fellow->_fellowship_table)
					{
						if (CWeenieObject *member = g_pWorld->FindObject(entry.first))
						{
							success = member->InqQuest(emote.msg.c_str());

							if (success)
								break;
						}
					}

					PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory);
					if (!emoteCategory)
						break;

					for (auto &entry : *emoteCategory)
					{
						if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
						{
							// match
							ExecuteEmoteSet(entry, target_id);
							break;
						}
					}
				}
			}

			break;
		}
	case UpdateQuest_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = target->UpdateQuest(emote.msg.c_str());
					
				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case StampQuest_EmoteType:
		{
			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->StampQuest(emote.msg.c_str());
			}
			break;
		}
	case StampFellowQuest_EmoteType:
		{
			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				Fellowship *fellow = target->GetFellowship();

				if (fellow)
				{
					for (auto &entry : fellow->_fellowship_table)
					{
						if (CWeenieObject *member = g_pWorld->FindObject(entry.first))
						{
							member->StampQuest(emote.msg.c_str());
						}
					}
				}
			}

			break;
		}
	case UpdateFellowQuest_EmoteType:
		{
			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				Fellowship *fellow = target->GetFellowship();
				if (!fellow)
				{
					PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(QuestNoFellow_EmoteCategory);
					if (!emoteCategory)
						break;

					for (auto &entry : *emoteCategory)
					{
						if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
						{
							// match
							ExecuteEmoteSet(entry, target_id);
							break;
						}
					}
				}
				else
				{
					/*
					bool success = false;

					for (auto &entry : fellow->_fellowship_table)
					{
						if (CWeenieObject *member = g_pWorld->FindObject(entry.first))
						{
							success = member->UpdateQuest(emote.msg.c_str());

							if (success)
								break;
						}
					}

					PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory);
					if (!emoteCategory)
						break;

					for (auto &entry : *emoteCategory)
					{
						if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
						{
							// match
							ExecuteEmoteSet(entry, target_id);
							break;
						}
					}
					*/

					target->SendText("Unsupported quest logic, please report to Pea how you received this.", LTT_DEFAULT);

					PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(QuestFailure_EmoteCategory);
					if (!emoteCategory)
						break;

					for (auto &entry : *emoteCategory)
					{
						if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
						{
							// match
							ExecuteEmoteSet(entry, target_id);
							break;
						}
					}
				}
			}

			break;
		}
	case IncrementQuest_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->IncrementQuest(emote.msg.c_str());
			}

			break;
		}
	case DecrementQuest_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->DecrementQuest(emote.msg.c_str());
			}

			break;
		}
	case EraseQuest_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->EraseQuest(emote.msg.c_str());
			}

			break;
		}
	case Goto_EmoteType:
		{
			PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(GotoSet_EmoteCategory);
			if (!emoteCategory)
				break;

			for (auto &entry : *emoteCategory)
			{
				if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
				{
					// match
					ExecuteEmoteSet(entry, target_id);
					break;
				}
			}
			break;
		}
	case InqIntStat_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				int intStat;
				if (target->m_Qualities.InqInt((STypeInt) emote.stat, intStat))
				{
					if (intStat >= emote.min && intStat <= emote.max)
					{
						success = true;
					}
				}

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case InqSkillTrained_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				SKILL_ADVANCEMENT_CLASS sac = SKILL_ADVANCEMENT_CLASS::UNTRAINED_SKILL_ADVANCEMENT_CLASS;
				target->m_Qualities.InqSkillAdvancementClass((STypeSkill)emote.stat, sac);

				if (sac == TRAINED_SKILL_ADVANCEMENT_CLASS || sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
					success = true;

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case InqSkillSpecialized_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				SKILL_ADVANCEMENT_CLASS sac = SKILL_ADVANCEMENT_CLASS::UNTRAINED_SKILL_ADVANCEMENT_CLASS;
				target->m_Qualities.InqSkillAdvancementClass((STypeSkill)emote.stat, sac);

				if (sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
					success = true;

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case InqSkillStat_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				DWORD skillStat;
				if (target->m_Qualities.InqSkill((STypeSkill)emote.stat, skillStat, FALSE))
				{
					if (skillStat >= emote.min && skillStat <= emote.max)
					{
						success = true;
					}
				}

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case InqRawSkillStat_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				DWORD skillStat;
				if (target->m_Qualities.InqSkill((STypeSkill)emote.stat, skillStat, TRUE))
				{
					if (skillStat >= emote.min && skillStat <= emote.max)
					{
						success = true;
					}
				}

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	case InqQuestSolves_EmoteType:
		{
			if (!_weenie->m_Qualities._emote_table)
				break;

			CWeenieObject *target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				int intQuestSolves = target->InqQuestSolves(emote.msg.c_str());

				if (intQuestSolves >= emote.min && intQuestSolves <= emote.max)
				{
					success = true;
				}

				PackableList<EmoteSet> *emoteCategory = _weenie->m_Qualities._emote_table->_emote_table.lookup(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory);
				if (!emoteCategory)
					break;

				for (auto &entry : *emoteCategory)
				{
					if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
					{
						// match
						ExecuteEmoteSet(entry, target_id);
						break;
					}
				}
			}

			break;
		}
	}
}

bool EmoteManager::IsExecutingAlready()
{
	return !_emoteQueue.empty();
}

void EmoteManager::Tick()
{
	if (_emoteQueue.empty())
		return;

	for (std::list<QueuedEmote>::iterator i = _emoteQueue.begin(); i != _emoteQueue.end();)
	{
		if (i->_executeTime > Timer::cur_time || _weenie->IsBusyOrInAction() || _weenie->IsMovingTo())
			break;

		ExecuteEmote(i->_data, i->_target_id);
		i = _emoteQueue.erase(i);
		if (i != _emoteQueue.end())
			i->_executeTime = Timer::cur_time + i->_data.delay;
	}
}

void EmoteManager::Cancel()
{
	_emoteQueue.clear();
}

void EmoteManager::OnDeath(DWORD killer_id)
{
	Cancel();

	if (_weenie->m_Qualities._emote_table)
	{
		PackableList<EmoteSet> *emoteSetList = _weenie->m_Qualities._emote_table->_emote_table.lookup(Death_EmoteCategory);

		if (emoteSetList)
		{
			double dice = Random::GenFloat(0.0, 1.0);

			for (auto &emoteSet : *emoteSetList)
			{
				if (dice < emoteSet.probability)
				{
					ExecuteEmoteSet(emoteSet, killer_id);
					break;
				}

				dice -= emoteSet.probability;
			}
		}
	}
}


