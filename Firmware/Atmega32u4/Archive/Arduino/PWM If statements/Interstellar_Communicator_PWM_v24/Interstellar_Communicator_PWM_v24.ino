/*
Logboek:
Interstellar_Communicator_PWM_v24 [3-4-22]:
- Alle logic omgebouwd.
Interstellar_Communicator_PWM_v22 [22-2-22]:
- Gedeeltelijk Debugged.
Interstellar_Communicator_PWM_v21 [16-1-22]:
- Aangepast voor unipolaire CV.
Interstellar_Communicator_PWM_v20 [3-6-21]:
- Logboek aangemaakt.
- Midi clock ticks veranderd naar altijd output.

TODO:
- Polyphony werkt nog niet
- Polyphony check na elke learn fase (niet alleen van noteOn), en elke kanaal er van checken.
- Test op glitches wanneer je veel midi ontvangt.
	- Glitches? Kijk of dit verdwijnt met midiThruOff.
		- Ja? Kijk of je midithru in hardware kan door-routen.
- Active sensing timeout nu niet gebruikt. Blijf testen om te kijken of er noten blijven hangen. Zo niet? Dan kan het zo blijven.
*/

#include <MIDI.h>
#include <EEPROM.h>

#define CLOCK_PIN 2
#define RESET_PIN 3
#define GATE_1_PIN 4
#define GATE_2_PIN 5
#define GATE_3_PIN 7
#define GATE_4_PIN 8
#define BUTTON_1_PIN A0
#define BUTTON_2_PIN A1
#define BUTTON_3_PIN A2
#define BUTTON_4_PIN A3

MIDI_CREATE_DEFAULT_INSTANCE();

// Frequency modes for TIMER1
#define PWM62k 1   //62500 Hz

// Direct PWM change variables
#define PWM9 OCR1A
#define PWM10 OCR1B

// Frequency modes for TIMER4
#define PWM187k 1   // 187500 Hz

// Direct PWM change variables
#define PWM6 OCR4D
#define PWM13 OCR4A

// Terminal count
#define PWM6_13_MAX OCR4C

const uint8_t rowTo_32u4PINF_bit[4] = {0x80, 0x40, 0x20, 0x10};

enum class Type: char {
    None,
    Keys,
    ChannelPressure,
    ControlChange,
    PitchBend,
    Size
};

enum class KeyType: char {
    None,
    PercussionVelocity,
    Keys,
    KeysVelocity,
    KeysPolyPressure,
    Size
};

//enum class { NEG = -1, NOTE, VELOCITY, ATC, ATP, CONTROL, BEND, PENDING_AT };
const char CV_RSHIFT[(char)Type::Size] = {0, 0, 0, 6, 6};

template <char typeAmount> class Polyphony {
    public:
    char counter[typeAmount][16];
    char width[typeAmount][16];
    char boundaryLow[typeAmount][16][4];
    char boundaryHigh[typeAmount][16][4];

    Polyphony()
    {
        for ( char t = 0; t < typeAmount; ++t )
        {
            for ( char c = 0; c < 16; ++c )
            {
                counter[t][c] = 0;
                width[t][c] = 0;

                for ( char r = 0; r < 4; ++r )
                {
                    boundaryLow[t][c][r] = 0;
                    boundaryHigh[t][c][r] = 127;
                }
            }
        }
    }
};
Polyphony <(char)KeyType::Size> polyphony;

