// MIDI library bevat al veel overhead. Dus snel midi parsen is waarschijnlijk alleeen nuttig als je alles zelf programmeert.
// Voor Insterstellar communicator gewoon branching gebruiken.

#include <MIDI.h>

MIDI_CREATE_DEFAULT_INSTANCE();

//uint8_t router[17][10][127] = {0};

void (*channelRouter[4][17])(uint8_t row);
void (*typeRouter[4][32])(uint8_t row);
void (*realTimeRouter[32]);
uint8_t data_1_router[4][127] = {0};
uint8_t data_2_router[4][127] = {0};

void setup() {
    // put your setup code here, to run once:
    MIDI.begin(MIDI_CHANNEL_OMNI);

    for ( uint8_t row = 0; row < 4; row++ )
    {
        for ( uint8_t channel = 0; channel < 17; channel++ )
        {
            channelRouter[row][channel] = nothing;
        }
        for ( uint8_t type = 0; type < 32; type++ )
        {
            typeRouter[row][type] = nothing;
        }
    }
}

bool learnOn = 0;

void loop()
{
    if ( MIDI.read() )
    {
        if ( learnOn )
        {
            for ( uint8_t row = 0; row < 4; row++ )
            {
                if ( button[row] )
                {
                    switch ( MIDI.getType() )
                    {
                        case midi::NoteOn:
                        case midi::NoteOff:
                        case midi::AfterTouchPoly:
                        case midi::ControlChange:
                        case midi::AfterTouchChannel:
                        case midi::PitchBend:
                            channelRouter[row][MIDI.getChannel()] = checkType;
                            break;
                        case midi::Clock:
                        case midi::Start:
                        case midi::Continue:
                        case midi::Stop:
                            realTimeRouter[MIDI.getType() & 31] = setClockout;
                        default: break;
                    }
                }
            }
        }
        else
        {
            (*realTimeRouter[MIDI.getType() & 31])(0); // realtime messages
            
            for ( uint8_t row = 0; row < 4; row++ )
            {
                (*channelRouter[row][MIDI.getChannel()])(row);
            }
        }
    }
}

void checkType(uint8_t row)
{
    (*typeRouter[row][MIDI.getType() >> 4])(row);
}

void checkData_1(uint8_t row)
{
    data_1_router[row][MIDI.getData1()];
}

void checkData_2(uint8_t row)
{
    data_2_router[row][MIDI.getData2()];
}

void setCVout(uint8_t row)
{
}

void setGateOut(uint8_t row)
{
}

void setClockout(uint8_t row)
{
}

void nothing(uint8_t row)
{
    return;
}
