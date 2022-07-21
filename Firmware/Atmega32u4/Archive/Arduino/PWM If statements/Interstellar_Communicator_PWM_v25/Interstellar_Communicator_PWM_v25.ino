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
- Wellicht nog een extra reeks kanalen toevoegen die gebruik maken van lastnote.
- Polyphony werkt nog niet
- Polyphony check na elke learn fase (niet alleen van noteOn), en elke kanaal er van checken.
- Test op glitches wanneer je veel midi ontvangt.
	- Glitches? Kijk of dit verdwijnt met midiThruOff.
		- Ja? Kijk of je midithru in hardware kan door-routen.
- Active sensing timeout nu niet gebruikt. Blijf testen om te kijken of er noten blijven hangen. Zo niet? Dan kan het zo blijven.
*/

#include <MIDI.h>
#include <EEPROM.h>
#include <MidiLastNote.h>

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
    None, // Te veel ram? Deze kan weg. Gebruik "Size".
    Keys,// Te veel ram? Deze kan weg. Gebruik None of Size van KeyType class.
    ChannelPressure,
    ControlChange,
    PitchBend,
    Size
};

enum class KeyType: char {
    None, // Te veel ram? Deze kan weg. Gebruik "Size"
    PercussionVelocity,
    KeysPitch,
    KeysVelocity,
    KeysPolyPressure,
    Size
};

enum class ChannelType: char {
    None,
    KeysCycling,
    Percussion,
    KeysSplit,
    Size
};

class Channels {
    private:
    ChannelType channelTypes[16];
    
    public:
    Channels()
    {
        for ( char c = 0; c < 16; ++c )
        {
            if ( c >= 0 and c <= 7 ) channelTypes[c] = ChannelType::KeysCycling;
            else if ( c >= 8 and c <= 11 ) channelTypes[c] = ChannelType::Percussion;
            else if ( c >= 12 and c <= 15 ) channelTypes[c] = ChannelType::KeysSplit;
        }
    }

    ChannelType getChannelType(char c)
    {
        return channelTypes[c];
    }
};
Channels channels;

//enum class { NEG = -1, NOTE, VELOCITY, ATC, ATP, CONTROL, BEND, PENDING_AT };
const char CV_RSHIFT[(char)Type::Size] = {0, 0, 0, 6, 6};