template <char typeAmount> class MidiDataType {
    private:
    char channels[typeAmount];
    Type types[16];
    KeyType keyTypes[16];
    char percNotes[16];
    
    public:
    MidiDataType()
    {
        for ( char t = 0; t < typeAmount; ++t )
        {
            channels[t] = -1;
        }
        for ( char c = 0; c < 16; ++c )
        {
            types[c] = Type::None;
            keyTypes[c] = KeyType::None;
            percNotes[c] = -1;
        }
    }

    inline char getChannel(Type t)
    {
        return channels[(char)t];
    }

//    inline Type getType(char channel)
//    {
//        return types[channel];
//    }

    inline KeyType getKeyType(char channel)
    {
        return keyTypes[channel];
    }
    
    inline char getPercNote(char note)
    {
        return percNotes[note];
    }

    void setChannelAndType(Type type, char channel)
    {
        for ( char t = 0; t < typeAmount; ++t )
        {
            channels[t] = t == (char)type ? channel : -1;
        }

//        for ( char c = 0; c < 16; ++c )
//        {
//            types[c] = c == channel ? type : Type::None;
//        }
    }

    void setKeyType(KeyType keyType, char channel)
    {
        for ( char c = 0; c < 16; ++c )
        {
            keyTypes[c] = c == channel ? keyType : KeyType::None;
        }

        if ( keyType == KeyType::Keys )
        {
            for ( char c; c < 16; ++c )
            {
                percNotes[channel] = -1;
            }
        }
    }

    void setPercNote(char channel, char note)
    {
        for ( char c = 0; c < 16; ++c )
        {
            percNotes[c] = c == channel ? note : -1;
        }
    }
};
MidiDataType <(char)Type::Size> learn[4];

char rowData[4] = {-1, -1, -1, -1};

//Saved in eeprom
//------
char msb_numbers[4];
// char lsb_numbers[4];
char nrpn_msb_controls[4];
char nrpn_lsb_controls[4];
bool precision[4];
//------

// 5 als minimum want midi clock pulse lengte bij 360 bpm is 60000 / 360*24 = 6,94 ms
const uint8_t PULSE_LENGTH_MS = 5;

void setup()
{
	setinputsandoutputs();
	configuremidisethandle();
	load_learn_status();
}

