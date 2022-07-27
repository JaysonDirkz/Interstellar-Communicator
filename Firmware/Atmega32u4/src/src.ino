#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

//#define DEBUG
#undef DEBUG
#include "debug.h"
#include "ActiveSensing.h"

#include <DirkXoniC.h>
using namespace DirkXoniC;

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

Channels channels;

const int8_t addressAmount = 4;
const int8_t rowAmount = 4;
typedef Programmer <addressAmount, rowAmount> learn_t;
learn_t learn; //Saved in eeprom

typedef TwoDimensionalLookup <(int8_t)MessageType::Size, 16> address_t;
address_t address; //Saved in eeprom

int8_t learnToPolyMap[addressAmount] = {-1, -1, -1, -1}; //Saved in eeprom
const int8_t polyAddressAmount = 2;
typedef Polyphony <polyAddressAmount> poly_t;
poly_t polyphony; //Saved in eeprom

const uint8_t keysActiveSize = 4;
//typedef ActiveKeysSmall <10> keysActive_t;
typedef ActiveKeysFast2 <10> keysActive_t;
keysActive_t keysActive[keysActiveSize];

const uint8_t lastNoteSize = 4;
//int8_t learnToLastNoteMap[4] = {-1, -1, -1, -1}; //Saved in eeprom
//MidiLastNote lastNote[4];
//LastNoteSmall <10> lastNote[4];
typedef LastNoteFast <10> lastNote_t;
lastNote_t lastNote[lastNoteSize];

int8_t rowNote[4] = {-1, -1, -1, -1};

//Saved in eeprom
int8_t msb_numbers[4];
// int8_t lsb_numbers[4];
int8_t nrpn_msb_controls[4];
int8_t nrpn_lsb_controls[4];
bool precision[4];

// Voor PolyPressure welke fingers actief zijn en hoeveel.
int8_t learnToPressureMap[4] = {-1, -1, -1, -1}; //Saved in eeprom
//ActiveKeysSmall <10> [2];
const int8_t pressuresSize = 2;
typedef Array <int8_t, 10> pressure_t;
pressure_t pressures[pressuresSize]{0, 0};
//int8_t pressures[2][128];

// 5 als minimum want midi clock pulse lengte bij 360 bpm is 60000 / 360*24 = 6,94 ms
const uint8_t PULSE_LENGTH_MS = 5;

void setup()
{
	setinputsandoutputs();
	configuremidisethandle();
   
	load_learn_status();
    writeGlobalAddresses();
    writeKeyAftertouchMapping();
    writePolyAddresses();
    checkPolyphony();
}

void setinputsandoutputs()
{
	MIDI.begin(MIDI_CHANNEL_OMNI);
//	MIDI.turnThruOff();
	
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
    MIDI.setHandleActiveSensing(active_sensing);
}

void load_learn_status()
{
    uint16_t a = 0; // eeprom address.
    EEPROM.get(a, learn); a += sizeof(learn);
    EEPROM.get(a, address); a += sizeof(address);
    EEPROM.get(a, polyphony); a += sizeof(polyphony);
    
    for ( int8_t i = 0; i < 4; ++i ) {
        EEPROM.get(a, learnToPolyMap[i]); a += sizeof(learnToPolyMap[i]);
        EEPROM.get(a, learnToPressureMap[i]); a += sizeof(learnToPressureMap[i]);
        EEPROM.get(a, msb_numbers[i]); a += sizeof(msb_numbers[i]);
        EEPROM.get(a, nrpn_msb_controls[i]); a += sizeof(msb_numbers[i]);
        EEPROM.get(a, nrpn_lsb_controls[i]); a += sizeof(msb_numbers[i]);
        EEPROM.get(a, precision[i]); a += sizeof(msb_numbers[i]);
    }

    DEBUG_LOAD_EEPROM
}

void save_learn_status()
{
    uint16_t a = 0; // eeprom address.
    EEPROM.put(a, learn); a += sizeof(learn);
    EEPROM.put(a, address); a += sizeof(address);
    EEPROM.put(a, polyphony); a += sizeof(polyphony);
    
    for ( int8_t i = 0; i < 4; ++i ) {
        EEPROM.put(a, learnToPolyMap[i]); a += sizeof(learnToPolyMap[i]);
        EEPROM.put(a, learnToPressureMap[i]); a += sizeof(learnToPressureMap[i]);
        EEPROM.put(a, msb_numbers[i]); a += sizeof(msb_numbers[i]);
        EEPROM.put(a, nrpn_msb_controls[i]); a += sizeof(msb_numbers[i]);
        EEPROM.put(a, nrpn_lsb_controls[i]); a += sizeof(msb_numbers[i]);
        EEPROM.put(a, precision[i]); a += sizeof(msb_numbers[i]);
    }

    DEBUG_SAVE_EEPROM
}

