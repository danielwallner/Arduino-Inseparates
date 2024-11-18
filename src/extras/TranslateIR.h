#include "Buttons.h"

#define RUN_TRANSLATIONS

#ifdef STATEFUL_SOURCE_NEXT
const button_type_t AvailableSources[] = {
    SOURCE_PHONO,
    SOURCE_CD,
    SOURCE_TUNER,
    SOURCE_AUX,
    SOURCE_TAPE1,
    SOURCE_TAPE2,
    SOURCE_VCR,
};
#endif

class TranslateIR
{
    struct stateful_buttons
    {
        bool initialized;
#ifdef STATEFUL_SOURCE_NEXT
        uint8_t current_source;
#endif
    };
public:
    button_type_t translate_incoming(uint8_t protocol, uint8_t address, uint8_t command)
    {
#ifdef DEC_PHILIPS_DVD_AS_TUNER
        switch (protocol)
        {
        case RC5:
        case RC6:
    // ################ PHILIPS ################
            if (address == 4)
            {
                switch (command)
                {
                case 199: return TUNER_POWER;
                case 0: return TUNER_0;
                case 1: return TUNER_1;
                case 2: return TUNER_2;
                case 3: return TUNER_3;
                case 4: return TUNER_4;
                case 5: return TUNER_5;
                case 6: return TUNER_6;
                case 7: return TUNER_7;
                case 8: return TUNER_8;
                case 9: return TUNER_9;
                case 131: return TUNER_BAND; // TITLE
                case 88: return TUNER_FREQ_UP; // UP
                case 89: return TUNER_FREQ_DOWN; // DOWN
                case 90: return TUNER_PRESET_UP; // RIGHT
                case 91: return TUNER_PRESET_DOWN; // LEFT
                case 44: return TUNER_TUNING_PRESET; // PLAY / PAUSE
                case 92: return TUNER_PRESET_SHIFT; // OK
                case 247: return TUNER_PRESET_SHIFT_A; // ZOOM
                case 29: return TUNER_PRESET_SHIFT_B; // REPEAT
                case 59: return TUNER_PRESET_SHIFT_C; // REPEAT A B
                case 75: return TUNER_MEMORY; // SUBTITLE
                case 78: return TUNER_MEMORY_SCAN; // AUDIO
                case 49: return TUNER_HITS; // STOP
                case 15: return TUNER_DISPLAY_MODE; // DISPLAY
                case 209: return TUNER_TUNING_MODE; // MENU
                case 130: return TUNER_FM_MODE; // SETUP
                case 126: return TUNER_MUTING; // USB
                }
            }
        }
#endif

        return NO_BUTTON;
    }

    button_type_t translate_simple(button_type_t inButton)
    {
        switch (inButton)
        {
#ifdef TRANSLATE_CD_PAUSE_STOP_TO_STOP
        case CD_PAUSE_STOP: return CD_STOP;
#endif
#ifdef SINGLE_CD
        // Remap disc select to turntable start/stop
        case CD_NEXT_DISC: return PHONO_START_STOP;
#endif
#ifdef MAP_MD_TO_PHONO
        case MD_PLAY: return PHONO_START_STOP;
        case MD_STOP: return PHONO_CUE;
#endif
        }

        return inButton;
    }

    button_type_t translate_stateful(button_type_t inButton_)
    {
        button_type_t inButton = translate_simple(inButton_);

        button_type_t outButton = inButton;
#ifdef STATEFUL_SOURCE_NEXT
        if (inButton == SOURCE_NEXT)
        {
            ++m_remoteState.current_source;
            if (m_remoteState.current_source > sizeof(AvailableSources) / sizeof(button_type_t))
            {
                m_remoteState.current_source = 0;
            }
            outButton = AvailableSources[m_remoteState.current_source];
        }
#endif
        return outButton;
    }

    button_type_t translate_outgoing(button_type_t button, bool isFirst)
    {
        if (IS_TAPE_BUTTON(button))
        {
#ifdef ENC_PIONEER_TAPE
#ifdef SINGLE_DECK
            uint8_t command = -1;
            switch (button)
            {
            // Remap codes that are useless on a single deck
            // These codes are known to work on a CT-S610
            case TAPE_DECK_A: command = 72; break; // COUNTER RESET
            case TAPE_DECK_B: command = 78; break; // TAPE RETURN
            case TAPE_DECK_A_B: command = 29; break; // MONITOR
            case TAPE_DIR_A: command = 82; break; // OPEN CLOSE
            case TAPE_DIR_B: command = 71; break; // COUNTER MODE
            }
            if (command != -1)
            {
                if (isFirst)
                {
                    IrSender.sendNEC(161, command, 0);
                }
                else
                {
                    IrSender.sendNECRepeat();
                }
                return 110;
            }
#endif
#endif
        }

        return NO_BUTTON;
    }

private:
    stateful_buttons m_remoteState = {0};
};

TranslateIR TIR;