void setinputsandoutputs()
{
	MIDI.begin(MIDI_CHANNEL_OMNI);
	MIDI.turnThruOff();
	
	pinMode(CLOCK_PIN, OUTPUT);
	pinMode(RESET_PIN, OUTPUT);
	pinMode(GATE_1_PIN, OUTPUT);
	pinMode(GATE_2_PIN, OUTPUT);
	pinMode(GATE_3_PIN, OUTPUT);
	pinMode(GATE_4_PIN, OUTPUT);

	pinMode(BUTTON_1_PIN, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
	pinMode(BUTTON_2_PIN, INPUT_PULLUP);
	pinMode(BUTTON_3_PIN, INPUT_PULLUP);
	pinMode(BUTTON_4_PIN, INPUT_PULLUP);

	digitalWrite(CLOCK_PIN,LOW);
	digitalWrite(RESET_PIN,LOW);
	digitalWrite(GATE_1_PIN,LOW);
	digitalWrite(GATE_2_PIN,LOW);
	digitalWrite(GATE_3_PIN,LOW);
	digitalWrite(GATE_4_PIN,LOW);
	
	confpwm();
}

void confpwm()
{
	// Configure Timer 1 (Pins 9, 10 and 11)
	TCCR1A=1;
	TCCR1B=PWM62k|0x08;  
	TCCR1C=0;

	// Configure Timer 4 (Pins 6 and 13)
	TCCR4A=0;
	TCCR4B=PWM187k;
	TCCR4C=0;
	TCCR4D=0;
	TCCR4D=0;
	
	// PLL Configuration
	// Use 96MHz / 2 = 48MHz
	PLLFRQ=(PLLFRQ&0xCF)|0x30;
	// PLLFRQ=(PLLFRQ&0xCF)|0x10; // Will double all frequencies
	
	// Terminal count for Timer 4 PWM
	PWM6_13_MAX=255;  
		
	// Prepare pin 9 to use PWM
	PWM9=0;   // Set PWM value between 0 and 255
	DDRB|=1<<5;    // Set Output Mode B5
	TCCR1A|=0x80;  // Activate channel
	
	// Prepare pin 10 to use PWM
	PWM10=0;   // Set PWM value between 0 and 255
	DDRB|=1<<6;    // Set Output Mode B6
	TCCR1A|=0x20;  // Set PWM value
	
	// Prepare pin 6 to use PWM
	PWM6=0;   // Set PWM value between 0 and 255
	DDRD|=1<<7;    // Set Output Mode D7
	TCCR4C|=0x09;  // Activate channel D
	
	// Prepare pin 5 to use PWM
	PWM13=0;   // Set PWM value between 0 and 255
	DDRC|=1<<7;    // Set Output Mode C6
	TCCR4A=0x82;  // Activate channel A
}

void configuremidisethandle()
{
	//When the following midi message type is recieved, the void function in between the brackets is carried out.
	MIDI.setHandleNoteOn(note_on);
	MIDI.setHandleNoteOff(note_off);
	MIDI.setHandleControlChange(control_change);
	MIDI.setHandlePitchBend(pitchbend);
	MIDI.setHandleAfterTouchChannel(atc);
	MIDI.setHandleAfterTouchPoly(atp);
	MIDI.setHandleClock(clock_tick);
	MIDI.setHandleStart(clock_start);
	MIDI.setHandleContinue(clock_continue);
	MIDI.setHandleStop(clock_stop);
//  MIDI.setHandleActiveSensing(RecievedActive);
}

void load_learn_status()
{

  
	for ( char row = 0; row < 4; row++ ) {
//		cv_type[row] = 				EEPROM.read(row);
//		cv_channel[row] = 			EEPROM.read(row+4);
//		perc_type[row] = 			EEPROM.read(row+8);
//		perc_channel[row] = 		EEPROM.read(row+12);
//		perc_num[row] = 			EEPROM.read(row+16);
//		polyphony[row + 1] = 			EEPROM.read(row+20);
//		polyphony[row + 5] = 		EEPROM.read(row+24);
//		polyphony[row + 9] = 		EEPROM.read(row+28);
//		polyphony[row + 13] = 		EEPROM.read(row+32);
//		msb_numbers[row] = 			EEPROM.read(row+36);
//		// lsb_numbers[row] = 			EEPROM.read(row+40);
//		nrpn_msb_controls[row] = 	EEPROM.read(row+44);
//		nrpn_lsb_controls[row] = 	EEPROM.read(row+48);
//		precision[row] = 			EEPROM.read(row+52);
	}
}

void loop()
{
	MIDI.read();
    controlThread();
    setupLearn();
}

char clockCounter = -1;
char resetCounter = -1;
char trigCounter[4] = {-1, -1, -1, -1};

void controlThread()
{
    auto currentMillis = millis();
    static auto lastMillis = currentMillis;
    if ( currentMillis != lastMillis )
    {
        lastMillis = currentMillis;

        switch ( clockCounter )
        {
            case -1: break;
            case 0: digitalWrite(CLOCK_PIN, LOW);
            default: --clockCounter; break;
        }

        switch ( resetCounter )
        {
            case -1: break;
            case 0: digitalWrite(RESET_PIN, LOW);
            default: --resetCounter; break;
        }
        
        for ( char row = 0; row < 4; row++ )
        {
            switch ( trigCounter[row] )
            {
                case -1: break;
                case 0: gate_out(row, LOW);
                default: --trigCounter[row]; break;
            }
        }
    }
}

void setupLearn()
{
    for ( char row = 0; row < 4; row++ )
    {
        bool buttonStates[4];
        static bool lastButtonStates[4] = {0, 0, 0, 0};

        buttonStates[row] = ~PINF & rowTo_32u4PINF_bit[row];
        char deltaButtonState = buttonStates[row] - lastButtonStates[row];
        lastButtonStates[row] = buttonStates[row];
        
        if ( deltaButtonState > 0 )
        {
            delay(200); // against hysterisis
            
            MIDI.setHandleNoteOn(learn_note);
            MIDI.disconnectCallbackFromType(midi::NoteOff);
            MIDI.setHandleControlChange(learn_control_change);
            MIDI.setHandlePitchBend(learn_pitchbend);
            MIDI.setHandleAfterTouchChannel(learn_atc);
            MIDI.disconnectCallbackFromType(midi::AfterTouchPoly);
            MIDI.disconnectCallbackFromType(midi::Clock);
            MIDI.disconnectCallbackFromType(midi::Start);
//            MIDI.disconnectCallbackFromType(midi::Continue);
            MIDI.disconnectCallbackFromType(midi::Stop);
        }
        else if ( deltaButtonState < 0 )
        {
            checkPolyphony();
            save_learn_status();
            delay(200); // against hysterisis

            MIDI.setHandleNoteOn(note_on);
            MIDI.setHandleNoteOff(note_off);
            MIDI.setHandleControlChange(control_change);
            MIDI.setHandlePitchBend(pitchbend);
            MIDI.setHandleAfterTouchChannel(atc);
            MIDI.setHandleAfterTouchPoly(atp);
            MIDI.setHandleClock(clock_tick);
            MIDI.setHandleStart(clock_start);
//            MIDI.setHandleContinue(clock_continue);
            MIDI.setHandleStop(clock_stop);
        }
    }
}

void checkPolyphony()
{
    for ( char channel = 0; channel < 16; ++channel )
    {
        enum ChannelType: char { KeysCycling, KeysSplit, Percussion };
        ChannelType channelType;
        if ( channel >= 0 and channel <= 7 ) channelType = ChannelType::KeysCycling;
        else if ( channel >= 8 and channel <= 11 ) channelType = ChannelType::Percussion;
        else if ( channel >= 12 and channel <= 15 ) channelType = ChannelType::KeysSplit;
        
        // Reset width.
        for ( char i = 0; i < (char)Type::Size; ++i )
        {
            polyphony.width[i][channel] = 0;
        }
    
        // Moet eigenlijk na elke learn fase gebeuren, en gecheckt worden voor alle kanalen.
        // Write new width for Keys and PercussionVelocity ( 1 == mono ).
        for ( char row = 0; row < 4; ++row )
        {
            if (learn[row].getChannel(Type::Keys) == channel )
            {
                // Keys
                if ( learn[row].getKeyType(channel) == KeyType::Keys )
                {
                    ++polyphony.width[(char)KeyType::Keys][channel];
                    MIDI.sendNoteOn(polyphony.width[(char)KeyType::Keys][channel], row, 5);
                }
        
                // PercussionVelocity: maximum of 1 (mono).
                if ( learn[row].getKeyType(channel) == KeyType::PercussionVelocity) polyphony.width[(char)KeyType::PercussionVelocity][channel] = 1;
            }
        }
    
        // Write width for KeysVelocity and KeysPolyPressure (not higher than Key width).
        for ( char row = 0; row < 4; ++row )
        {
            if (learn[row].getChannel(Type::Keys) == channel )
            {
                if ( learn[row].getKeyType(channel) == KeyType::KeysVelocity) ++polyphony.width[(char)KeyType::KeysVelocity][channel];
                if ( learn[row].getKeyType(channel) == KeyType::KeysPolyPressure) ++polyphony.width[(char)KeyType::KeysPolyPressure][channel];
            }
        }
        polyphony.width[(char)KeyType::KeysVelocity][channel] = min(polyphony.width[(char)KeyType::Keys][channel], polyphony.width[(char)KeyType::KeysVelocity][channel]);
        polyphony.width[(char)KeyType::KeysPolyPressure][channel] = min(polyphony.width[(char)KeyType::Keys][channel], polyphony.width[(char)KeyType::KeysPolyPressure][channel]);
    
        // Write the low and high boundaries.
        for ( char keyType = 0; keyType < (char)KeyType::Size; keyType = keyType + 1 )
        {
            char polyCount = 0;
            for ( char row = 0; row < 4; ++row )
            {
                switch ( channelType )
                {
                    case ChannelType::KeysCycling: // Already set when learned.
                    break;
                    
                    case ChannelType::Percussion: // Already set when learned.
                    break;
            
                    case ChannelType::KeysSplit:
                    {
                        char polyWidth = polyphony.width[keyType][channel];
                        if ( polyWidth > 1 )
                        {
                            char boundariesLow[3][4] = {
                                {0, 63, 0, 0},
                                {0, 42, 85, 0},
                                {0, 31, 63, 95}
                            };
                            char boundariesHigh[3][4] = {
                                {63, 127, 127, 127},
                                {42, 85, 127, 127},
                                {31, 63, 95, 127}
                            };
    
                            polyphony.boundaryLow[keyType][channel][row] = boundariesLow[polyWidth - 2][polyCount];
                            polyphony.boundaryHigh[keyType][channel][row] = boundariesHigh[polyWidth - 2][polyCount];
    
                            ++polyCount;
                        }
                        else
                        {
                            polyphony.boundaryLow[keyType][channel][row] = 0;
                            polyphony.boundaryHigh[keyType][channel][row] = 127;
                        }
                    }
                    break;
                    
                    default:
                    break;
                }
            }  
        }
    }
}

void save_learn_status()
{
	for ( char row = 0; row < 4; row++ )
	{
//		EEPROM.update(row, 	  cv_type[row]);
//		EEPROM.update(row+4,  cv_channel[row]);
//		EEPROM.update(row+8,  perc_type[row]);
//		EEPROM.update(row+12, perc_channel[row]);
//		EEPROM.update(row+16, perc_num[row]);
//		EEPROM.update(row+20, polyphony[row + 1]);
//		EEPROM.update(row+24, polyphony[row + 5]);
//		EEPROM.update(row+28, polyphony[row + 9]);
//		EEPROM.update(row+32, polyphony[row + 13]);
//		EEPROM.update(row+36, msb_numbers[row]);
//		// EEPROM.update(row+40, lsb_numbers[row]);
//		EEPROM.update(row+44, nrpn_msb_controls[row]);
//		EEPROM.update(row+48, nrpn_lsb_controls[row]);
//		EEPROM.update(row+52, precision[row]);
	}
}

void learn_note(char channel, char note, char velocity)
{
    --channel;
        
    // A different channel range will program a different note type.
    enum ChannelType: char { KeysCycling, KeysSplit, Percussion };
    ChannelType channelType;
    if ( channel >= 0 and channel <= 7 ) channelType = ChannelType::KeysCycling;
    else if ( channel >= 8 and channel <= 11 ) channelType = ChannelType::Percussion;
    else if ( channel >= 12 and channel <= 15 ) channelType = ChannelType::KeysSplit;

    // Program the note type, velocity or aftertouch for each row.
    char multiTypeCounter = 0;
	for ( char row = 0; row < 4; row++ )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn[row].setChannelAndType(Type::Keys, channel);
            
            switch ( channelType )
            {
                case ChannelType::KeysCycling:
                    polyphony.boundaryLow[keyType][channel][row] = 0;
                    polyphony.boundaryHigh[keyType][channel][row] = 127;
                case ChannelType::KeysSplit:
                    switch ( multiTypeCounter )
                    {
                        case 0:
                        learn[row].setKeyType(KeyType::Keys, channel);
                        break;
                        
                        case 1:
                        learn[row].setKeyType(KeyType::KeysVelocity, channel);
                        break;
                        
                        case 2:
                        learn[row].setKeyType(KeyType::KeysPolyPressure, channel);
                        break;
                        
                        default:
                        break;
                    }
                break;
                
                case ChannelType::Percussion:
                    learn[row].setKeyType(KeyType::PercussionVelocity, channel);
                    learn[row].setPercNote(channel, note);

                    polyphony.boundaryLow[keyType][channel][row] = note;
                    polyphony.boundaryHigh[keyType][channel][row] = note;
                break;

                default:
                break;
            }

            ++multiTypeCounter;
        }
	}
}