void loop()
{
	MIDI.read();
    controlThread();
    setupLearn();

    if ( activeSensing_getTimeout(700) ) activeSensing_onTimeout();
}

int8_t clockCounter = -1;
int8_t resetCounter = -1;
int8_t trigCounter[4] = {-1, -1, -1, -1};

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
        
        for ( int8_t row = 0; row < 4; ++row )
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

int8_t globalAddrCnt = 0;

void clearRowData()
{
    for ( int8_t row = 0; row < rowAmount; ++row )
    {
        rowNote[row] = -1;
    }
}

void clearKeysActive()
{
    for ( int8_t addr = 0; addr < keysActiveSize; ++addr )
    {
        keysActive[addr].reset();
    }
}

void setupLearn()
{
    for ( int8_t row = 0; row < 4; ++row )
    {
        bool buttonStates[4];
        static bool lastButtonStates[4] = {0, 0, 0, 0};

        buttonStates[row] = ~PINF & rowTo_32u4PINF_bit[row];
        int8_t deltaButtonState = buttonStates[row] - lastButtonStates[row];
        lastButtonStates[row] = buttonStates[row];
        
        if ( deltaButtonState > 0 )
        {
            delay(200); // against hysterisis

            globalAddrCnt = 0; // Reset global address counter.
            address.fill(); // Reset all global addresses.
            clearRowData(); // Reset global rowNote data.
            clearKeysActive();
            
            MIDI.setHandleNoteOn(learn_note);
            MIDI.disconnectCallbackFromType(midi::NoteOff);
            MIDI.setHandleControlChange(learn_control_change);
            MIDI.setHandlePitchBend(learn_pitchbend);
            MIDI.setHandleAfterTouchChannel(learn_atc);
            MIDI.setHandleAfterTouchPoly(learn_atp);
            MIDI.disconnectCallbackFromType(midi::Clock);
            MIDI.disconnectCallbackFromType(midi::Start);
            MIDI.disconnectCallbackFromType(midi::Continue);
            MIDI.disconnectCallbackFromType(midi::Stop);
        }
        else if ( deltaButtonState < 0 )
        {
            writeGlobalAddresses();
            writeKeyAftertouchMapping();
            writePolyAddresses();
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
            MIDI.setHandleContinue(clock_continue);
            MIDI.setHandleStop(clock_stop);
        }
    }
}

void writeGlobalAddresses()
{
    // Write addresses for learned types.
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        if ( learn.getRow(addr) > -1 )
        {
            address.set((int8_t)learn.getType(addr), learn.getChannel(addr), addr);
        }
    }
}

void writeKeyAftertouchMapping()
{
    // Reset
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        learnToPressureMap[addr] = -1;
    }

    int8_t count = 0;
    
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            for ( int8_t addr = 0; addr < addressAmount; ++addr )
            {
                if ( learn.getType(addr) == MessageType::KeysPolyPressure and learn.getChannel(addr) == channel )
                {
                    learnToPressureMap[addr] = count;
                    ++count;
                }
            }
        }
    }
}

