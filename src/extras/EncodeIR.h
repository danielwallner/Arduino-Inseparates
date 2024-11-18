#include "Buttons.h"

int16_t send_ir(button_type_t button, uint8_t flags)
{
    bool isFirst = (flags & INS_FLAG_REPEAT) == 0;

#ifdef RUN_TRANSLATIONS
    {
        unsigned interval = TIR.translate_outgoing(button, isFirst);
        if (interval != 0)
        {
            return interval;
        }
    }
#endif

    {
        // Bang & Olufsen
#ifdef ENC_BEOSYSTEM
        int command = -1;
        switch (button)
        {
        case SYSTEM_POWER_TOGGLE: command = 12; break; // STANDBY
        case SLEEP: command = 12; break; // STANDBY
        case VOLUME_UP: command = 96; break;
        case VOLUME_DOWN: command = 100; break;
        case VOLUME_MUTE: command = 13; break;

        case SOURCE_PHONO: command = 147; break;
        case SOURCE_CD: command = 146; break;
        case SOURCE_TUNER: command = 129; break;
        case SOURCE_AUX: command = 131; break;
        case SOURCE_TAPE1: command = 145; break;
        case SOURCE_TAPE2: command = 148; break;
        case SOURCE_TV: command = 128; break;
        case SOURCE_DVD: command = 134; break;
        case SOURCE_VCR: command = 133; break;

        case SYSTEM_UP: command = 30; break;
        case SYSTEM_DOWN: command = 31; break;
        case SYSTEM_LEFT: command = 50; break;
        case SYSTEM_RIGHT: command = 52; break;
        case SYSTEM_PLAY: command = 53; break; // GO
        case SYSTEM_STOP: command = 54; break;

        case TUNER_0: command = 0; break;
        case TUNER_1: command = 1; break;
        case TUNER_2: command = 2; break;
        case TUNER_3: command = 3; break;
        case TUNER_4: command = 4; break;
        case TUNER_5: command = 5; break;
        case TUNER_6: command = 6; break;
        case TUNER_7: command = 7; break;
        case TUNER_8: command = 8; break;
        case TUNER_9: command = 9; break;
        case TUNER_FREQ_UP: command = 30; break; // UP
        case TUNER_FREQ_DOWN: command = 31; break; // DOWN
        case TUNER_PRESET_UP: command = 30; break; // UP
        case TUNER_PRESET_DOWN: command = 31; break; // DOWN
        case TUNER_PRESET_SHIFT: command = 32; break; // TUNE
        case TUNER_MEMORY: command = 32; break; // TUNE
        }
        if (command != -1)
        {
            INS_SEND_BEO_DATALINK((1 << 8) | command, !isFirst); // Address 1 is audio
            return 0;
        }
#endif
    }

    if (IS_CD_BUTTON(button))
    {
        // Philips CD
#ifdef ENC_PHILIPS_CD
        uint8_t command = -1;
        switch (button)
        {
        case CD_FFWD: command = 52; break;
        case CD_REW: command = 50; break;
        case CD_NEXT: command = 32; break;
        case CD_PREV: command = 33; break;
        case CD_PLAY: command = 53; break;
        case CD_STOP: command = 54; break;
        case CD_PAUSE: command = 48; break;
        case CD_PAUSE_STOP: command = 54; break; // STOP
        case CD_RANDOM: command = 28; break;
        case CD_OPEN_CLOSE: command = 45; break;
        }
        if (command != -1)
        {
            INS_SEND_RC5(20, command, isFirst);
        }
        return 114;
#endif
    }

    if (IS_TUNER_BUTTON(button))
    {
        // Sony Tuner
#ifdef ENC_SONY_TUNER
        // Most of these were found by testing codes on an ST-S590ES
        int command = -1;
        switch (button)
        {
        case TUNER_POWER: command = 46; break;
        case TUNER_1: command = 0; break;
        case TUNER_2: command = 1; break;
        case TUNER_3: command = 2; break;
        case TUNER_4: command = 3; break;
        case TUNER_5: command = 4; break;
        case TUNER_6: command = 5; break;
        case TUNER_7: command = 6; break;
        case TUNER_8: command = 7; break;
        case TUNER_9: command = 8; break;
        case TUNER_0: command = 9; break;
        case TUNER_BAND: command = 15; break;
        case TUNER_FREQ_UP: command = 18; break;
        case TUNER_FREQ_DOWN: command = 19; break;
        case TUNER_PRESET_UP: command = 16; break;
        case TUNER_PRESET_DOWN: command = 17; break;
        case TUNER_TUNING_PRESET: command = 52; break;
        case TUNER_PRESET_SHIFT: command = 51; break;
        case TUNER_PRESET_SHIFT_A: command = 48; break;
        case TUNER_PRESET_SHIFT_B: command = 49; break;
        case TUNER_PRESET_SHIFT_C: command = 50; break;
        case TUNER_MEMORY: command = 14; break;
        case TUNER_DISPLAY_MODE: command = 75; break;
        case TUNER_TUNING_MODE: command = 23; break;
        case TUNER_FM_MODE: command = 33; break;
        case TUNER_MUTING: command = 34; break;
        }
        if (command != -1)
        {
            INS_SEND_SIRC(13, command, 12);
        }
        return 45;
#endif
    }

    if (IS_TAPE_BUTTON(button))
    {
#ifdef ENC_DENON_TAPE
        // These were found by testing codes on a DRS-810
        // The IR indicator lights up for all codes + 2 divisible by 4 but the deck does not react to all these codes
        int command = -1;
        switch (button)
        {
        //case : command = 6; break; // Tape length
        //case : command = 26; break; // Music search forward
        //case : command = 30; break; // Play ?
        //case : command = 42; break; // Counter reset
        //case : command = 46; break; // Remain
        //case : command = 106; break; // Dolby NR Type
        //case : command = 134; break; // Tape monitor
        //case : command = 154; break; // Music search backwards
        //case : command = 170; break; // Memo
        //case : command = 178; break; // Dolby NR On/Off
        //case : command = 222; break; // Open close loop?
        case TAPE_FFWD_A: command = 90; break;
        case TAPE_REW_A: command = 218; break;
//        case TAPE_REC_MUTE_A: command = 18; break;
        case TAPE_REC_PAUSE_A: command = 250; break;
        //case TAPE_REV_A: command = 87; break;
        case TAPE_STOP_A: command = 122; break;
        case TAPE_PLAY_A: command = 58; break;
        case TAPE_PAUSE_A: command = 186; break;
        //case TAPE_DECK_A_B: command = 83; break;
        case TAPE_OPEN_A: command = 206; break;
        }
        if (command != -1)
        {
            INS_SEND_DENON(4, command);
        }
        return 330;
#endif

#ifdef ENC_PIONEER_TAPE
        // Pioneer Cassette Deck
        // Some of these were found by testing codes on a CT-S610
        // This deck reacts on codes named in other lists as belonging to deck B/II (base codes without adding 70)
        // Pioneer appears to consider deck II to be the main deck in this case but deck A/I in other cases
        // Hence the ability to be able to swap deck A/I and B/II below, depending on remote
        // This could be made stateful depending on what deck you did select last
        // Note that at least CU-SX039 is missing codes in the LIRC database as this remote has a shift key and can output codes for both decks
        // CU-SX039 outputs codes for deck A/I (base codes + 70) when you don't press the shift key and surprisingly no code at all for REC PAUSE when the shift key is not pressed
#ifdef SWAP_DECK_A_B
        const uint8_t deckAOffset = 0;
        const uint8_t deckBOffset = 70;
#else
        const uint8_t deckAOffset = 70;
        const uint8_t deckBOffset = 0;
#endif
        int command = -1;
        switch (button)
        {
        case TAPE_POWER: command = 28; break;
        case TAPE_FFWD_A: command = 16 + deckAOffset; break;
        case TAPE_FFWD_B: command = 16 + deckBOffset; break;
        case TAPE_REW_A: command = 17 + deckAOffset; break;
        case TAPE_REW_B: command = 17 + deckBOffset; break;
        case TAPE_REC_MUTE_A: command = 18 + deckAOffset; break;
        case TAPE_REC_MUTE_B: command = 18 + deckBOffset; break;
        case TAPE_REC_PAUSE_A: command = 20 + deckAOffset; break;
        case TAPE_REC_PAUSE_B: command = 20 + deckBOffset; break;
        case TAPE_REV_A: command = 21 + deckAOffset; break;
        case TAPE_REV_B: command = 21 + deckBOffset; break;
        case TAPE_STOP_A: command = 22 + deckAOffset; break;
        case TAPE_STOP_B: command = 22 + deckBOffset; break;
        case TAPE_PLAY_A: command = 23 + deckAOffset; break;
        case TAPE_PLAY_B: command = 23 + deckBOffset; break;
        case TAPE_PAUSE_A: command = 24 + deckAOffset; break;
        case TAPE_PAUSE_B: command = 24 + deckBOffset; break;
        case TAPE_DECK_A: command = 12; break;
        case TAPE_DECK_B: command = 13; break;
        //case TAPE_DECK_A_B: command = ; break;
        }
        if (command != -1)
        {
            if (isFirst)
            {
                INS_SEND_NEC(161, command);
            }
            else
            {
                INS_SEND_NEC_REPEAT();
            }
        }
        return 110;
#endif
    }

    return 100;
}