void note_on(char channel, char note, char velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
    --channel;
    
    for ( char row = 0; row < 4; row++ )
    {
        Type type = Type::Keys;

        if (channel == learn[row].getChannel(type) )
        {
            KeyType keyType = learn[row].getKeyType(channel);
            
            if (// Bij type == Percussion en ControlPercussion zal boundarLow en boundaryHigh hetzelfde zijn.
                channel >= polyphony.boundaryLow[(char)keyType][channel][row]
                and
                channel <= polyphony.boundaryHigh[(char)keyType][channel][row]
                and
                (
                    rowData[row] == -1
                    or polyphony.width[(char)keyType][channel] == polyphony.counter[(char)keyType][channel]
                )
            ) {
                polyphony.counter[(char)keyType][channel] += rowData[row] == -1;
                rowData[row] = note;
    
                MIDI.sendNoteOn(polyphony.width[(char)keyType][channel], row, 5);
                MIDI.sendNoteOn(polyphony.counter[(char)keyType][channel], row, 5);
    
                switch ( keyType )
                {   
                    case KeyType::Keys:
                        cv_out(row, note, type);
                        gate_out(row, true);
                    break;
    
                    case KeyType::PercussionVelocity: // Velocity to CV out
                    case KeyType::KeysVelocity:
                        cv_out(row, velocity, type);
                    break;

                    case KeyType::KeysPolyPressure: // Only rowData[row] = note;
                    default: break;
                }
            }
        }

        if ( note == learn[row].getPercNote(channel) )
        {
            trigCounter[row] = PULSE_LENGTH_MS;
            gate_out(row, true);
        }
    }
}