void writePolyAddresses()
{
    // Reset
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        learnToPolyMap[addr] = -1;
    }
    
    // Include in polyphony if type and channel match more than 1 time found.
    int8_t polyAddrCount = 0;
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            for ( int8_t type = (int8_t)MessageType::Keys; type <= (int8_t)MessageType::KeysPolyPressure; ++type )
            {
                int8_t polyWidth = 0;
                
                for ( int8_t addr = 0; addr < addressAmount; ++addr )
                {
                    if ( learn.getType(addr) == (MessageType)type and learn.getChannel(addr) == channel )
                    {
                        learnToPolyMap[addr] = polyAddrCount;
                        ++polyWidth;

                        // Reset het poly adres als het mono blijkt te zijn.
                        // Niet mono? Dan poly adres counter incrementen.
                        if ( addr == address.get((int8_t)type, channel) )
                        {
                            if ( polyWidth == 1  ) learnToPolyMap[addr] = -1;
                            else ++polyAddrCount;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void checkPolyphony()
{
    for ( int8_t a = 0; a < polyAddressAmount; ++a )
    {
        // Reset width.
        polyphony.width[a] = 0;

        // Reset counter
        polyphony.counter[a] = 0;

        // Reset count and row info.
        for ( int8_t count = 0; count < 4; ++count )
        {
            polyphony.rowAtCount[a][count] = -1;

            int8_t row = count;
            polyphony.countAtRow[row] = 0;
        }
    }

    // Write all width
    for ( int8_t a = 0; a < addressAmount; ++a )
    {
        int8_t polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            ++polyphony.width[polyAddr];
        }
    }

    // Write width for KeysVelocity and KeysPolyPressure (not higher than Key width).
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            int8_t keyAddr = address.get((int8_t)MessageType::Keys, channel);
            int8_t velAddr = address.get((int8_t)MessageType::KeysVelocity, channel);
            int8_t pressAddr = address.get((int8_t)MessageType::KeysPolyPressure, channel);

            if ( keyAddr > -1 )
            {
                if ( velAddr > -1 ) polyphony.width[velAddr] = min(polyphony.width[keyAddr], polyphony.width[velAddr]);
                if ( pressAddr > -1 ) polyphony.width[pressAddr] = min(polyphony.width[keyAddr], polyphony.width[pressAddr]);
            }
        }
    }

    // Write rowAtCount
    for ( int8_t a = 0; a < addressAmount; ++a )
    {
        int8_t polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            int8_t count = polyphony.counter[polyAddr]++;
            
            if ( count < polyphony.width[polyAddr] ) polyphony.rowAtCount[polyAddr][count] = learn.getRow(a);
        }
    }

    // Write boundary.
    for ( int8_t a = 0; a < addressAmount; ++a )
    {
        int8_t polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            if ( channels.getChannelType(learn.getChannel(a)) == ChannelType::KeysSplit )
            {
                int8_t row = learn.getRow(a);
                int8_t width = polyphony.width[polyAddr];
                
                if ( width > 1 )
                {
                    // [width][row]
                    const int8_t boundariesHigh[3][4] = {
                        {63, 127, 127, 127},
                        {42, 85, 127, 127},
                        {31, 63, 95, 127}
                    };
                    
                    polyphony.boundary[row] = boundariesHigh[width - 2][row];
                }
                else polyphony.boundary[row] = 127;
            }
        }
    }
    
    // Clear all counters.
    for ( int8_t a = 0; a < polyAddressAmount; ++a )
    {
        polyphony.counter[a] = 0;
    }
}

void learn_note(uint8_t channel, uint8_t note, uint8_t velocity)
{
    --channel;
    
    // Program the note type, velocity or aftertouch for each row.
    globalAddrCnt = 0;
	for ( int8_t row = 0; row < 4; ++row )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            switch ( channels.getChannelType(channel) )
            {
                case ChannelType::KeysCycling:
                case ChannelType::KeysSplit:
                    switch ( globalAddrCnt )
                    {
                        case 0:
                            learn.program(row, MessageType::Keys, channel, row);
                        break;
                        
                        case 1:
                            learn.program(row, MessageType::KeysVelocity, channel, row);
                        break;
                        
                        case 2:
                        // Bij ontvangst van polyaftertouch wordt channelaftertouch hier naar toe veranderd.
                        // Maar kan niet terug veranderd worden naar channlpressure. Dus polypressure heeft prio.
                            learn.program(row, MessageType::ChannelPressure, channel, row);

                        // Voor note herkenning bij polypressure.
                        // Zorgt ervoor dat polypressure alleen tijdens de huidige leerfase ingeleerd kan worden. (Want rowNote wordt weer gereset.
                            rowNote[row] = note;
                        break;

                        case 3:
                            learn.program(row, MessageType::PitchBend, channel, row);
                        break;
                        
                        default:
                        break;
                    }
                break;
                
                case ChannelType::Percussion:
                    learn.program(row, MessageType::PercussionVelocity, channel, row);
                    learn.setPercNote(row, channel, note);
                break;

                default:
                break;
            }

            ++globalAddrCnt;
        }
	}
}

void learn_atc(uint8_t channel, uint8_t aftertouch)
{
//    --channel;
//
//    for ( int8_t row = 0; row < 4; ++row )
//    {
//        if (
//            learn.getChannel(row) == channel
//            and
//            learn.getType(row) == MessageType::ChannelPressure
//            and
//            (~PINF & rowTo_32u4PINF_bit[row]) > 0
//        ) {
////            address.set((int8_t)MessageType::ChannelPressure, channel, row);
//        }
//    }
}

