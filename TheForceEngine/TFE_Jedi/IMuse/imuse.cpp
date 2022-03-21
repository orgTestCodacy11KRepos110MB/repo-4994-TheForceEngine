#include "imuse.h"
#include <TFE_Audio/midi.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>
#include "imTrigger.h"
#include "imSoundFader.h"
#include "midiData.h"

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImMidiPlayer;
	struct SoundChannel;
	struct ImMidiChannel;

	struct ImMidiChannel
	{
		ImMidiPlayer* player;
		SoundChannel* channel;
		ImMidiChannel* sharedMidiChannel;
		s32 channelId;

		s32 pgm;
		s32 priority;
		s32 noteReq;
		s32 volume;

		s32 pan;
		s32 modulation;
		s32 finalPan;
		s32 sustain;

		s32* instrumentMask;
		s32* instrumentMask2;
		ImMidiPlayer* sharedPart;

		s32 u3c;
		s32 u40;
		s32 sharedPartChannelId;
	};

	struct SoundChannel
	{
		ImMidiChannel* data;
		s32 partStatus;
		s32 partPgm;
		s32 partTrim;
		s32 partPriority;
		s32 priority;
		s32 partNoteReq;
		s32 partVolume;
		s32 groupVolume;
		s32 partPan;
		s32 modulation;
		s32 sustain;
		s32 pitchBend;
		s32 outChannelCount;
		s32 pan;
	};

	struct PlayerData
	{
		ImMidiPlayer* player;
		ImSoundId soundId;
		s32 seqIndex;
		s32 chunkOffset;
		s32 tick;
		s32 prevTick;
		s32 chunkPtr;
		s32 ticksPerBeat;
		s32 beatsPerMeasure;
		s32 tickFixed;
		s32 tempo;
		s32 step;
		s32 speed;
		s32 stepFixed;
	};

	struct ImMidiPlayer
	{
		ImMidiPlayer* prev;
		ImMidiPlayer* next;
		PlayerData* data;
		ImMidiPlayer* sharedPart;
		s32 sharedPartId;
		ImSoundId soundId;
		s32 marker;
		s32 group;
		s32 priority;
		s32 volume;
		s32 groupVolume;
		s32 pan;
		s32 detune;
		s32 transpose;
		s32 mailbox;
		s32 hook;

		SoundChannel channels[imChannelCount];
	};

	struct InstrumentSound
	{
		ImMidiPlayer* soundList;
		// Members used per sound.
		InstrumentSound* next;
		ImMidiPlayer* soundPlayer;
		s32 instrumentId;
		s32 channelId;
		s32 curTick;
		s32 curTickFixed;
	};

	struct ImSoundFader
	{
		s32 active;
		ImSoundId soundId;
		iMuseParameter param;
		s32 curValue;
		s32 timeRem;
		s32 fadeTime;
		s32 signedStep;
		s32 unsignedStep;
		s32 errAccum;
		s32 stepDir;
	};

	typedef void(*MidiCmdFunc1)(PlayerData* playerData, u8* chunkData);
	typedef void(*MidiCmdFunc2)(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	union MidiCmdFunc
	{
		MidiCmdFunc1 func1;
		MidiCmdFunc2 func2;
	};

	static u8 s_midiMsgSize[] =
	{
		3, 3, 3, 3, 2, 2, 3, 1,
	};
	static s32 s_midiMessageSize2[] =
	{
		3, 3, 3, 3, 2, 2, 3,
	};

	enum ImMidiMessageIndex
	{
		IM_MID_NOTE_OFF = 0,
		IM_MID_NOTE_ON = 1,
		IM_MID_KEY_PRESSURE = 2,
		IM_MID_CONTROL_CHANGE = 3,
		IM_MID_PROGRAM_CHANGE = 4,
		IM_MID_CHANNEL_PRESSURE = 5,
		IM_MID_PITCH_BEND = 6,
		IM_MID_SYS_FUNC = 7,
		IM_MID_EVENT = 8,
		IM_MID_COUNT = 9,
	};
		
	/////////////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////////////
	void ImAdvanceSound(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc);
	void ImAdvanceMidiPlayer(PlayerData* playerData);
	void ImJumpSustain(ImMidiPlayer* player, u8* sndData, PlayerData* playerData1, PlayerData* playerData2);
	void ImMidiGetSoundInstruments(ImMidiPlayer* player, s32* soundMidiInstrumentMask, s32* midiInstrumentCount);
	s32  ImMidiGetTickDelta(PlayerData* playerData, u32 prevTick, u32 tick);
	void ImMidiProcessSustain(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc, ImMidiPlayer* player);

	void ImSoundPlayerLock();
	void ImSoundPlayerUnlock();
	s32  ImPauseMidi();
	s32  ImPauseDigitalSound();
	s32  ImResumeMidi();
	s32  ImResumeDigitalSound();
	s32  ImHandleChannelGroupVolume();
	s32  ImGetGroupVolume(s32 group);
	void ImHandleChannelVolumeChange(ImMidiPlayer* player, SoundChannel* channel);
	void ImResetSoundChannel(SoundChannel* channel);
	void ImFreeMidiChannel(ImMidiChannel* channelData);
	void ImMidiSetupParts();
	void ImAssignMidiChannel(ImMidiPlayer* player, SoundChannel* channel, ImMidiChannel* midiChannel);
	u8*  ImGetSoundData(ImSoundId id);
	u8*  ImInternalGetSoundData(ImSoundId soundId);
	ImMidiChannel* ImGetFreeMidiChannel();
	ImMidiPlayer* ImGetFreePlayer(s32 priority);
	s32 ImStartMidiPlayerInternal(PlayerData* data, ImSoundId soundId);
	s32 ImAddPlayer(ImMidiPlayer** sndList, ImMidiPlayer* player);
	s32 ImSetupMidiPlayer(ImSoundId soundId, s32 priority);
	s32 ImFreeSoundPlayer(ImSoundId soundId);
	s32 ImReleaseAllPlayers();
	s32 ImReleaseAllWaveSounds();
	s32 ImGetSoundType(ImSoundId soundId);
	s32 ImSetMidiParam(ImSoundId soundId, s32 param, s32 value);
	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value);
	s32 ImGetMidiParam(ImSoundId soundId, s32 param);
	s32 ImGetWaveParam(ImSoundId soundId, s32 param);
	s32 ImGetPendingSoundCount(s32 soundId);
	ImSoundId ImFindNextMidiSound(ImSoundId soundId);
	ImSoundId ImFindNextWaveSound(ImSoundId soundId);
	ImMidiPlayer* ImGetSoundPlayer(ImSoundId soundId);
	s32 ImSetHookMidi(ImSoundId soundId, s32 value);
	s32 ImFixupSoundTick(PlayerData* data, s32 value);
	s32 ImSetSequence(PlayerData* data, u8* sndData, s32 seqIndex);

	void ImMidiLock();
	void ImMidiUnlock();
	s32  ImGetDeltaTime();
	s32  ImMidiSetSpeed(PlayerData* data, u32 value);
	void ImSetTempo(PlayerData* data, u32 tempo);
	void ImSetMidiTicksPerBeat(PlayerData* data, s32 ticksPerBeat, s32 beatsPerMeasure);;

	// Midi channel commands
	void ImMidiChannelSetVolume(ImMidiChannel* midiChannel, s32 volume);
	void ImHandleChannelDetuneChange(ImMidiPlayer* player, SoundChannel* channel);
	void ImHandleChannelPitchBend(ImMidiPlayer* player, s32 channelIndex, s32 fractValue, s32 intValue);
	void ImMidiChannelSetPgm(ImMidiChannel* midiChannel, s32 pgm);
	void ImMidiChannelSetPriority(ImMidiChannel* midiChannel, s32 priority);
	void ImMidiChannelSetPartNoteReq(ImMidiChannel* midiChannel, s32 noteReq);
	void ImMidiChannelSetPan(ImMidiChannel* midiChannel, s32 pan);
	void ImMidiChannelSetModulation(ImMidiChannel* midiChannel, s32 modulation);
	void ImHandleChannelPan(ImMidiChannel* midiChannel, s32 pan);
	void ImSetChannelSustain(ImMidiChannel* midiChannel, s32 sustain);

	void ImCheckForTrackEnd(PlayerData* playerData, u8* data);
	void ImMidiJump2_NoteOn(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiCommand(ImMidiPlayer* player, s32 channelIndex, s32 midiCmd, s32 value);
	void ImMidiStopAllNotes(ImMidiPlayer* player);

	void ImMidiNoteOff(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiNoteOn(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiProgramChange(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiPitchBend(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2);
	void ImMidiEvent(PlayerData* playerData, u8* chunkData);
			
	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	static const s32 s_channelMask[imChannelCount] =
	{
		(1 <<  0), (1 <<  1), (1 <<  2), (1 <<  3),
		(1 <<  4), (1 <<  5), (1 <<  6), (1 <<  7),
		(1 <<  8), (1 <<  9), (1 << 10), (1 << 11),
		(1 << 12), (1 << 13), (1 << 14), (1 << 15)
	};

	const char* c_midi = "MIDI";
	const u32 c_crea = 0x61657243;	// "Crea"
	static s32 s_iMuseTimestepMicrosec = 6944;

	static ImMidiChannel s_midiChannels[imChannelCount];
	static s32 s_imPause = 0;
	static s32 s_midiPaused = 0;
	static s32 s_digitalPause = 0;
	static s32 s_sndPlayerLock = 0;
	static s32 s_midiLock = 0;
	static s32 s_trackTicksRemaining;
	static s32 s_midiTickDelta;
	static s32 s_midiTrackEnd;
	static s32 s_imEndOfTrack;

	static ImMidiPlayer s_soundPlayers[2];
	static ImMidiPlayer** s_soundPlayerList = nullptr;
	static PlayerData* s_imSoundHeaderCopy1 = nullptr;
	static PlayerData* s_imSoundHeaderCopy2 = nullptr;

	static s32 s_groupVolume[groupMaxCount] = { 0 };
	static s32 s_soundGroupVolume[groupMaxCount] =
	{
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
		imMaxVolume, imMaxVolume, imMaxVolume, imMaxVolume,
	};

	static s32 s_midiChannelTrim[imChannelCount] = { 0 };
	static s32 s_midiInstrumentChannelMask[MIDI_INSTRUMENT_COUNT];
	static s32 s_midiInstrumentChannelMask3[MIDI_INSTRUMENT_COUNT];
	static s32 s_midiInstrumentChannelMask2[MIDI_INSTRUMENT_COUNT];
	static s32 s_curMidiInstrumentMask[MIDI_INSTRUMENT_COUNT];
	static s32 s_curInstrumentCount;

	// Midi functions for each message type - see ImMidiMessageIndex{} above.
	static MidiCmdFunc s_jumpMidiCmdFunc[IM_MID_COUNT] =
	{
		nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr,
		ImMidiEvent,
	};
	static MidiCmdFunc s_jumpMidiCmdFunc2[IM_MID_COUNT] =
	{
		nullptr, (MidiCmdFunc1)ImMidiJump2_NoteOn, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr,
		ImCheckForTrackEnd
	};
	static MidiCmdFunc s_midiCmdFunc[IM_MID_COUNT] =
	{
		(MidiCmdFunc1)ImMidiNoteOff, (MidiCmdFunc1)ImMidiNoteOn, nullptr, (MidiCmdFunc1)ImMidiCommand,
		(MidiCmdFunc1)ImMidiProgramChange, nullptr, (MidiCmdFunc1)ImMidiPitchBend, nullptr,
		ImMidiEvent,
	};

	static InstrumentSound** s_imActiveInstrSounds = nullptr;
	static InstrumentSound** s_imInactiveInstrSounds = nullptr;

	// Midi files loaded.
	static u32 s_midiFileCount = 0;
	static u8* s_midiFiles[6];

	/////////////////////////////////////////////////////////// 
	// Main API
	// -----------------------------------------------------
	// This includes both high level functions and low level
	// Note:
	// The original DOS code wrapped calls to an internal
	// dispatch function which is removed for TFE, calling
	// the functions directly instead.
	/////////////////////////////////////////////////////////// 

	////////////////////////////////////////////////////
	// High level functions
	////////////////////////////////////////////////////
	s32 ImSetMasterVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetMasterVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetMusicVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetMusicVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetSfxVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetSfxVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetVoiceVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetVoiceVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartSfx(ImSoundId soundId, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartVoice(ImSoundId soundId, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartMusic(ImSoundId soundId, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	////////////////////////////////////////////////////
	// Low level functions
	////////////////////////////////////////////////////
	s32 ImInitialize()
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImTerminate(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImPause(void)
	{
		s32 res = imSuccess;
		if (!s_imPause)
		{
			res  = ImPauseMidi();
			res |= ImPauseDigitalSound();
		}
		s_imPause++;
		return (res == imSuccess) ? s_imPause : res;
	}

	s32 ImResume(void)
	{
		s32 res = imSuccess;
		if (s_imPause == 1)
		{
			res  = ImResumeMidi();
			res |= ImResumeDigitalSound();
		}
		if (s_imPause)
		{
			s_imPause--;
		}
		return (res == imSuccess) ? s_imPause : res;
	}

	s32 ImSetGroupVol(s32 group, s32 volume)
	{
		if (group >= groupMaxCount || volume > imMaxVolume)
		{
			return imArgErr;
		}
		else if (volume == imGetValue)
		{
			return s_groupVolume[group];
		}

		s32 groupVolume = s_groupVolume[group];
		if (group == groupMaster)
		{
			s_groupVolume[groupMaster] = volume;
			s_soundGroupVolume[groupMaster] = 0;
			for (s32 i = 1; i < groupMaxCount; i++)
			{
				s_soundGroupVolume[i] = ((s_groupVolume[i] + 1) * volume) >> imVolumeShift;
			}
		}
		else
		{
			s_groupVolume[group] = volume;
			s_soundGroupVolume[group] = ((s_groupVolume[groupMaster] + 1) * volume) >> imVolumeShift;
		}
		ImHandleChannelGroupVolume();
		// TODO: func_2e9689();

		return groupVolume;
	}

	s32 ImStartSound(ImSoundId soundId, s32 priority)
	{
		u8* data = ImInternalGetSoundData(soundId);
		if (!data)
		{
			TFE_System::logWrite(LOG_ERROR, "IMuse", "null sound addr in StartSound()");
			return imFail;
		}

		s32 i = 0;
		for (; i < 4; i++)
		{
			if (data[i] != c_midi[i])
			{
				break;
			}
		}
		if (i == 4)  // Midi
		{
			return ImSetupMidiPlayer(soundId, priority);
		}
		else  // Digital Sound
		{
			// IM_TODO: Digital sound
		}
		return imSuccess;
	}

	s32 ImStopSound(ImSoundId soundId)
	{
		s32 type = ImGetSoundType(soundId);
		if (type == typeMidi)
		{
			return ImFreeSoundPlayer(soundId);
		}
		else if (type == typeWave)
		{
			// IM_TODO: Digital sound
		}
		return imFail;
	}

	s32 ImStopAllSounds(void)
	{
		s32 res = ImClearAllSoundFaders();
		res |= ImClearTriggersAndCmds();
		res |= ImReleaseAllPlayers();
		res |= ImReleaseAllWaveSounds();
		return res;
	}

	s32 ImGetNextSound(ImSoundId soundId)
	{
		ImSoundId nextMidiId = ImFindNextMidiSound(soundId);
		ImSoundId nextWaveId = ImFindNextWaveSound(soundId);
		if (nextMidiId == 0 || (nextWaveId > 0 && nextMidiId >= nextWaveId))
		{
			return nextWaveId;
		}
		return nextMidiId;
	}

	s32 ImSetParam(ImSoundId soundId, s32 param, s32 value)
	{
		iMuseSoundType type = (iMuseSoundType)ImGetSoundType(soundId);
		if (type == typeMidi)
		{
			return ImSetMidiParam(soundId, param, value);
		}
		else if (type == typeWave)
		{
			return ImSetWaveParam(soundId, param, value);
		}
		return imFail;
	}

	s32 ImGetParam(ImSoundId soundId, s32 param)
	{
		iMuseSoundType type = (iMuseSoundType)ImGetSoundType(soundId);
		if (param == soundType)
		{
			return type;
		}
		else if (param == soundPendCount)
		{
			return ImGetPendingSoundCount(soundId);
		}
		else if (type == typeMidi)
		{
			return ImGetMidiParam(soundId, param);
		}
		else if (type == typeWave)
		{
			return ImGetWaveParam(soundId, param);
		}
		else if (param == soundPlayCount)
		{
			// This is zero here, since there is no valid type - 
			// so no sounds are that type are playing.
			return 0;
		}
		return imFail;
	}

	s32 ImSetHook(ImSoundId soundId, s32 value)
	{
		iMuseSoundType type = (iMuseSoundType)ImGetSoundType(soundId);
		if (type == typeMidi)
		{
			return ImSetHookMidi(soundId, value);
		}
		else if (type == typeWave)
		{
			// IM_TODO
		}
		return imFail;
	}

	s32 ImGetHook(ImSoundId soundId)
	{
		// Not called from Dark Forces.
		return imSuccess;
	}

	s32 ImJumpMidi(ImSoundId soundId, s32 chunk, s32 measure, s32 beat, s32 tick, s32 sustain)
	{
		ImMidiPlayer* player = ImGetSoundPlayer(soundId);
		if (!player) { return imInvalidSound; }

		PlayerData* data = player->data;
		u8* sndData = ImInternalGetSoundData(data->soundId);
		if (!sndData) { return imInvalidSound; }

		measure--;
		beat--;
		chunk--;
		if (measure >= 1000 || beat >= 12 || tick >= 480)
		{
			return imArgErr;
		}

		ImMidiLock();
		memcpy(s_imSoundHeaderCopy2, data, 56);
		memcpy(s_imSoundHeaderCopy1, data, 56);

		u32 newTick = ImFixupSoundTick(data, intToFixed16(measure)*16 + intToFixed16(beat) + tick);
		// If we are jumping backwards - we reset the chunk.
		if (chunk != s_imSoundHeaderCopy1->seqIndex || newTick < (u32)s_imSoundHeaderCopy1->chunkOffset)
		{
			if (ImSetSequence(s_imSoundHeaderCopy1, sndData, chunk))
			{
				TFE_System::logWrite(LOG_ERROR, "iMuse", "sq jump to invalid chunk.");
				ImMidiUnlock();
				return imFail;
			}
		}
		s_imSoundHeaderCopy1->tick = newTick;
		ImAdvanceSound(s_imSoundHeaderCopy1, sndData, s_jumpMidiCmdFunc);
		if (s_imEndOfTrack)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "sq jump to invalid ms:bt:tk...");
			ImMidiUnlock();
			return imFail;
		}

		if (sustain)
		{
			// Loop through the channels.
			for (s32 c = 0; c < imChannelCount; c++)
			{
				ImMidiCommand(data->player, c, MID_SUSTAIN_SWITCH, 0);
				ImMidiCommand(data->player, c, MID_MODULATIONWHEEL_MSB, 0);
				ImHandleChannelPitchBend(data->player, c, 0, 64);
			}
			ImJumpSustain(data->player, sndData, s_imSoundHeaderCopy1, s_imSoundHeaderCopy2);
		}
		else
		{
			ImMidiStopAllNotes(data->player);
		}

		memcpy(data, s_imSoundHeaderCopy1, 56);
		s_imEndOfTrack = 1;
		ImMidiUnlock();

		return imSuccess;
	}

	s32 ImSendMidiMsg(ImSoundId soundId, s32 arg1, s32 arg2, s32 arg3)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImShareParts(ImSoundId sound1, ImSoundId sound2)
	{
		ImMidiPlayer* player1 = ImGetSoundPlayer(sound1);
		ImMidiPlayer* player2 = ImGetSoundPlayer(sound2);
		if (!player1 || player1->sharedPart || !player2 || player2->sharedPart)
		{
			return imFail;
		}

		player1->sharedPart = player2;
		player2->sharedPart = player1;
		player1->sharedPartId = sound2;
		player2->sharedPartId = sound1;
		ImMidiSetupParts();
		return imSuccess;
	}

	/////////////////////////////////////////////////////////// 
	// Internal Implementation
	/////////////////////////////////////////////////////////// 
	s32 ImPauseMidi()
	{
		s_midiPaused = 1;
		return imSuccess;
	}

	s32 ImPauseDigitalSound()
	{
		ImSoundPlayerLock();
		s_digitalPause = 1;
		ImSoundPlayerUnlock();
		return imSuccess;
	}
		
	s32 ImResumeMidi()
	{
		s_midiPaused = 0;
		return imSuccess;
	}

	s32 ImResumeDigitalSound()
	{
		ImSoundPlayerLock();
		s_digitalPause = 0;
		ImSoundPlayerUnlock();
		return imSuccess;
	}
		
	void ImSoundPlayerLock()
	{
		s_sndPlayerLock++;
	}

	void ImSoundPlayerUnlock()
	{
		if (s_sndPlayerLock)
		{
			s_sndPlayerLock--;
		}
	}

	s32 ImHandleChannelGroupVolume()
	{
		ImMidiPlayer* player = *s_soundPlayerList;
		while (player)
		{
			s32 sndVol = player->volume + 1;
			s32 groupVol = ImGetGroupVolume(player->group);
			player->groupVolume = (sndVol * groupVol) >> imVolumeShift;

			for (s32 i = 0; i < imChannelCount; i++)
			{
				SoundChannel* channel = &player->channels[i];
				ImHandleChannelVolumeChange(player, channel);
			}
			player = player->next;
		}
		return imSuccess;
	}

	s32 ImGetGroupVolume(s32 group)
	{
		if (group >= groupMaxCount)
		{
			return imArgErr;
		}
		return s_soundGroupVolume[group];
	}

	void ImHandleChannelVolumeChange(ImMidiPlayer* player, SoundChannel* channel)
	{
		if (!channel->outChannelCount)
		{
			// This should never be hit.
			TFE_System::logWrite(LOG_ERROR, "IMuse", "Sound channel has 0 output channels.");
			assert(0);
		}
		s32 partTrim   = channel->partTrim   + 1;
		s32 partVolume = channel->partVolume + 1;
		channel->groupVolume = (partVolume * partTrim * player->groupVolume) >> (2*imVolumeShift);

		// These checks seem redundant. If groupVolume is != 0, then by definition partTrim and partVolume are != 0.
		if (!player->groupVolume || !channel->partTrim || !channel->partVolume)
		{
			if (!channel->data) { return; }
			ImResetSoundChannel(channel);
		}
		else if (channel->data) // has volume.
		{
			ImMidiChannelSetVolume(channel->data, channel->groupVolume);
		}
		ImMidiSetupParts();
	}

	void ImMidiSetupParts()
	{
		ImMidiPlayer* player = *s_soundPlayerList;
		ImMidiPlayer*  prevPlayer2  = nullptr;
		ImMidiPlayer*  prevPlayer   = nullptr;
		SoundChannel* prevChannel2 = nullptr;
		SoundChannel* prevChannel  = nullptr;
		s32 r;

		while (player)
		{
			for (s32 m = 0; m < imChannelCount; m++)
			{
				SoundChannel* channel = &player->channels[m];
				ImMidiPlayer* sharedPart = player->sharedPart;
				SoundChannel* sharedChannels = nullptr;
				if (player->sharedPart)
				{
					sharedChannels = player->sharedPart->channels;
				}
				SoundChannel* sharedChannels2 = sharedChannels;
				if (sharedChannels2)
				{
					if (channel->data && !sharedChannels->data && sharedChannels->partStatus)
					{
						ImMidiPlayer* sharedPart = player->sharedPart;
						if (sharedPart->groupVolume && sharedChannels->partTrim && sharedChannels->partVolume)
						{
							ImAssignMidiChannel(sharedPart, sharedChannels, channel->data);
						}
					}
					else if (!channel->data && sharedChannels2->data && channel->partStatus && player->groupVolume &&
						channel->partTrim && channel->partVolume)
					{
						ImAssignMidiChannel(player, channel, sharedChannels2->data->sharedMidiChannel);
					}
				}
				if (channel->data)
				{
					if (sharedChannels2 && sharedChannels2->data)
					{
						if (sharedChannels2->priority > channel->priority)
						{
							continue;
						}
					}
					if (prevChannel)
					{
						if (channel->priority > prevChannel->priority)
						{
							continue;
						}
					}
					prevPlayer = player;
					prevChannel = channel;
					r = 1;
				}
				else if (channel->partStatus && player->groupVolume && channel->partTrim && channel->partVolume)
				{
					if (!prevChannel2 || channel->priority > prevChannel2->priority)
					{
						prevPlayer2 = player;
						prevChannel2 = channel;
						r = 0;
					}
				}
			}
			player = player->next;

			// This will only start once all of the players has been exhausted.
			if (prevChannel2 && !player)
			{
				ImMidiChannel* midiChannel = ImGetFreeMidiChannel();
				ImMidiPlayer*  newPlayer  = nullptr;
				SoundChannel* newChannel = nullptr;
				if (midiChannel)
				{
					newPlayer  = prevPlayer2;
					newChannel = prevChannel2;
				}
				else
				{
					// IM_TODO:
				}
				ImAssignMidiChannel(newPlayer, newChannel, midiChannel);

				player = *s_soundPlayerList;
				prevPlayer2  = nullptr;
				prevPlayer   = nullptr;
				prevChannel2 = nullptr;
				prevChannel  = nullptr;
			}
		}
	}

	ImMidiChannel* ImGetFreeMidiChannel()
	{
		for (s32 i = 0; i < imChannelCount - 1; i++)
		{
			if (!s_midiChannels[i].player && !s_midiChannels[i].sharedPart)
			{
				return &s_midiChannels[i];
			}
		}
		return nullptr;
	}

	void ImAssignMidiChannel(ImMidiPlayer* player, SoundChannel* channel, ImMidiChannel* midiChannel)
	{
		if (!channel || !midiChannel)
		{
			return;
		}

		channel->data = midiChannel;
		midiChannel->player  = player;
		midiChannel->channel = channel;

		ImMidiChannelSetPgm(midiChannel, channel->partPgm);
		ImMidiChannelSetPriority(midiChannel, channel->priority);
		ImMidiChannelSetPartNoteReq(midiChannel, channel->partNoteReq);
		ImMidiChannelSetVolume(midiChannel, channel->groupVolume);
		ImMidiChannelSetPan(midiChannel, channel->partPan);
		ImMidiChannelSetModulation(midiChannel, channel->modulation);
		ImHandleChannelPan(midiChannel, channel->pan);
		ImSetChannelSustain(midiChannel, channel->sustain);
	}

	u8* ImGetSoundData(ImSoundId id)
	{
		if (id & imMidiFlag)
		{
			return s_midiFiles[id & imMidiMask];
		}
		else
		{
			// IM_TODO: Digital sound.
		}
		return nullptr;
	}

	s32 ImInternal_SoundValid(ImSoundId soundId)
	{
		if (soundId && soundId < imValidMask)
		{
			return 1;
		}
		return 0;
	}

	u8* ImInternalGetSoundData(ImSoundId soundId)
	{
		if (ImInternal_SoundValid(soundId))
		{
			return ImGetSoundData(soundId);
		}
		return nullptr;
	}

	ImMidiPlayer* ImGetFreePlayer(s32 priority)
	{
		ImMidiPlayer* soundPlayer = s_soundPlayers;
		for (s32 i = 0; i < 2; i++, soundPlayer++)
		{
			if (!soundPlayer->soundId)
			{
				return soundPlayer;
			}
		}
		TFE_System::logWrite(LOG_ERROR, "iMuse", "no spare players");
		return nullptr;
	}
		
	s32 ImStartMidiPlayerInternal(PlayerData* data, ImSoundId soundId)
	{
		u8* sndData = ImInternalGetSoundData(soundId);
		if (!sndData)
		{
			return imInvalidSound;
		}

		data->soundId = soundId;
		data->tickFixed = 0;
		ImSetTempo(data, 500000); // microseconds per beat, 500000 = 120 beats per minute
		ImMidiSetSpeed(data, 128);
		ImSetMidiTicksPerBeat(data, 480, 4);
		return ImSetSequence(data, sndData, 0);
	}

	s32 ImAddPlayer(ImMidiPlayer** sndList, ImMidiPlayer* player)
	{
		if (!player || player->next || player->prev)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "List arg err when adding");
			return imArgErr;
		}

		player->next = *sndList;
		if (*sndList)
		{
			player->next->prev = player;
		}

		player->prev = nullptr;
		*sndList = player;
		return imSuccess;
	}

	s32 ImSetupMidiPlayer(ImSoundId soundId, s32 priority)
	{
		s32 clampedPriority = clamp(priority, 0, imMaxPriority);
		ImMidiPlayer* player = ImGetFreePlayer(clampedPriority);
		if (!player)
		{
			return imAllocErr;
		}

		player->sharedPart = nullptr;
		player->sharedPartId = 0;
		player->marker = -1;
		player->group = groupMusic;
		player->priority = clampedPriority;
		player->volume = imMaxVolume;
		player->groupVolume = ImGetGroupVolume(player->group);
		player->pan = imPanCenter;

		player->detune = 0;
		player->transpose = 0;
		player->mailbox = 0;
		player->hook = 0;

		for (s32 i = 0; i < imChannelCount; i++)
		{
			SoundChannel* channel = &player->channels[i];

			channel->data = nullptr;
			channel->partStatus = 0;
			channel->partPgm = 128;
			channel->partTrim = imMaxVolume;
			channel->partPriority = 0;
			channel->priority = player->priority;
			channel->partNoteReq = 1;
			channel->partVolume = imMaxVolume;
			channel->groupVolume = player->groupVolume;
			channel->partPan = imPanCenter;
			channel->modulation = 0;
			channel->sustain = 0;
			channel->pitchBend = 0;
			channel->outChannelCount = 2;
			channel->pan = 0;
		}

		PlayerData* playerData = player->data;
		if (ImStartMidiPlayerInternal(playerData, soundId))
		{
			return imFail;
		}

		player->soundId = soundId;
		ImAddPlayer(s_soundPlayerList, player);
		return imSuccess;
	}

	void _ImNoteOff(s32 channelId, s32 instrId)
	{
		// Stub
	}

	void ImSendMidiMsg_(s32 channelId, MidiController ctrl, s32 value)
	{
		// Stub
	}

	void ImSendMidiMsg_(u8 channel, u8 msg, u8 arg1)
	{
		// Stub
	}

	void ImSendMidiMsg_R_(u8 channel, u8 msg)
	{
		// Stub
	}

	// For Pan, "Fine" resolution is 14-bit where 8192 (0x2000) is center - MID_PAN_LSB
	// Most devices use coarse adjustment instead (7 bits, 64 is center) - MID_PAN_MSB
	void ImSetPanFine_(s32 channel, s32 pan)
	{
		// Stub
	}
		
	void ImResetSoundChannel(SoundChannel* channel)
	{
		ImMidiChannel* data = channel->data;
		if (data)
		{
			ImFreeMidiChannel(data);
			data->player = nullptr;
			data->channel = nullptr;
			channel->data = nullptr;
		}
	}
		
	void ImFreeMidiChannel(ImMidiChannel* channelData)
	{
		if (channelData)
		{
			u32 channelMask = s_channelMask[channelData->channelId];
			channelData->sustain = 0;
			for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
			{
				s32 instrMask = channelData->instrumentMask[i];
				if (channelData->instrumentMask[i] & channelMask)
				{
					_ImNoteOff(channelData->channelId, i);
					channelData->instrumentMask[i] &= ~channelMask;
				}
				else if (channelData->instrumentMask2[i] & channelMask)
				{
					_ImNoteOff(channelData->channelId, i);
					channelData->instrumentMask2[i] &= ~channelMask;
				}
			}
		}
	}

	void ImReleaseSoundPlayer(ImMidiPlayer* player)
	{
		// imNotImplemented
	}

	s32 ImFreeSoundPlayer(ImSoundId soundId)
	{
		return imNotImplemented;
	}

	s32 ImReleaseAllPlayers()
	{
		ImMidiPlayer* player = nullptr;
		do
		{
			player = *s_soundPlayerList;
			if (player)
			{
				ImReleaseSoundPlayer(player);
			}
		} while (player);
		return imSuccess;
	}

	s32 ImReleaseAllWaveSounds()
	{
		/*
		** Stubbed
		ImWaveSound* snd = s_imWaveSounds;
		ImSoundPlayerLock();
		if (snd)
		{
			// IM_TODO
		}
		ImSoundPlayerUnlock();
		*/
		return imSuccess;
	}
		
	s32 ImGetSoundType(ImSoundId soundId)
	{
		return imNotImplemented;
	}

	s32 ImSetMidiParam(ImSoundId soundId, s32 param, s32 value)
	{
		return imNotImplemented;
	}

	s32 ImSetWaveParam(ImSoundId soundId, s32 param, s32 value)
	{
		return imNotImplemented;
	}

	s32 ImGetMidiParam(ImSoundId soundId, s32 param)
	{
		return imNotImplemented;
	}

	s32 ImGetWaveParam(ImSoundId soundId, s32 param)
	{
		return imNotImplemented;
	}

	s32 ImGetPendingSoundCount(s32 soundId)
	{
		// Not called by Dark Forces.
		assert(0);
		return 0;
	}

	// Search a list for the smallest non-zero value that is atleast minValue.
	ImSoundId ImFindNextMidiSound(ImSoundId soundId)
	{
		ImSoundId value = 0;
		ImMidiPlayer* entry = *s_soundPlayerList;
		while (entry)
		{
			if (soundId < entry->soundId)
			{
				if (!value || value > entry->soundId)
				{
					value = entry->soundId;
				}
			}
			entry = entry->next;
		}
		return value;
	}

	ImSoundId ImFindNextWaveSound(ImSoundId soundId)
	{
		ImSoundId nextSoundId = 0;
		/*
		** Stubbed **
		ImWaveSound* snd = s_imWaveSounds;
		while (snd)
		{
			if (snd->soundId > soundId)
			{
				if (nextSoundId)
				{
					// IM_TODO
				}
				nextSoundId = snd->soundId;
			}
			snd = snd->next;
		}
		*/
		return nextSoundId;
	}

	ImMidiPlayer* ImGetSoundPlayer(ImSoundId soundId)
	{
		ImMidiPlayer* player = *s_soundPlayerList;
		while (player)
		{
			if (player->soundId == soundId)
			{
				break;
			}
			player = player->next;
		}
		return player;
	}

	s32 ImSetHookMidi(ImSoundId soundId, s32 value)
	{
		if ((u32)value > 0x80000000ul)
		{
			return imArgErr;
		}
		ImMidiPlayer* player = ImGetSoundPlayer(soundId);
		if (!player)
		{
			return imInvalidSound;
		}
		player->hook = value;
		return imSuccess;
	}

	void ImMidiLock()
	{
		s_midiLock++;
	}

	void ImMidiUnlock()
	{
		if (s_midiLock)
		{
			s_midiLock--;
		}
	}

	// Gets the deltatime in microseconds (i.e. 1,000 ms / 1 microsec).
	s32 ImGetDeltaTime()
	{
		return s_iMuseTimestepMicrosec;
	}
		
	s32 ImMidiSetSpeed(PlayerData* data, u32 value)
	{
		if (value > 255)
		{
			return imArgErr;
		}

		ImMidiLock();
		data->speed = value;
		data->stepFixed = (value * data->step) >> 7;
		ImMidiUnlock();
		return imSuccess;
	}
		
	void ImSetTempo(PlayerData* data, u32 tempo)
	{
		s32 ticks = ImGetDeltaTime() * 480;
		data->tempo = tempo;

		// Keep reducing the tick scale? until it is less than one?
		// This also reduces the tempo as well.
		while (1)
		{
			if (!(ticks & 0xffff0000))
			{
				if (!(tempo & 0xffff0000))
				{
					break;
				}
				ticks >>= 1;
				tempo >>= 1;
			}
			else
			{
				ticks >>= 1;
				tempo >>= 1;
			}
		}

		data->step = (ticks << 16) / tempo;
		ImMidiSetSpeed(data, data->speed);
	}

	void ImSetMidiTicksPerBeat(PlayerData* data, s32 ticksPerBeat, s32 beatsPerMeasure)
	{
		ImMidiLock();
		data->ticksPerBeat = ticksPerBeat;
		data->beatsPerMeasure = intToFixed16(beatsPerMeasure);
		ImMidiUnlock();
	}

	s32 ImSetSequence(PlayerData* data, u8* sndData, s32 seqIndex)
	{
		u8* chunk = midi_gotoHeader(sndData, "MTrk", seqIndex);
		if (!chunk)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "Sq couldn't find chunk %d", seqIndex + 1);
			return imFail;
		}

		// Skip the header and size to the data.
		u8* chunkData = &chunk[8];	// ebx
		data->seqIndex = seqIndex;	// eax
		// offset of the chunk data from the base sound data.
		data->chunkOffset = s32(chunkData - sndData);

		u32 dt = midi_getVariableLengthValue(&chunkData);
		data->prevTick = ImFixupSoundTick(data, dt);
		data->tick = 0;
		data->chunkPtr = s32(chunkData - (sndData + data->chunkOffset));
		return imSuccess;
	}

	s32 ImFixupSoundTick(PlayerData* data, s32 value)
	{
		while ((value & 0xffff) >= data->ticksPerBeat)
		{
			value = value - data->ticksPerBeat + ONE_16;
			while ((value & FIXED(15)) >= data->beatsPerMeasure)
			{
				value = value - data->beatsPerMeasure + ONE_16;
			}
		}
		return value;
	}
						
	// Advance the current sound to the next tick.
	void ImAdvanceSound(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc)
	{
		s_imEndOfTrack = 0;
		s32 prevTick = playerData->prevTick;
		s32 tick = playerData->tick;
		u8* chunkData = sndData + playerData->chunkOffset + playerData->chunkPtr;
		while (tick > prevTick)
		{
			u8 data = chunkData[0];
			s32 idx = 0;
			if (data < 0xf0)
			{
				if (data & 0x80)
				{
					u8 msgType = (data & 0x70) >> 4;
					u8 channel = data & 0x0f;
					if (midiCmdFunc[msgType].func2)
					{
						midiCmdFunc[msgType].func2(playerData->player, channel, chunkData[1], chunkData[2]);
					}
					chunkData += s_midiMsgSize[msgType];
				}
			}
			else
			{
				if (data == 0xf0)
				{
					idx = IM_MID_SYS_FUNC;
				}
				else
				{
					if (data != 0xff)
					{
						// 2eec0b:
						printf("ERROR:sq unknown msg type 0x%x...", data);
						ImReleaseSoundPlayer(playerData->player);
						return;
					}

					idx = IM_MID_EVENT;
					chunkData++;
				}
				if (midiCmdFunc[idx].func1)
				{
					midiCmdFunc[idx].func1(playerData, chunkData);
				}

				// This steps past the current chunk, if we don't know what a chunk is, it should be skipped.
				chunkData++;
				// chunkSize is a variable length value.
				chunkData += midi_getVariableLengthValue(&chunkData);
			}

			if (s_imEndOfTrack)
			{
				TFE_System::logWrite(LOG_ERROR, "iMuse", "ImAdvanceSound() - Invalid end of track encountered.");
				return;
			}

			u32 dt = midi_getVariableLengthValue(&chunkData);
			prevTick += dt;
			if ((prevTick & 0xffff) >= playerData->ticksPerBeat)
			{
				prevTick = ImFixupSoundTick(playerData, prevTick);
			}
		}
		playerData->prevTick = prevTick;
		u8* chunkBase = sndData + playerData->chunkOffset;
		playerData->chunkPtr = s32(chunkData - chunkBase);
	}

	// This gets called at a fixed rate, where each step = 'stepFixed' ticks.
	void ImAdvanceMidiPlayer(PlayerData* playerData)
	{
		playerData->tickFixed += playerData->stepFixed;
		playerData->tick += floor16(playerData->tickFixed);
		playerData->tickFixed &= 0xffff0000;

		if ((playerData->tick & 0xffff) >= playerData->ticksPerBeat)
		{
			playerData->tick = ImFixupSoundTick(playerData, playerData->tick);
		}
		if (playerData->prevTick < playerData->tick)
		{
			u8* sndData = ImInternalGetSoundData(playerData->soundId);
			if (sndData)
			{
				ImAdvanceSound(playerData, sndData, s_midiCmdFunc);
				return;
			}
			TFE_System::logWrite(LOG_ERROR, "iMuse", "sq int handler got null addr");
			// TODO: ERROR handling
		}
	}
		
	void ImMidiGetSoundInstruments(ImMidiPlayer* player, s32* soundMidiInstrumentMask, s32* midiInstrumentCount)
	{
		*midiInstrumentCount = 0;

		u32 channelMask = 0;
		// Determine which outout channels that this sound is playing on.
		// The result is a 16 bit mask. For example, if the sound is only playing on channel 1,
		// the resulting mask = 1<<1 = 2.
		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiChannel* midiChannel = &s_midiChannels[i];
			if (player == midiChannel->player)
			{
				channelMask |= s_channelMask[midiChannel->channelId];
			}
		}

		for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
		{
			u32 value = s_midiInstrumentChannelMask[i] & channelMask;
			soundMidiInstrumentMask[i] = value;
			// Count the number of channels currently playing the instrument.
			// This is counting the bits.
			while (value)
			{
				(*midiInstrumentCount) += (value & 1);
				value >>= 1;
			}
		}

		channelMask = 0;
		for (s32 i = 0; i < imChannelCount - 1; i++)
		{
			ImMidiChannel* midiChannel = &s_midiChannels[i];
			if (player == midiChannel->sharedPart)
			{
				channelMask |= s_channelMask[midiChannel->sharedPartChannelId];
			}
		}

		for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
		{
			u32 value = s_midiInstrumentChannelMask2[i] & channelMask;
			soundMidiInstrumentMask[i] |= value;

			while (value)
			{
				(*midiInstrumentCount) += (value & 1);
				value >>= 1;
			}
		}
	}

	s32 ImMidiGetTickDelta(PlayerData* playerData, u32 prevTick, u32 tick)
	{
		const u32 ticksPerMeasure = playerData->ticksPerBeat * playerData->beatsPerMeasure;

		// Compute the previous midi tick.
		u32 prevMeasure = prevTick >> 14;
		u32 prevMeasureTick = prevMeasure * ticksPerMeasure;

		u32 prevBeat = (prevTick & 0xf0000) >> 16;
		u32 prevBeatTick = prevBeat * playerData->ticksPerBeat;
		u32 prevMidiTick = prevMeasureTick + prevBeatTick + (prevTick & 0xffff);

		// Compute the current midi tick.
		u32 curMeasure = tick >> 14;
		u32 curMeasureTick = curMeasure * ticksPerMeasure;

		u32 curBeat = (tick & 0xf0000) >> 16;
		u32 curBeatTick = curBeat * playerData->ticksPerBeat;
		u32 curMidiTick = curMeasureTick + curBeatTick + (tick & 0xffff);

		return curMidiTick - prevMidiTick;
	}

	void ImMidiProcessSustain(PlayerData* playerData, u8* sndData, MidiCmdFunc* midiCmdFunc, ImMidiPlayer* player)
	{
		s_midiTrackEnd = 0;

		u8* data = sndData + playerData->chunkOffset + playerData->chunkPtr;
		s_midiTickDelta = ImMidiGetTickDelta(playerData, playerData->prevTick, playerData->tick);

		while (!s_midiTrackEnd)
		{
			u8 value = data[0];
			u8 msgFuncIndex;
			u32 msgSize;
			if (value < 0xf0 && (value & 0x80))
			{
				u8 msgType = (value & 0x70) >> 4;
				u8 channel = (value & 0x0f);
				if (midiCmdFunc[msgType].func2)
				{
					midiCmdFunc[msgType].func2(player, channel, data[1], data[2]);
				}
				msgSize = s_midiMessageSize2[msgType];
			}
			else
			{
				if (value == 0xf0)
				{
					msgFuncIndex = IM_MID_SYS_FUNC;
				}
				else if (value != 0xff)
				{
					TFE_System::logWrite(LOG_ERROR, "iMuse", "su unknown  msg type 0x%x.", value);
					return;
				}
				else
				{
					msgFuncIndex = IM_MID_EVENT;
					data++;
				}
				if (midiCmdFunc[msgFuncIndex].func1)
				{
					midiCmdFunc[msgFuncIndex].func1(playerData, data);
				}
				data++;
				msgSize = midi_getVariableLengthValue(&data);
			}
			data += msgSize;
			// Length of message in "ticks"
			s_midiTickDelta += midi_getVariableLengthValue(&data);
		}
	}

	void ImJumpSustain(ImMidiPlayer* player, u8* sndData, PlayerData* playerData1, PlayerData* playerData2)
	{
		for (s32 i = 0; i < imChannelCount; i++)
		{
			SoundChannel* channel = &player->channels[i];
			s32 trim = 0;
			if (channel)
			{
				trim = 1 << channel->partTrim;
			}
			s_midiChannelTrim[i] = trim;
		}

		// Remove instruments based on the midi channel 'trim'.
		ImMidiGetSoundInstruments(player, s_curMidiInstrumentMask, &s_curInstrumentCount);
		InstrumentSound* instrInfo = *s_imActiveInstrSounds;
		while (instrInfo && s_curInstrumentCount)
		{
			if (instrInfo->soundPlayer == player)
			{
				s32 mask = s_curMidiInstrumentMask[instrInfo->instrumentId];
				s32 trim = s_midiChannelTrim[instrInfo->channelId];
				if (trim & mask)
				{
					s_curMidiInstrumentMask[instrInfo->instrumentId] &= ~trim;
					s_curInstrumentCount--;
				}
			}
			instrInfo = instrInfo->next;
		}

		if (s_curInstrumentCount)
		{
			ImMidiProcessSustain(playerData1, sndData, s_jumpMidiCmdFunc2, player);

			// Make sure all notes were turned off during the sustain, and if not then clean up or there will be hanging notes.
			if (s_curInstrumentCount)
			{
				TFE_System::logWrite(LOG_ERROR, "iMuse", "su couldn't find all note-offs...");
				for (s32 i = 0; i < MIDI_INSTRUMENT_COUNT; i++)
				{
					if (s_curMidiInstrumentMask[i])
					{
						for (s32 t = 0; t < imChannelCount; t++)
						{
							if (s_curMidiInstrumentMask[i] & s_midiChannelTrim[t])
							{
								TFE_System::logWrite(LOG_ERROR, "iMuse", "missing note %d on chan %d...", i, s_curMidiInstrumentMask[i]);
								ImMidiNoteOff(player, t, i, 0);
							}
						}
					}
				}
			}
		}

		s_trackTicksRemaining = 0;
		instrInfo = *s_imActiveInstrSounds;
		while (instrInfo)
		{
			s32 curTick = instrInfo->curTick;
			if (curTick > s_trackTicksRemaining)
			{
				s_trackTicksRemaining = curTick;
			}
			instrInfo = instrInfo->next;
		}
		ImMidiGetSoundInstruments(player, s_curMidiInstrumentMask, &s_curInstrumentCount);
		ImMidiProcessSustain(playerData2, sndData, s_jumpMidiCmdFunc2, player);
	}

	////////////////////////////////////////
	// Midi Commands
	////////////////////////////////////////
	void ImMidiChannelSetPgm(ImMidiChannel* midiChannel, s32 pgm)
	{
		if (midiChannel && pgm != midiChannel->pgm)
		{
			midiChannel->sharedMidiChannel->pgm = pgm;
			midiChannel->pgm = pgm;
			ImSendMidiMsg_R_(midiChannel->channelId, pgm);
		}
	}

	void ImMidiChannelSetPriority(ImMidiChannel* midiChannel, s32 priority)
	{
		if (midiChannel && priority != midiChannel->priority)
		{
			midiChannel->sharedMidiChannel->priority = priority;
			midiChannel->priority = priority;
			ImSendMidiMsg_(midiChannel->channelId, MID_GPC1_MSB, priority);
		}
	}

	void ImMidiChannelSetPartNoteReq(ImMidiChannel* midiChannel, s32 noteReq)
	{
		if (midiChannel && noteReq != midiChannel->noteReq)
		{
			midiChannel->sharedMidiChannel->noteReq = noteReq;
			midiChannel->noteReq = noteReq;
			ImSendMidiMsg_(midiChannel->channelId, MID_GPC2_MSB, noteReq);
		}
	}

	void ImMidiChannelSetPan(ImMidiChannel* midiChannel, s32 pan)
	{
		if (midiChannel && pan != midiChannel->pan)
		{
			midiChannel->sharedMidiChannel->pan = pan;
			midiChannel->pan = pan;
			ImSendMidiMsg_(midiChannel->channelId, MID_PAN_MSB, pan);
		}
	}

	void ImMidiChannelSetModulation(ImMidiChannel* midiChannel, s32 modulation)
	{
		if (midiChannel && modulation != midiChannel->modulation)
		{
			midiChannel->sharedMidiChannel->modulation = modulation;
			midiChannel->modulation = modulation;
			ImSendMidiMsg_(midiChannel->channelId, MID_MODULATIONWHEEL_MSB, modulation);
		}
	}

	void ImHandleChannelPan(ImMidiChannel* midiChannel, s32 pan)
	{
		if (midiChannel && pan != midiChannel->finalPan)
		{
			midiChannel->sharedMidiChannel->finalPan = pan;
			midiChannel->finalPan = pan;
			ImSetPanFine_(midiChannel->channelId, 2 * pan + 8192);
		}
	}

	void ImSetChannelSustain(ImMidiChannel* midiChannel, s32 sustain)
	{
		if (midiChannel)
		{
			midiChannel->sustain = sustain;
			if (!sustain)
			{
				for (s32 r = 0; r < MIDI_INSTRUMENT_COUNT; r++)
				{
					if (midiChannel->instrumentMask2[r] & s_channelMask[midiChannel->channelId])
					{
						// IM_TODO
					}
				}
			}
		}
	}

	void ImMidiChannelSetVolume(ImMidiChannel* midiChannel, s32 volume)
	{
		if (midiChannel && volume != midiChannel->volume)
		{
			midiChannel->sharedMidiChannel->volume = volume;
			midiChannel->volume = volume;
			ImSendMidiMsg_(midiChannel->channelId, MID_VOLUME_MSB, volume);
		}
	}
		
	void ImHandleChannelDetuneChange(ImMidiPlayer* player, SoundChannel* channel)
	{
		channel->pan = (player->transpose << 8) + player->detune + channel->pitchBend;
		ImMidiChannel* data = channel->data;
		if (data)
		{
			ImHandleChannelPan(data, channel->pan);
		}
	}

	void ImHandleChannelPitchBend(ImMidiPlayer* player, s32 channelIndex, s32 fractValue, s32 intValue)
	{
		SoundChannel* channel = &player->channels[channelIndex];
		s32 pitchBend = ((intValue << 7) | fractValue) - (64 << 7);
		if (channel->outChannelCount)
		{
			channel->pan = (channel->outChannelCount * pitchBend) >> 5;	// range -256, 256
			ImHandleChannelDetuneChange(player, channel);
		}
		else
		{
			// No sound channels, this should never be hit.
			assert(0);
		}
	}

	//////////////////////////////////
	// Midi Advance Functions
	//////////////////////////////////
	void ImCheckForTrackEnd(PlayerData* playerData, u8* data)
	{
	}

	void ImMidiJump2_NoteOn(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2)
	{
	}

	void ImMidiStopAllNotes(ImMidiPlayer* player)
	{
		for (s32 i = 0; i < imChannelCount; i++)
		{
			ImMidiCommand(player, i, MID_ALL_NOTES_OFF, 0);
		}
	}

	void ImMidiCommand(ImMidiPlayer* player, s32 channelIndex, s32 midiCmd, s32 value)
	{
	}

	void ImMidiNoteOff(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2)
	{
	}

	void ImMidiNoteOn(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2)
	{
	}

	void ImMidiProgramChange(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2)
	{
	}

	void ImMidiPitchBend(ImMidiPlayer* player, u8 channel, u8 arg1, u8 arg2)
	{
	}

	void ImMidiEvent(PlayerData* playerData, u8* chunkData)
	{
	}
}