void note_off(char channel, char note, char velocity)
{
    --channel;
    
    for ( char row = 0; row < 4; row++ )
    {
        Type type = Type::Keys;

        if ( channel == learn[row].getChannel(type) and note == rowData[row] )
        {   
            KeyType keyType = learn[row].getKeyType(channel);
            
            --polyphony.counter[(char)keyType][channel];
            rowData[row] = -1;

            switch ( keyType )
            {   
                case KeyType::Keys:
//                    cv_out(row, note, type);
                    gate_out(row, false);
                break;

                case KeyType::PercussionVelocity:
                case KeyType::KeysVelocity:
                    cv_out(row, velocity, type);
                break;
                
                default: break;
            }
        }
// Misschien niet nodig.
//        if ( note == learn[row].percNote[channel] )
//        {
//            trigCounter[row] = 0;
//            gate_out(row, false);
//        }
    }
}

void learn_atc(char channel, char aftertouch)
{
    --channel;
    
    for ( char row = 0; row < 4; row++ )
    {
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn[row].setChannelAndType(Type::KeysChannelPressure, channel);
//            learn[row].channel[Type::KeysChannelPressure] = channel;
//            learn[row].type[channel] = Type::KeysChannelPressure;
        }
    }
}

void atc(char channel, char aftertouch)
{
    --channel;
    
    Type type = Type::KeysChannelPressure;
    
    for ( char row = 0; row < 4; row++ )
    {
        if ( channel == learn[row].getChannel(type) )
        {
            cv_out(row, aftertouch, type);
        }
    }
}