void learn_atp(uint8_t channel, uint8_t note, uint8_t aftertouch)
{
    --channel;

    for ( int8_t row = 0; row < 4; ++row )
    {
        if (
            learn.getChannel(row) == channel
            and
            learn.getType(row) == MessageType::ChannelPressure
            and
            (~PINF & rowTo_32u4PINF_bit[row]) > 0
            and rowNote[row] == note
        ) {
            learn.program(row, MessageType::KeysPolyPressure, channel, row);
        }
    }
}

void learn_control_change(uint8_t channel, uint8_t number, uint8_t control)
{
    --channel;
    
    static bool msb_ready = false;
    static bool nrpn_ready = false;

    if ( globalAddrCnt == 0 )
    {
    for ( int8_t row = 0; row < 4; ++row )
    {
        precision[row] = false;
        
        if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            switch ( number )
            {
                case 38:
                    if ( nrpn_ready && msb_ready ) {
                        precision[row] = true;
                        nrpn_ready = false;
                        msb_ready = false;
                    }
                    break;
                case 6:
                    if ( nrpn_ready ) {
                        learn.program(row, MessageType::ControlChange, channel, row);
                        msb_ready = true;
                    }
                    break;
                case 98:
                    if ( control == 127 ) break;
                    else if ( msb_ready ) {
                        nrpn_lsb_controls[row] = control;
                        nrpn_ready = true;
                    }
                    break;
                case 99:
                    if ( control == 127 ) break;
                    else nrpn_msb_controls[row] = control;
                    break;
                default:
                    learn.program(row, MessageType::ControlChange, channel, row);
    
                    if ( msb_numbers[row] + 32 == number && msb_ready ) precision[row] = true;
                    
                    if ( msb_ready || (msb_numbers[row] == number && learn.getChannel(row) == channel) ) msb_ready = false;
                    else {
                        msb_numbers[row] = number;
                        msb_ready = true;
                    }
                    break;
            }
        }
    }
    }
}

void learn_pitchbend(uint8_t channel, int pitch)
{
    --channel;

    if ( globalAddrCnt == 0 )
    {
    for ( int8_t row = 0; row < 4; ++row )
    {
        if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn.program(row, MessageType::PitchBend, channel, row);
        }
    }
    }
}

void note_on(uint8_t channel, uint8_t note, uint8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
    --channel;

    if ( channels.getChannelType(channel) == ChannelType::Percussion )
    {
        MessageType type = MessageType::PercussionVelocity;
        if ( address.getState((int8_t)type, channel) )
        {
            int8_t globalAddr = address.get((int8_t)type, channel);
            int8_t row = learn.getRow(globalAddr);
            cv_out(row, velocity, 0);
        }
    
        for ( int8_t row = 0; row < 4; ++row )
        {
            if ( note == learn.getPercNote(row, channel) )
            {
                trigCounter[row] = PULSE_LENGTH_MS;
                gate_out(row, true);
                return;
            }
        }
    }
    else
    {
        for ( int8_t t = (int8_t)MessageType::Keys; t <= (int8_t)MessageType::KeysPolyPressure; ++t )
        {
            MessageType type = (MessageType)t;
            if ( address.getState((int8_t)type, channel) )
            {
                int8_t globalAddr = address.get((int8_t)type, channel);
                int8_t polyAddr = learnToPolyMap[globalAddr];
                
                // Schrijf row.
                int8_t row;

                if ( polyAddr > -1 )
                {
                    if ( channels.getChannelType(channel) == ChannelType::KeysCycling )
                    {
                        if ( polyphony.counter[polyAddr] >= polyphony.width[polyAddr] ) polyphony.counter[polyAddr] = 0;
                        
                        row = polyphony.rowAtCount[polyAddr][ polyphony.counter[polyAddr]++ ];
                    }
                    else row = getRowFromSplit(note, polyAddr);
                }
                else row = learn.getRow(globalAddr);


                // Output.
                static int8_t velocitySave = 0;                
                
                switch ( type )
                {
                    case MessageType::Keys:
                    {
//                        if ( keysActive[row].noteOn(note) ) // Regel alleen nodig met LastNoteSmall voor de veiligheid.
//                        {
//                            lastNote[row].noteOn(note, velocity);
//                        }
                        keysActive[row].noteOn(note);
                        lastNote[row].noteOn(keysActive[row].getPosition(note), velocity);            
                        velocitySave = lastNote[row].getVelocity();

                        int8_t positionsOfKey = lastNote[row].getPitch();
                        cv_out(row, keysActive[row].getKey(positionsOfKey), 0);
                        gate_out(row, true);
                    }
                    break;
                
                    case MessageType::KeysVelocity:
                        cv_out(row, velocitySave, 0);
                    break;
                
                    case MessageType::KeysPolyPressure:
                        learn.setRow(globalAddr, row);
                        rowNote[row] = note; // Nog nodig, want je hebt al key keysActive?

                        keysActive[row].noteOn(note);
                    default: break;
                }
            }
        }
    }
}