template <const char typeAmount, const char adressAmount, const char midiChannelsAmount = 16, const char rowAmount = 4> class Polyphony {
    public:
    char adress[typeAmount][midiChannelsAmount];
    char counter[adressAmount];
    char width[adressAmount];
    char countAtRow[adressAmount][rowAmount]; // row kan misschien weg?
    char rowAtCount[adressAmount][rowAmount];
    char boundary[rowAmount];

    Polyphony()
    {
        for ( char t = 0; t < typeAmount; ++t )
        {
            for ( char c = 0; c < midiChannelsAmount; ++c )
            {
                adress[t][c] = -1;
            }
        }

        for ( char a = 0; a < adressAmount; ++a )
        {
            counter[a] = 0;
            width[a] = 0;

            for ( char r = 0; r < 4; ++r )
            {
                countAtRow[a][r] = r;
                rowAtCount[a][r] = r;
            }
        }

        for ( char r = 0; r < 4; ++r )
        {
            boundary[r] = 127;
        }
    }
};
const char adressAmount = 4;
Polyphony <(char)KeyType::Size, adressAmount> polyphony;

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

    inline KeyType getKeyType(char channel)
    {
        return keyTypes[channel];
    }
    
    inline char getPercNote(char note)
    {
        return percNotes[note];
    }

    void setChannel(Type type, char channel)
    {
        for ( char t = 0; t < typeAmount; ++t )
        {
            channels[t] = t == (char)type ? channel : -1;
        }

        if ( type != Type::Keys )
        {
            for ( char c = 0; c < 16; ++c )
            {
                keyTypes[c] = KeyType::None;
            }
        }
    }

    void setKeyType(KeyType keyType, char channel)
    {
        for ( char c = 0; c < 16; ++c )
        {
            keyTypes[c] = c == channel ? keyType : KeyType::None;
        }

        if ( keyType == KeyType::KeysPitch )
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

MidiLastNote lastNote[4];

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

  
	for ( char row = 0; row < 4; ++row ) {
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
        
        for ( char row = 0; row < 4; ++row )
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
    for ( char row = 0; row < 4; ++row )
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
            clearRowData();
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

void clearRowData()
{
    for ( char row = 0; row < 4; ++row )
    {
        rowData[row] = -1;
    }
}

void checkPolyphony()
{
    // Reset width.
    for ( char i = 0; i < 4; ++i )
    {
        polyphony.width[i] = 0;
    }
        
    for ( char channel = 0; channel < 16; ++channel )
    {   
        // Moet eigenlijk na elke learn fase gebeuren, en gecheckt worden voor alle kanalen.
        // Write new width for Keys and PercussionVelocity ( 1 == mono ).
        for ( char row = 0; row < 4; ++row )
        {
            if ( learn[row].getChannel(Type::Keys) == channel )
            {
                KeyType keyType = learn[row].getKeyType(channel);
                char polyAdr = polyphony.adress[(char)keyType][channel];
                
                // Keys
                if ( keyType == KeyType::KeysPitch )
                {
                    polyphony.countAtRow[polyAdr][row] = polyphony.width[polyAdr];
                    polyphony.rowAtCount[polyAdr][ polyphony.width[polyAdr] ] = row;
                    
                    ++polyphony.width[polyAdr];
                    MIDI.sendNoteOn(polyphony.width[polyAdr], row, 5);
                }
        
                // PercussionVelocity: maximum of 1 (mono).
                if ( keyType == KeyType::PercussionVelocity) polyphony.width[polyAdr] = 1;
            }
        }
    
        // Write width for KeysVelocity and KeysPolyPressure (not higher than Key width).
        for ( char row = 0; row < 4; ++row )
        {
            if (learn[row].getChannel(Type::Keys) == channel )
            {
                KeyType keyType = learn[row].getKeyType(channel);
                char polyAdr = polyphony.adress[(char)keyType][channel];
                
                if ( keyType == KeyType::KeysVelocity)
                {
                    polyphony.countAtRow[polyAdr][row] = polyphony.width[polyAdr];
                    polyphony.rowAtCount[polyAdr][ polyphony.width[polyAdr] ] = row;
                    ++polyphony.width[polyAdr];
                }
                if ( keyType == KeyType::KeysPolyPressure)
                {
                    polyphony.countAtRow[polyAdr][row] = polyphony.width[polyAdr];
                    polyphony.rowAtCount[polyAdr][ polyphony.width[polyAdr] ] = row;
                    ++polyphony.width[polyAdr];
                }
            }
        }
        polyphony.width[polyphony.adress[(char)KeyType::KeysVelocity][channel]] = min(polyphony.width[polyphony.adress[(char)KeyType::KeysPitch][channel]], polyphony.width[polyphony.adress[(char)KeyType::KeysVelocity][channel]]);
        polyphony.width[polyphony.adress[(char)KeyType::KeysPolyPressure][channel]] = min(polyphony.width[polyphony.adress[(char)KeyType::KeysPitch][channel]], polyphony.width[polyphony.adress[(char)KeyType::KeysPolyPressure][channel]]);
    
        // Write the low and high boundaries.
        if ( channels.getChannelType(channel) == ChannelType::KeysSplit )
        {
            // Clear the all counters for this channel.
            for ( char keyType = 0; keyType < (char)KeyType::Size; ++keyType )
            {
                char polyAdr = polyphony.adress[keyType][channel];
                polyphony.counter[polyAdr] = 0;
            }
            
            for ( char row = 0; row < 4; ++row )
            {
                char keyType = (char)learn[row].getKeyType(channel);
                if ( keyType != (char)KeyType::None )
                {
                    char polyAdr = polyphony.adress[keyType][channel];
                    char polyWidth = polyphony.width[polyAdr];
                    if ( polyWidth > 1 )
                    {
                        const char boundariesHigh[3][4] = {
                            {63, 127, 127, 127},
                            {42, 85, 127, 127},
                            {31, 63, 95, 127}
                        };
    
                        polyphony.boundary[row] = boundariesHigh[polyWidth - 2][polyphony.counter[polyAdr]];
    
                        ++polyphony.counter[polyAdr];
                    }
                    else
                    {
                        polyphony.boundary[row] = 127;
                    }
                }
            }

            // Clear the all counters.
            for ( char i = 0; i < 4; ++i )
            {
                polyphony.counter[i] = 0;
            }
        }
    }
}

void save_learn_status()
{
	for ( char row = 0; row < 4; ++row )
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

    // Program the note type, velocity or aftertouch for each row.
    char multiTypeCounter = 0;
	for ( char row = 0; row < 4; ++row )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn[row].setChannel(Type::Keys, channel);
            
            switch ( channels.getChannelType(channel) )
            {
                case ChannelType::KeysCycling:
                case ChannelType::KeysSplit:
                    switch ( multiTypeCounter )
                    {
                        case 0:
                        learn[row].setKeyType(KeyType::KeysPitch, channel);
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

    switch ( channels.getChannelType(channel) )
    {
        case ChannelType::KeysCycling:
            note_on_keyscycling(channel, note, velocity);
        break;
        
        case ChannelType::Percussion:
            note_on_percussion(channel, note, velocity);
        break;

        case ChannelType::KeysSplit:
            note_on_keyssplit(channel, note, velocity);
        break;

        default: break;
    }
}

inline void note_on_keyscycling(char channel, char note, char velocity)
{
    Type type = Type::Keys;
    
    uint8_t firstMess = 0xFF;
    
    for ( char row = 0; row < 4; ++row )
    {
        if ( channel == learn[row].getChannel(type) )
        {
            KeyType keyType = learn[row].getKeyType(channel);

            char polyAdr = polyphony.adress[(char)keyType][channel];
            
            if ( bitRead(firstMess, (char)keyType)
                and
                row == polyphony.rowAtCount[polyAdr][ polyphony.counter[polyAdr] ] )
            {
                bitClear(firstMess, (char)keyType);
        
                if ( ++polyphony.counter[polyAdr] == polyphony.width[polyAdr] ) polyphony.counter[polyAdr] = 0;

                note_on_keyshandling(channel, note, velocity, row, keyType);
            }
        }
    }
}

inline void note_on_percussion(char channel, char note, char velocity)
{
    Type type = Type::Keys;
    
    for ( char row = 0; row < 4; ++row )
    {
        if ( note == learn[row].getPercNote(channel) )
        {
            if ( channel == learn[row].getChannel(type) )
            {
                cv_out(row, velocity, type);
            }
            
            trigCounter[row] = PULSE_LENGTH_MS;
            gate_out(row, true);
            return;
        }
    }
}

inline void note_on_keyssplit(char channel, char note, char velocity)
{
    Type type = Type::Keys;
    
    for ( char row = 0; row < 4; ++row )
    {
        if ( channel == learn[row].getChannel(type) )
        {
            KeyType keyType = learn[row].getKeyType(channel);

            if ( note <= polyphony.boundary[row] ) note_on_keyshandling(channel, note, velocity, row, keyType);
        }
    }
}

void note_on_keyshandling(char channel, char note, char velocity, char row, KeyType keyType)
{
    Type type = Type::Keys;
    
    rowData[row] = note;
    
//    MIDI.sendNoteOn(polyphony.width[polyAdr], row, 5);
//    MIDI.sendNoteOn(polyphony.counter[polyAdr], row, 5);
    
    switch ( keyType )
    {   
        case KeyType::KeysPitch:
            cv_out(row, note, type);
            gate_out(row, true);
        break;
    
        case KeyType::KeysVelocity:
            cv_out(row, velocity, type);
        break;
    
        case KeyType::KeysPolyPressure: // Only rowData[row] = note;
        default: break;
    }
}

void note_off(char channel, char note, char velocity)
{
    --channel;
    
    for ( char row = 0; row < 4; ++row )
    {
        Type type = Type::Keys;

        if ( channel == learn[row].getChannel(type) and note == rowData[row] )
        {
            KeyType keyType = learn[row].getKeyType(channel);

            char polyAdr = polyphony.adress[(char)keyType][channel];
            polyphony.counter[polyAdr] = polyphony.countAtRow[polyAdr][row];
            
            rowData[row] = -1;

            switch ( keyType )
            {   
                case KeyType::KeysPitch:
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
    
    for ( char row = 0; row < 4; ++row )
    {
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn[row].setChannel(Type::ChannelPressure, channel);
//            learn[row].channel[Type::ChannelPressure] = channel;
//            learn[row].type[channel] = Type::ChannelPressure;
        }
    }
}

void atc(char channel, char aftertouch)
{
    --channel;
    
    Type type = Type::ChannelPressure;
    
    for ( char row = 0; row < 4; ++row )
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
    
    for ( char row = 0; row < 4; ++row )
    {   
        if ( KeyType::KeysPolyPressure == learn[row].getKeyType(channel) and note == rowData[row] )
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

	for ( char row = 0; row < 4; ++row )
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
                    learn[row].setChannel(Type::ControlChange, channel);
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
                learn[row].setChannel(Type::ControlChange, channel);
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
    
	for ( char row = 0; row < 4; ++row )
	{
		if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
		{
            learn[row].setChannel(Type::PitchBend, channel);
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