void atp(char channel, char note, char aftertouch)
{
    --channel;
    
    for ( char row = 0; row < 4; row++ )
    {   
        if ( KeyType::KeysPolyPressure == learn[row].getKeyType[channel] and note == rowData[row] )
        {
            cv_out(row, aftertouch, Type::Keys);
        }
    }
}

void learn_control_change(char channel, char number, char control)
{
    --channel;
    
	static bool msb_ready = false;
	static bool nrpn_ready = false;

	for ( char row = 0; row < 4; row++ )
	{
		precision[row] = false;
		
		if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
		{
			switch ( number )
			{
			case 38:
				if ( nrpn_ready && msb_ready )
				{
					precision[row] = true;

					nrpn_ready = false;
					msb_ready = false;
				}
				break;
			case 6:
				if ( nrpn_ready )
				{
                    learn[row].setChannelAndType(Type::ControlChange, channel);
//                    learn[row].channel[Type::ControlChange] = channel;
//                    learn[row].type[channel] = Type::ControlChange;

					msb_ready = true;
				}
				break;
			case 98:
				if ( control == 127 )
				{
					break;
				}
				else if ( msb_ready )
				{
					nrpn_lsb_controls[row] = control;

					nrpn_ready = true;
				}
				break;
			case 99:
				if ( control == 127 )
				{
					break;
				}
				else
				{
					nrpn_msb_controls[row] = control;
				}
				break;
			default:
                learn[row].setChannelAndType(Type::ControlChange, channel);
//                learn[row].channel[Type::ControlChange] = channel;
//                learn[row].type[channel] = Type::ControlChange;

				if ( msb_numbers[row] + 32 == number && msb_ready )
				{
					precision[row] = true;
				}
				
				if ( msb_ready || msb_numbers[row] == number && learn[row].getChannel(Type::ControlChange) == channel )
				{
					msb_ready = false;
				}
				else
				{
					msb_numbers[row] = number;

					msb_ready = true;
				}

				break;
			} 
		}
	}
}

void learn_pitchbend(char channel, int pitch)
{
    --channel;
    
	for ( char row = 0; row < 4; row++ )
	{
		if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
		{
            learn[row].setChannelAndType(Type::PitchBend, channel);
//            learn[row].channel[Type::PitchBend] = channel;
//            learn[row].type[channel] = Type::PitchBend;
		}
	}
}

//test deze