void note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    --channel;

    if ( channels.getChannelType(channel) == ChannelType::Percussion )
    {
        //Niks voor Perc off.
    }
    else
    {
        for ( int8_t t = (int8_t)MessageType::Keys; t <= (int8_t)MessageType::KeysPolyPressure; t = t + 1 )
        {
            MessageType type = (MessageType)t;
            
            if ( address.getState((int8_t)type, channel) )
            {
                int8_t globalAddr = address.get((int8_t)type, channel);
                int8_t polyAddr = learnToPolyMap[globalAddr];
                
                // Schrijf row.
                if ( polyAddr > -1 ) //polyphony
                {
                    if ( channels.getChannelType(channel) == ChannelType::KeysCycling )
                    {
                        for ( int8_t c = 0; c < polyphony.width[polyAddr]; ++c )
                        {
                            static int8_t keyRowHist[addressAmount] = {0, 0};
                            // Output.
                            note_off_lastStage(globalAddr, type, note, velocity, polyphony.rowAtCount[polyAddr][c], &keyRowHist[c]);

                            polyphony.counter[polyAddr] = lastNote[keyRowHist[c]].getState() ? c + 1 : c;
                        }
                    }
                    else
                    {
                        static int8_t keyRow = 0;
                        // Output.
                        note_off_lastStage(globalAddr, type, note, velocity, getRowFromSplit(note, polyAddr), &keyRow);
                    }
                }
                else
                {
                    static int8_t keyRow = 0;
                    // Output.
                    note_off_lastStage(globalAddr, type, note, velocity, learn.getRow(globalAddr), &keyRow);
                }
            }
        }
    }
}

void note_off_lastStage(int8_t globalAddr, MessageType type, int8_t note, int8_t velocity, int8_t row, int8_t* keyRowPntr)
{
    switch ( type )
    {
        case MessageType::Keys:
        {
            int8_t keyPosition = keysActive[row].getPosition(note);
            
            if ( keysActive[row].noteOff(note) )
            {
                if ( lastNote[row].noteOff(keyPosition, velocity) )
                {
                    *keyRowPntr = row;

                    int8_t keyOut = keysActive[row].getKey(lastNote[row].getPitch());
                    if ( keyOut > -1 ) cv_out(row, keyOut, 0);
                    gate_out(row, lastNote[row].getState());
                }
            }
        }
        break;

        case MessageType::KeysVelocity:
            cv_out(row, lastNote[*keyRowPntr].getVelocity(), 0);
        break;

        case MessageType::KeysPolyPressure:
        {
            keysActive[row].noteOff(note);
            
            if ( lastNote[*keyRowPntr].getState() )
            {
                learn.setRow(globalAddr, row);
                int8_t positionOfLastKey = lastNote[*keyRowPntr].getPitch();
                int8_t lastKey = keysActive[*keyRowPntr].getKey(positionOfLastKey);
                rowNote[row] = lastKey; // Nog nodig, want je hebt al key keysActive?

                int8_t atpAddr = learnToPressureMap[row];
                if ( atpAddr > -1 ) cv_out(row, pressures[atpAddr].get(keysActive[*keyRowPntr].getPosition(lastKey)), 0);
            }
            else rowNote[row] = -1; // Nog nodig, want je hebt al key keysActive?
        }
        default: break;
    }
}

int8_t getRowFromSplit(int8_t note, int8_t polyAddr)
{
    for ( int8_t c = 0; polyphony.width[polyAddr]; ++c )
    {
        int8_t rowTemp = polyphony.rowAtCount[polyAddr][c];
        
        if ( note <= polyphony.boundary[rowTemp] )
        {
            return rowTemp;
        }
    }

    return -1;
}

void atc(uint8_t channel, uint8_t aftertouch)
{
    --channel;
    
    MessageType type = MessageType::ChannelPressure;

    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        cv_out(learn.getRow(globalAddr), aftertouch, 0);
    }
}

