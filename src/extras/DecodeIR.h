#include "Buttons.h"

// Main sources of IR codes:
// https://sourceforge.net/p/lirc-remotes/code/ci/master/tree/remotes/
// https://www.marantz.com/-/media/files/documentmaster/marantzna/us/marantz-2014-ir-command-sheet.xls

bool decode_ir(uint8_t protocol, uint8_t numberOfBits, uint8_t address, uint8_t command, uint8_t extra, button_type_t *out)
{
    button_type_t button = NO_BUTTON;

#ifdef RUN_TRANSLATIONS
    *out = TIR.translate_incoming();
#else
    *out = NO_BUTTON;
#endif

    switch (protocol)
    {
#ifdef DEC_BEO
    case BANG_OLUFSEN:
        {
            switch (command)
            {
            case 12: button = SLEEP; break;
            case 96: button = VOLUME_UP; break;
            case 100: button = VOLUME_DOWN; break;
            case 13: button = VOLUME_MUTE; break;
            case 128: button = SOURCE_TV; break;
            case 129: button = SOURCE_TUNER; break;
            case 131: button = SOURCE_AUX; break;
            case 133: button = SOURCE_VCR; break;
            case 134: button = SOURCE_DVD; break;
            case 145: button = SOURCE_TAPE1; break;
            case 146: button = SOURCE_CD; break;
            case 147: button = SOURCE_PHONO; break;
            case 148: button = SOURCE_TAPE2; break;

            // Debugging
#if 1
            case 50: button = TUNER_PRESET_SHIFT; break; // Left
            case 52: button = TAPE_PLAY_A; break; // Right
            case 30: button = TUNER_PRESET_UP; break; // Up
            case 31: button = TUNER_PRESET_DOWN; break; // Down
            case 213: button = CD_STOP; break; // Green
            case 212: button = CD_PLAY; break; // Yellow
            case 216: button = TAPE_FFWD_A; break; // Blue
            case 217: button = TAPE_REW_A; break; // Red
#endif
            }
        }
        break;
#endif

    case DENON:
// ################ DENON ################
#ifdef DEC_DENON
        switch (address)
        {
        case 4:
            // Tape
            switch (command)
            {
            case 90: button = TAPE_FFWD_A; break;
            case 218: button = TAPE_REW_A; break;
            case 58: button = TAPE_PLAY_A; break;
            case 186: button = TAPE_PAUSE_A; break;
            case 122: button = TAPE_STOP_A; break;
            case 250: button = TAPE_REC_PAUSE_A; break;
            case 206: button = TAPE_OPEN_A; break;
            }
            break;
        }
#endif
        break;

    case RC5:
    case RC6:
// ################ PHILIPS ################
#ifdef DEC_PHILIPS
        // Philips,Marantz system remotes RH6624,RH6640/01
        if (address == 16)
        {
            switch (command)
            {
            case 12:
                if (numberOfBits == 13)
                {
                    button = SYSTEM_POWER_TOGGLE; break;
                }
                break;
            case 38: button = SLEEP; break;
            case 16: button = VOLUME_UP; break;
            case 17: button = VOLUME_DOWN; break;
            case 13: button = VOLUME_MUTE; break;
            case 0:
                switch (extra)
                {
                case 13: button = SOURCE_NEXT; break;
                case 6: button = SOURCE_AUX; break;
                }
                break;
            }
        }
        if (address == 05)
        {
            switch (command)
            {
            case 63: button = SOURCE_VCR; break;
            }
        }
        if (address == 21)
        {
            switch (command)
            {
            case 63: button = SOURCE_PHONO; break;
            }
        }
        if (address == 20)
        {
            switch (command)
            {
            case 63: button = SOURCE_CD; break;

            case 12: button = CD_POWER; break;
            case 1: button = CD_1; break;
            case 2: button = CD_2; break;
            case 3: button = CD_3; break;
            case 4: button = CD_4; break;
            case 5: button = CD_5; break;
            case 6: button = CD_6; break;
            case 7: button = CD_7; break;
            case 8: button = CD_8; break;
            case 9: button = CD_9; break;
            case 0: button = CD_0; break;
            case 52: button = CD_FFWD; break;
            case 50: button = CD_REW; break;
            case 32: button = CD_NEXT; break;
            case 33: button = CD_PREV; break;
            case 53: button = CD_PLAY; break;
            case 48: button = CD_PAUSE; break;
            case 54: button = CD_STOP; break;
            case 28: button = CD_RANDOM; break;
            case 29: button = CD_REPEAT; break;
            case 30: button = CD_NEXT_DISC; break;
            case 45: button = CD_OPEN_CLOSE; break;
            }
        }
        if (address == 17)
        {
            switch (command)
            {
            case 63: button = SOURCE_TUNER; break;

            case 12: button = TUNER_POWER; break;
            case 1: button = TUNER_1; break;
            case 2: button = TUNER_2; break;
            case 3: button = TUNER_3; break;
            case 4: button = TUNER_4; break;
            case 5: button = TUNER_5; break;
            case 6: button = TUNER_6; break;
            case 7: button = TUNER_7; break;
            case 8: button = TUNER_8; break;
            case 9: button = TUNER_9; break;
            case 0: button = TUNER_0; break;
            case 30: button = TUNER_FREQ_UP; break;
            case 31: button = TUNER_FREQ_DOWN; break;
            case 32:
                if (numberOfBits == 13)
                {
                    button = TUNER_PRESET_UP; break;
                }
                else switch (extra)
                {
                case 0: button = TUNER_PRESET_SHIFT; break;
                case 30: button = TUNER_PRESET_SHIFT_A; break;
                case 31: button = TUNER_PRESET_SHIFT_B; break;
                case 32: button = TUNER_PRESET_SHIFT_C; break;
                }
                break;
            case 33: button = TUNER_PRESET_DOWN; break;
            case 41: button = TUNER_MEMORY; break;
            case 43: button = TUNER_MEMORY_SCAN; break;
            case 15: button = TUNER_DISPLAY_MODE; break;
            case 37: button = TUNER_TUNING_MODE; break;
            }
        }
        if (address == 18)
        {
            switch (command)
            {
            case 63: button = SOURCE_TAPE1; break;

            case 12: button = TAPE_POWER; break;
            //case : button = TAPE_DIR_A; break;
            case 33: button = TAPE_FFWD_A; break; // TRACK DOWN
            case 32: button = TAPE_REW_A; break; // TRACK UP
            case 50: button = TAPE_REV_A; break; // LEFT
            //case 52: button = TAPE_FWD_A; break; // RIGHT
            case 53: button = TAPE_PLAY_A; break;
            case 48: button = TAPE_PAUSE_A; break;
            case 54: button = TAPE_STOP_A; break;
            case 55: button = TAPE_REC_PAUSE_A; break; // RECORD
            case 45: button = TAPE_OPEN_A; break;
            case 44: button = TAPE_DECK_A; break;
            case 46: button = TAPE_DECK_B; break;
            }
        }
        if (address == 23)
        {
            switch (command)
            {
            case 63: button = SOURCE_TAPE2; break;
            }
        }
#endif
        break;

    case NEC:

// ################ Harman Kardon ################
#ifdef DEC_HARMAN
        switch (address)
        {
        case (128 << 8) | 112:
            switch (command)
            {
            case 219: button = SLEEP; break;
            case 192: button = SYSTEM_POWER_TOGGLE; break; // Power
            case 193: button = VOLUME_MUTE; break;
            case 199: button = VOLUME_UP; break;
            case 200: button = VOLUME_DOWN; break;

            case 194: button = SOURCE_PHONO; break;
            case 196: button = SOURCE_CD; break;
            case 195: button = SOURCE_TUNER; break;
            case 206: button = SOURCE_AUX; break; // TV/AUX
            case 205: button = SOURCE_TAPE_MON; break; // ?
            case 204: button = SOURCE_TAPE1; break;
            //case 205: button = SOURCE_TAPE2; break; // ?
            case 202: button = SOURCE_VCR; break;

            case 6: button = CD_FFWD; break;
            case 7: button = CD_REW; break;
            case 4: button = CD_NEXT; break;
            case 5: button = CD_PREV; break;
            case 2: button = CD_PLAY; break;
            case 3: button = CD_PAUSE; break;
            case 1: button = CD_STOP; break;
            case 80: button = CD_NEXT_DISC; break;

            case 132: button = TUNER_FREQ_UP; break;
            case 133: button = TUNER_FREQ_DOWN; break;
            }
            break;
        case (130 << 8) | 114:
            switch (command)
            {
            case 2: button = TAPE_DIR_A; break; // <
            case 4: button = TAPE_FFWD_A; break;
            case 5: button = TAPE_REW_A; break;
            case 1: button = TAPE_PLAY_A; break;
            case 6: button = TAPE_PAUSE_A; break;
            case 3: button = TAPE_STOP_A; break;
            case 29: button = TAPE_DECK_A_B; break; // Select
            }
            break;
        }
#endif

// ################ NAD ################
#ifdef DEC_NAD
        // NAD might also use the SONY protocol
        if (address == (135 << 8) | 124)
        {
            switch (command)
            {
            case 128: button = SLEEP; break;
            case 200: button = SYSTEM_POWER_TOGGLE; break; // Off
            case 37: button = SYSTEM_POWER_TOGGLE; break; // Power
            case 148: button = VOLUME_MUTE; break;
            case 136: button = VOLUME_UP; break;
            case 140: button = VOLUME_DOWN; break;

            case 137: button = SOURCE_PHONO; break; // ?
            case 133: button = SOURCE_CD; break;
            case 129: button = SOURCE_TUNER; break; // Tuner FM
            case 155: button = SOURCE_AUX; break;
            case 141: button = SOURCE_TAPE1; break;
            case 145: button = SOURCE_TAPE2; break;
            case 192: button = SOURCE_VCR; break; // Video 2
            case 193: button = SOURCE_TV; break; // Video 3 / LD
            case 194: button = SOURCE_DVD; break; // Video 1 / LD

            case 12: button = CD_1; break;
            case 13: button = CD_2; break;
            case 14: button = CD_3; break;
            case 15: button = CD_4; break;
            case 16: button = CD_5; break;
            case 17: button = CD_6; break;
            case 18: button = CD_7; break;
            case 19: button = CD_8; break;
            case 21: button = CD_9; break;
            case 22: button = CD_0; break;
            case 7: button = CD_FFWD; break;
            case 4: button = CD_REW; break;
            case 6: button = CD_NEXT; break;
            case 5: button = CD_PREV; break;
            case 1: button = CD_PLAY; break;
            case 74: button = CD_PAUSE; break;
            case 2: button = CD_STOP; break;
            //case : button = CD_RANDOM; break;
            case 10: button = CD_REPEAT; break;
            //case : button = CD_PGM; break;
            //case : button = CD_CLEAR; break;
            case 72: button = CD_OPEN_CLOSE; break;
            // case 137: button = CD_NEXT_DISC; break; // ?

            case 138: button = TUNER_1; break;
            case 142: button = TUNER_2; break;
            case 146: button = TUNER_3; break;
            case 150: button = TUNER_4; break;
            case 139: button = TUNER_5; break;
            case 143: button = TUNER_6; break;
            case 147: button = TUNER_7; break;
            case 151: button = TUNER_8; break;
            case 152: button = TUNER_9; break;
            case 199: button = TUNER_0; break;
            case 221: button = TUNER_BAND; break;
            case 212: button = TUNER_FREQ_UP; break;
            case 211: button = TUNER_FREQ_DOWN; break;
            case 210: button = TUNER_PRESET_UP; break;
            case 209: button = TUNER_PRESET_DOWN; break;
            case 36: button = TUNER_PRESET_SHIFT; break; // Bank
            case 38: button = TUNER_DISPLAY_MODE; break;
            case 55: button = TUNER_FM_MODE; break; // FM mute

            case 84: button = TAPE_DIR_A; break; // Rev
            case 222: button = TAPE_DIR_B; break; // Rev
            case 86: button = TAPE_FFWD_A; break;
            case 157: button = TAPE_FFWD_B; break;
            case 87: button = TAPE_REW_A; break;
            case 158: button = TAPE_REW_B; break;
            case 83: button = TAPE_PLAY_A; break;
            case 156: button = TAPE_PLAY_B; break;
            case 82: button = TAPE_STOP_A; break;
            case 159: button = TAPE_STOP_B; break;
            case 85: button = TAPE_REC_PAUSE_A; break; // Record
            case 154: button = TAPE_REC_PAUSE_B; break; // Record
            }
        }
#endif

        // ################ PIONEER ################
#ifdef DEC_PIONEER
        // Pioneer system remotes CU-SXxxx, AXD7247
        if (address == 165)
        {
            switch (command)
            {
            case 28: button = SYSTEM_POWER_TOGGLE; break;
            case 25: button = SLEEP; break;
            case 72: button = SLEEP; break;
            case 10: button = VOLUME_UP; break;
            case 11: button = VOLUME_DOWN; break;
            case 18: button = VOLUME_MUTE; break;
            case 85: button = SOURCE_NEXT; break; // FUNCTION
            case 153: button = SOURCE_NEXT; break; // FUNCTION
            case 77: button = SOURCE_PHONO; break;
            case 76: button = SOURCE_CD; break;
            case 71: button = SOURCE_TUNER; break;
            //case : button = SOURCE_AUX; break;
            case 78: button = SOURCE_TAPE1; break;
            case 29: button = SOURCE_TAPE2; break;
            case 15: button = SOURCE_VCR; break;
            case 12: button = SOURCE_TV; break;
            //case 13: button = SOURCE_LD; break;
            case 133: button = SOURCE_DVD; break;
            //case 136: button = SOURCE_CD-R; break; // CD-R/MD
            }
        }
        if (address == 166)
        {
            switch (command)
            {
            case 28: button = SYSTEM_POWER_TOGGLE; break;
            case 25: button = SLEEP; break;
            case 205: button = TIMER_SNOOZE; break;
            case 10: button = VOLUME_UP; break;
            case 11: button = VOLUME_DOWN; break;
            //case 220: button = SFC_PRESET; break;
            //case 202: button = SMART_OPE; break;
            case 76: button = SOURCE_AUX; break; // AUX / CD II
            //case 157: button = SYSTEM_DISP; break;
            case 153: button = SOURCE_NEXT; break; // FUNCTION
            case 218: button = SOURCE_NEXT; break; // f ?
            case 72: button = TUNER_MEMORY_SCAN; break;
            //case 73: button = TUNER_FM_AM; break;
            case 68: button = CD_PLAY_PAUSE; break;
            //case 23: button = TAPE_PLAY_PAUSE/TAPE_DIR; break;
            }
        }
        if (address == 162)
        {
            switch (command)
            {
            case 28: button = CD_POWER; break;
            case 1: button = CD_1; break;
            case 2: button = CD_2; break;
            case 3: button = CD_3; break;
            case 4: button = CD_4; break;
            case 5: button = CD_5; break;
            case 6: button = CD_6; break;
            case 7: button = CD_7; break;
            case 8: button = CD_8; break;
            case 9: button = CD_9; break;
            case 0: button = CD_0; break;
            case 14: button = CD_FFWD; break;
            case 15: button = CD_REW; break;
            case 16: button = CD_NEXT; break;
            case 17: button = CD_PREV; break;
            case 23: button = CD_PLAY; break;
            case 24: button = CD_PAUSE; break;
            case 22: button = CD_STOP; break;
            case 74: button = CD_RANDOM; break;
            case 12: button = CD_REPEAT; break;
            case 13: button = CD_PGM; break;
            case 69: button = CD_CLEAR; break;
            case 81: button = CD_OPEN_CLOSE; break;
            case 29: button = CD_NEXT_DISC; break;
            case 65: button = CD_NEXT_DISC; break; // DISC SET
            case 147: button = CD_PREV_DISC; break;
            case 193: button = CD_PREV_DISC; break;
            }
        }
        if (address == 164)
        {
            switch (command)
            {
            case 28: button = TUNER_POWER; break;
            case 1: button = TUNER_1; break;
            case 2: button = TUNER_2; break;
            case 3: button = TUNER_3; break;
            case 4: button = TUNER_4; break;
            case 5: button = TUNER_5; break;
            case 6: button = TUNER_6; break;
            case 7: button = TUNER_7; break;
            case 8: button = TUNER_8; break;
            case 9: button = TUNER_9; break;
            case 0: button = TUNER_0; break;
            case 19: button = TUNER_BAND; break;
            case 86: button = TUNER_FREQ_UP; break;
            case 87: button = TUNER_FREQ_DOWN; break;
            case 16: button = TUNER_PRESET_UP; break;
            case 17: button = TUNER_PRESET_DOWN; break;
            //case : button = TUNER_PRESET_SHIFT; break;
            //case 64: button = TUNER_CLASS; break;
            //case 30: button = TUNER_MPX; break;
            //case 83: button = TUNER_RF_ATT; break;
            //case 66: button = TUNER_SEARCH; break;
            case 77: button = TUNER_MEMORY_SCAN; break;
            case 78: button = TUNER_HITS; break;
            case 74: button = TUNER_DISPLAY_MODE; break;
            case 30: button = TUNER_FM_MODE; break;
            }
        }
        if (address == 161)
        {
            switch (command)
            {
            case 28: button = TAPE_POWER; break;
            //case 91: button = TAPE_DIR_A; break;
            //case 21: button = TAPE_DIR_B; break;
            case 86: button = TAPE_FFWD_A; break;
            case 16: button = TAPE_FFWD_B; break;
            case 87: button = TAPE_REW_A; break;
            case 17: button = TAPE_REW_B; break;
            case 91: button = TAPE_REV_A; break;
            case 21: button = TAPE_REV_B; break;
            case 93: button = TAPE_PLAY_A; break; // FORWARD
            case 23: button = TAPE_PLAY_B; break; // FORWARD
            case 92: button = TAPE_STOP_A; break;
            case 22: button = TAPE_STOP_B; break;
            case 90: button = TAPE_REC_PAUSE_A; break;
            case 20: button = TAPE_REC_PAUSE_B; break;
            case 88: button = TAPE_REC_MUTE_A; break;
            case 18: button = TAPE_REC_MUTE_B; break;
            //case : button = TAPE_DECK_A_B; break;
            case 12: button = TAPE_DECK_A; break;
            case 13: button = TAPE_DECK_B; break;
            //case 76: button = TAPE_SELECT; break;
            }
        }
#endif

// ################ SANSUI ################
#ifdef DEC_SANSUI
        // Sansui system remotes RS-2000
        if (address == 186)
        {
            switch (command)
            {
            case 0: button = SYSTEM_POWER_TOGGLE; break;
            case 1: button = VOLUME_MUTE; break;
            case 3: button = VOLUME_UP; break;
            case 2: button = VOLUME_DOWN; break;

            case 4: button = SOURCE_PHONO; break;
 
            case 25: button = CD_NEXT; break;
            case 24: button = CD_PREV; break;
            case 27: button = CD_PLAY_PAUSE; break;
            case 26: button = CD_STOP; break;

            case 7: button = TUNER_BAND; break;
            //case 7: button = TUNER_P_CALL; break;

            case 14: button = SOURCE_VCR; break;
            //case 15: button = SOURCE_VCR2; break;

            case 9: button = TAPE_FFWD_A; break;
            case 17: button = TAPE_FFWD_B; break;
            case 8: button = TAPE_REW_A; break;
            case 16: button = TAPE_REW_B; break;
            case 11: button = TAPE_PLAY_A; break;
            case 19: button = TAPE_PLAY_B; break;
            case 10: button = TAPE_STOP_A; break;
            case 18: button = TAPE_STOP_B; break;
            case 20: button = TAPE_REC_PAUSE_B; break; // REC
            case 22: button = TAPE_REC_MUTE_B; break;
            }
        }
#endif

// ################ YAMAHA ################
#ifdef DEC_YAMAHA
        // Yamaha CD remotes
        if (address == 121)
        {
            switch (command)
            {
            case 6: button = CD_FFWD; break;
            case 5: button = CD_REW; break;
            case 7: button = CD_NEXT; break;
            case 4: button = CD_PREV; break;
            case 2: button = CD_PLAY; break;
            case 85: button = CD_PAUSE; break;
            case 86: button = CD_STOP; break;
            case 1: button = CD_OPEN_CLOSE; break;
            }
        }
        // Yamaha system remotes VU07410,VU07420,VU07430,VP59040
        if (address == 122)
        {
            switch (command)
            {
            case 31: button = SYSTEM_POWER_TOGGLE; break;
            case 26: button = VOLUME_UP; break;
            case 27: button = VOLUME_DOWN; break;
            case 20: button = SOURCE_PHONO; break;
            case 21: button = SOURCE_CD; break;
            case 22: button = SOURCE_TUNER; break;
            case 23: button = SOURCE_AUX; break;
            case 24: button = SOURCE_TAPE1; break;
            case 25: button = SOURCE_TAPE2; break;
            case 90: button = EQ_ON_FLAT; break;
            case 91: button = EQ_NEXT_PRESET; break;
            case 14: button = PHONO_START_STOP; break;
            case 12: button = CD_FFWD; break;
            case 13: button = CD_REW; break;
            case 10: button = CD_NEXT; break;
            case 11: button = CD_PREV; break;
            case 8: button = CD_PLAY; break;
            case 9: button = CD_PAUSE_STOP; break;
            case 79: button = CD_NEXT_DISC; break;
            case 16: button = TUNER_PRESET_UP; break;
            case 17: button = TUNER_PRESET_DOWN; break;
            case 18: button = TUNER_PRESET_SHIFT; break;
            case 7: button = TAPE_DIR_A; break;
            case 64: button = TAPE_DIR_B; break;
            case 2: button = TAPE_FFWD_A; break;
            case 1: button = TAPE_REW_A; break;
            case 0: button = TAPE_PLAY_A; break;
            case 3: button = TAPE_STOP_A; break;
            case 4: button = TAPE_REC_PAUSE_A; break;
            case 5: button = TAPE_REC_MUTE_A; break;
            case 6: button = TAPE_DECK_A_B; break;
            }
        }
        // RAX 12 is different
        if (address == 125)
        {
            switch (command)
            {
            case 224: button = SYSTEM_POWER_TOGGLE; break;
            case 227: button = SLEEP; break;
            case 228: button = VOLUME_UP; break;
            case 229: button = VOLUME_DOWN; break;
            case 233: button = SOURCE_PHONO; break;
            case 234: button = SOURCE_CD; break;
            case 235: button = SOURCE_TUNER; break;
            case 238: button = SOURCE_AUX; break;
            case 236: button = SOURCE_TAPE1; break;
            case 237: button = SOURCE_TAPE2; break;
            case 245: button = TUNER_PRESET_UP; break;
            case 246: button = TUNER_PRESET_DOWN; break;
            case 247: button = TUNER_PRESET_SHIFT; break;
            }
        }
#endif
        break;

    case SONY:

// ################ SONY ################
#ifdef DEC_SONY
        // Sony system remotes RM-Sxxx,RM-AVxxxx
        if (address == 16)
        {
            switch (command)
            {
            case 21: button = SYSTEM_POWER_TOGGLE; break;
            case 96: button = SLEEP; break;
            case 18: button = VOLUME_UP; break;
            case 19: button = VOLUME_DOWN; break;
            case 20: button = VOLUME_MUTE; break;
            case 32: button = SOURCE_PHONO; break;
            case 37: button = SOURCE_CD; break;
            case 33: button = SOURCE_TUNER; break;
            case 29: button = SOURCE_AUX; break; // ?
            case 35: button = SOURCE_TAPE1; break;
            //case 20: button = SOURCE_TAPE2; break;
            case 106: button = SOURCE_TV; break;
            case 125: button = SOURCE_DVD; break;
            case 34: button = SOURCE_VCR; break; // ?

            case 52: button = TAPE_FFWD_A; break;
            case 51: button = TAPE_REW_A; break;
            case 55: button = TAPE_REV_A; break;
            case 50: button = TAPE_PLAY_A; break;
            case 57: button = TAPE_PAUSE_A; break;
            case 56: button = TAPE_STOP_A; break;
            case 54: button = TAPE_REC_PAUSE_A; break; // REC
            case 63: button = TAPE_REC_MUTE_A; break;
            }
        }
        if (address == 12)
        {
            switch (command)
            {
            case 105: button = SOURCE_NEXT; break;
            }
        }
        if (address == 1850)
        {
            switch (command)
            {
            case 120: button = SYSTEM_UP; break;
            case 121: button = SYSTEM_DOWN; break;
            case 122: button = SYSTEM_LEFT; break;
            case 123: button = SYSTEM_RIGHT; break;
            case 124: button = SYSTEM_ENTER; break;
            case 52: button = SYSTEM_FFWD; break;
            case 51: button = SYSTEM_REW; break;
            case 49: button = SYSTEM_NEXT; break;
            case 48: button = SYSTEM_PREV; break;
            case 50: button = SYSTEM_PLAY; break;
            case 57: button = SYSTEM_PAUSE; break;
            case 56: button = SYSTEM_STOP; break;
            }
        }
        if (address == 17)
        {
            switch (command)
            {
            case 46: button = CD_POWER; break;
            case 0: button = CD_1; break;
            case 1: button = CD_2; break;
            case 2: button = CD_3; break;
            case 3: button = CD_4; break;
            case 4: button = CD_5; break;
            case 5: button = CD_6; break;
            case 6: button = CD_7; break;
            case 7: button = CD_8; break;
            case 8: button = CD_9; break;
            case 52: button = CD_FFWD; break;
            case 51: button = CD_REW; break;
            case 49: button = CD_NEXT; break;
            case 48: button = CD_PREV; break;
            case 50: button = CD_PLAY; break;
            case 57: button = CD_PAUSE; break;
            case 56: button = CD_STOP; break;
            case 53: button = CD_RANDOM; break;
            case 44: button = CD_REPEAT; break;
            case 31: button = CD_PGM; break;
//            case : button = CD_CLEAR; break;
            case 62: button = CD_NEXT_DISC; break;
            case 22: button = CD_OPEN_CLOSE; break;
            }
        }
        if (address == 15)
        {
            switch (command)
            {
            case 42: button = MD_PLAY; break;
            case 40: button = MD_STOP; break;
            }
        }
        if (address == 13)
        {
            switch (command)
            {
            case 46: button = TUNER_POWER; break;
            case 0: button = TUNER_1; break;
            case 1: button = TUNER_2; break;
            case 2: button = TUNER_3; break;
            case 3: button = TUNER_4; break;
            case 4: button = TUNER_5; break;
            case 5: button = TUNER_6; break;
            case 6: button = TUNER_7; break;
            case 7: button = TUNER_8; break;
            case 8: button = TUNER_9; break;
            case 9: button = TUNER_0; break;
            case 15: button = TUNER_BAND; break;
            case 18: button = TUNER_FREQ_UP; break;
            case 19: button = TUNER_FREQ_DOWN; break;
            case 16: button = TUNER_PRESET_UP; break;
            case 17: button = TUNER_PRESET_DOWN; break;
            case 52: button = TUNER_TUNING_PRESET; break;
            case 51: button = TUNER_PRESET_SHIFT; break;
            case 48: button = TUNER_PRESET_SHIFT_A; break;
            case 49: button = TUNER_PRESET_SHIFT_B; break;
            case 50: button = TUNER_PRESET_SHIFT_C; break;
            case 14: button = TUNER_MEMORY; break;
            case 75: button = TUNER_DISPLAY_MODE; break;
            case 23: button = TUNER_TUNING_MODE; break;
            case 33: button = TUNER_FM_MODE; break;
            case 34: button = TUNER_MUTING; break;
            }
        }
        if (address == 14)
        {
            switch (command)
            {
            case 46: button = TAPE_POWER; break;
            case 28: button = TAPE_FFWD_B; break;
            case 27: button = TAPE_REW_B; break;
            case 32: button = TAPE_REV_B; break;
            case 26: button = TAPE_PLAY_B; break;
            case 25: button = TAPE_PAUSE_B; break;
            case 24: button = TAPE_STOP_B; break;
            case 30: button = TAPE_REC_PAUSE_B; break; // REC
            case 31: button = TAPE_REC_MUTE_B; break;
            //case 6: button = TAPE_DECK_A_B; break;

            case 116: button = TAPE_PLAY_A; break; // PLAY/REV
            case 52: button = TAPE_PLAY_B; break; // PLAY/REV
            }
        }

#endif
        break;

    case PANASONIC:

// ################ TECHNICS ################
#ifdef DEC_TECHNICS
        // Technics system remotes RAK-SC304W,RAK-CH745WH,EUR643861
        if (address == 10)
        {
            switch (command)
            {
            case 32: button = VOLUME_UP; break;
            case 33: button = VOLUME_DOWN; break;
            case 38: button = VOLUME_BALANCE_LEFT; break;
            case 39: button = VOLUME_BALANCE_RIGHT; break;
            case 50: button = VOLUME_MUTE; break;
            //case 194: button = SBASS; break;

            case 144: button = SOURCE_PHONO; break;
            case 148: button = SOURCE_CD; break;
            case 146: button = SOURCE_TUNER; break;
            case 153: button = SOURCE_AUX; break; // EXT
            case 154: button = SOURCE_AUX; break;
            case 150: button = SOURCE_TAPE1; break;
            case 170: button = SOURCE_TAPE1; break; // TAPE MONITOR
            case 151: button = SOURCE_TAPE2; break;
            case 159: button = SOURCE_TV; break; // TV / VCR2
            case 162: button = SOURCE_DVD; break; // VDP
            case 163: button = SOURCE_DVD; break;
            case 158: button = SOURCE_VCR; break;
            }
        }
        if (address == 74)
        {
            switch (command)
            {
            case 61: button = SYSTEM_POWER_TOGGLE; break;
            case 150: button = SLEEP; break;

            case 192: button = SOURCE_CD; break; // EASY PLAY
            case 193: button = SOURCE_TAPE1; break; // EASY PLAY
            case 194: button = SOURCE_TUNER; break; // EASY PLAY

            case 16: button = TUNER_1; break;
            case 17: button = TUNER_2; break;
            case 18: button = TUNER_3; break;
            case 19: button = TUNER_4; break;
            case 20: button = TUNER_5; break;
            case 21: button = TUNER_6; break;
            case 22: button = TUNER_7; break;
            case 23: button = TUNER_8; break;
            case 24: button = TUNER_9; break;
            case 25: button = TUNER_0; break;
            case 164: button = TUNER_BAND; break;
            //case : button = TUNER_FREQ_UP; break;
            //case : button = TUNER_FREQ_DOWN; break;
            case 52: button = TUNER_PRESET_UP; break;
            case 53: button = TUNER_PRESET_DOWN; break;
            //case : button = TUNER_TUNING_PRESET; break;
            //case : button = TUNER_PRESET_SHIFT; break;
            //case : button = TUNER_PRESET_SHIFT_A; break;
            //case : button = TUNER_PRESET_SHIFT_B; break;
            //case : button = TUNER_PRESET_SHIFT_C; break;
            //case : button = TUNER_MEMORY; break;
            //case : button = TUNER_MEMORY_SCAN; break;
            //case : button = TUNER_HITS; break;
            case 85: button = TUNER_DISPLAY_MODE; break;
            //case : button = TUNER_TUNING_MODE; break;
            case 51: button = TUNER_FM_MODE; break; // AUTO / MONO
            //case : button = TUNER_MUTING; break;
            //case 209: button = TUNER_SEARCH; break;
            //case 208: button = TUNER_SELECT; break;
            }
        }
        if (address == 266)
        {
            switch (command)
            {
            case 143: button = EQ_ON_FLAT; break;
            case 130: button = EQ_NEXT_PRESET; break;
            }
        }
        if (address == 234)
        {
            switch (command)
            {
            case 10: button = PHONO_START; break;
            case 0: button = PHONO_STOP; break;
            }
        }
        if (address == 170)
        {
            switch (command)
            {
            case 61: button = CD_POWER; break;
            case 16: button = CD_1; break;
            case 17: button = CD_2; break;
            case 18: button = CD_3; break;
            case 19: button = CD_4; break;
            case 20: button = CD_5; break;
            case 21: button = CD_6; break;
            case 22: button = CD_7; break;
            case 23: button = CD_8; break;
            case 24: button = CD_9; break;
            case 25: button = CD_0; break;
            case 3: button = CD_FFWD; break;
            case 2: button = CD_REW; break;
            case 74: button = CD_NEXT; break;
            case 73: button = CD_PREV; break;
            case 10: button = CD_PLAY; break;
            case 6: button = CD_PAUSE; break;
            case 0: button = CD_STOP; break;
            case 77: button = CD_RANDOM; break;
            case 71: button = CD_REPEAT; break;
            case 138: button = CD_PGM; break;
            case 164: button = CD_NEXT_DISC; break;
            case 1: button = CD_OPEN_CLOSE; break;
            //case 85: button = CD_TIME_MODE; break;
            }
        }
        if (address == 138)
        {
            switch (command)
            {
            case 3: button = TAPE_FFWD_A; break;
            case 2: button = TAPE_REW_A; break;
            case 11: button = TAPE_REV_A; break;
            case 10: button = TAPE_PLAY_A; break;
            case 6: button = TAPE_PAUSE_A; break;
            case 0: button = TAPE_STOP_A; break;
            case 8: button = TAPE_REC_PAUSE_A; break; // REC
            case 130: button = TAPE_REC_MUTE_A; break; // AUTO REC MUTE
            case 1: button = TAPE_OPEN_A; break;
            case 149: button = TAPE_DECK_A_B; break;
            }
        }
 #endif
        break;
    }

    if (button == NO_BUTTON)
    {
        return false;
    }

#ifdef RUN_TRANSLATIONS
    *out = TIR.translate_stateful(button);
#else
    *out = button;
#endif

    return true;
}