void control_change(char channel, char number, char control)
{
    --channel;
    
    Type type = Type::ControlChange;

    static char nrpn_iter[4] = {0, 0, 0,};
    static char msb_controller[4] = {-1, -1, -1, -1};
    
	bool msb_6 = 6 == number;
	bool lsb_38 = 38 == number;
	
	bool nrpn_99 = 99 == number;
	bool nrpn_98 = 98 == number;

	const int NRPN_99_CTRL = B00 << 7 | B0100001;
	const int NRPN_98_CTRL = B01 << 7 | B1000001;
	const int MSB_6 = B10 << 7 | B0001001;
	const int LSB_38 = B10 << 7 | B0010001;
	const int MSB = B0000011;
	const int LSB = B0000101;

	for ( char row = 0; row < 4; ++row )
	{
		bool cv_main = channel == learn[row].getChannel(type);

		bool msb_num = msb_numbers[row] == number;
		bool lsb_num = msb_numbers[row] + 32 == number;

		bool nrpn_msb_ctrl = nrpn_msb_controls[row] == control;
		bool nrpn_lsb_ctrl = nrpn_lsb_controls[row] == control;

		switch ( nrpn_iter[row] << 7 | (nrpn_lsb_ctrl & nrpn_98) << 6 | (nrpn_msb_ctrl & nrpn_99) << 5 | lsb_38 << 4 | msb_6 << 3 | lsb_num << 2 | msb_num << 1 | cv_main )
		{
			case NRPN_99_CTRL:
				nrpn_iter[row] = 1;
				return;
            break;
            
			case NRPN_98_CTRL:
				nrpn_iter[row] = 2;
				return;
            break;
             
			case MSB_6:
				if ( precision[row] )
				{
					msb_controller[row] = control;
                    return;
				}
				else
				{
					cv_out(row, (control << 6) + 64, type);
					nrpn_iter[row] = 0;
				}
//				return;
            break;
            
			case LSB_38:
				cv_out(row, msb_controller[row] << 7 | control, type);
				nrpn_iter[row] = 0;
//				return;
            break;
            
			case MSB:
				if ( precision[row] )
				{
					msb_controller[row] = control;
                  return;
				} else cv_out(row, (control << 6) + 64, type);
//				return;
            break;
            
			case LSB:
				cv_out(row, msb_controller[row] << 7 | control, type);
//				return;
            break;
            
			default:
			break;
		}
	}
}

void pitchbend(char channel, int pitch)
{
    --channel;
    
	Type type = Type::PitchBend;
    
	for ( char row = 0; row < 4; ++row )
    {
		if ( channel == learn[row].getChannel(type) )
		{
			cv_out(row, pitch, type);
		}
	}
}

void cv_out(char row, int number, Type type)
{
	uint8_t value = number >> CV_RSHIFT[(char)type];
	switch (row)
	{
		case 0: PWM6 = value; break;
		case 1: PWM9 = value; break;
		case 2: PWM10 = value; break;
		case 3: PWM13 = value; break;
	}

	 if ( !MIDI.getThruState() ) {
	 	MIDI.sendControlChange(0, value >> 7, row + 1);
	 	MIDI.sendControlChange(32, value & 127, row + 1);
	 }
}

void gate_out(char row, bool state)
{
	const char GATES[4] = {GATE_1_PIN, GATE_2_PIN, GATE_3_PIN, GATE_4_PIN};

	digitalWrite(GATES[row], state);

	 if ( !MIDI.getThruState() ) {
	 	MIDI.sendNoteOn(0, state, row + 1);
	 }
}

void clock_tick()
{
    digitalWrite(CLOCK_PIN, HIGH);
    clockCounter = PULSE_LENGTH_MS;
//    MIDI.sendRealTime(midi::Clock);
}

void clock_start()
{
	digitalWrite(RESET_PIN, HIGH);	
    resetCounter = PULSE_LENGTH_MS;
}

void clock_continue()
{
}

void clock_stop()
{
	digitalWrite(CLOCK_PIN, LOW);
	digitalWrite(RESET_PIN, LOW);
	gate_out(0, LOW);
	gate_out(1, LOW);
	gate_out(2, LOW);
	gate_out(3, LOW);
}

// void sense_timeout()
// {
// 	clock_stop();
// }