void atp(uint8_t channel, uint8_t note, uint8_t aftertouch)
{
    --channel;

    MessageType type = MessageType::KeysPolyPressure;
    
    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        int8_t polyAddr = learnToPolyMap[globalAddr];
        
        if ( polyAddr > -1 )
        {
            for ( int8_t c = 0; c < polyphony.width[polyAddr]; ++c )
            {
                int8_t row = polyphony.rowAtCount[polyAddr][c];

                int8_t atpAddr = learnToPressureMap[row];
                int8_t keyPos = keysActive[row].getPosition(note);
                if ( atpAddr > -1 and keyPos > -1 ) pressures[atpAddr].set(keyPos, aftertouch);
                
                if (  note == rowNote[row] ) cv_out(row, aftertouch, 0);
            }
        }
        else
        {
            int8_t row = learn.getRow(globalAddr);

            int8_t atpAddr = learnToPressureMap[row];
            int8_t keyPos = keysActive[row].getPosition(note);
            if ( atpAddr > -1 and keyPos > -1 ) pressures[atpAddr].set(keyPos, aftertouch);
            if (  note == rowNote[row] ) cv_out(row, aftertouch, 0);
        }
    }
}

//test deze

void control_change(uint8_t channel, uint8_t number, uint8_t control)
{
    --channel;
    
    static int8_t nrpn_iter[4] = {0, 0, 0, 0};
    static int8_t msb_controller[4] = {-1, -1, -1, -1};
    
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
    
	for ( int8_t row = 0; row < 4; ++row )
	{
		bool cv_main = channel == learn.getChannel(row) && MessageType::ControlChange == learn.getType(row);
        
		bool msb_num = msb_numbers[row] == number;
		bool lsb_num = msb_numbers[row] + 32 == number;
        
		bool nrpn_msb_ctrl = nrpn_msb_controls[row] == control;
		bool nrpn_lsb_ctrl = nrpn_lsb_controls[row] == control;
        
		switch ( nrpn_iter[row] << 7 | (nrpn_lsb_ctrl & nrpn_98) << 6 | (nrpn_msb_ctrl & nrpn_99) << 5 | lsb_38 << 4 | msb_6 << 3 | lsb_num << 2 | msb_num << 1 | cv_main )
		{
			case NRPN_99_CTRL:
				return nrpn_iter[row] = 1;
			case NRPN_98_CTRL:
				return nrpn_iter[row] = 2;
			case MSB_6:
				if ( precision[row] ) return msb_controller[row] = control;
				nrpn_iter[row] = 0;
				return cv_out(row, control, 0);
			case LSB_38:
				nrpn_iter[row] = 0;
				return cv_out(row, msb_controller[row] << 7 | control, 6);
			case MSB:
				if ( precision[row] ) return msb_controller[row] = control;
				return cv_out(row, control, 0);
			case LSB:
				return cv_out(row, msb_controller[row] << 7 | control, 6);
			default:
			    break;
		}
	}
}

void pitchbend(uint8_t channel, int pitch)
{
    --channel;
    
	MessageType type = MessageType::PitchBend;

    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        cv_out(learn.getRow(globalAddr), pitch + 8192, 6);
    }
}


void cv_out(int8_t row, int number, int8_t rightShift)
{
	uint8_t value = number >> rightShift;
	switch (row)
	{
		case 0: PWM6 = value; break;
		case 1: PWM9 = value; break;
		case 2: PWM10 = value; break;
		case 3: PWM13 = value; break;
        default: break;
	}
	
	DEBUG_CV_OUT
}

void gate_out(int8_t row, bool state)
{
	const int8_t GATES[4] = {GATE_1_PIN, GATE_2_PIN, GATE_3_PIN, GATE_4_PIN};
	digitalWrite(GATES[row], state);

    DEBUG_GATE_OUT
}

void clock_tick()
{
    digitalWrite(CLOCK_PIN, HIGH);
    clockCounter = PULSE_LENGTH_MS;
    activeSensing_update();
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

void active_sensing()
{
    activeSensing_update();
}

void activeSensing_onTimeout()
{
    for ( int8_t i = 0; i < keysActiveSize; ++i ) keysActive[i].reset();
    for ( int8_t i = 0; i < lastNoteSize; ++i ) lastNote[i].reset();
    digitalWrite(CLOCK_PIN, LOW);
    digitalWrite(RESET_PIN, LOW);
    gate_out(0, LOW);
    gate_out(1, LOW);
    gate_out(2, LOW);
    gate_out(3, LOW);

    DEBUG_ACTIVE_TIMEOUT
}
