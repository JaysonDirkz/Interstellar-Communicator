#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

#define DEBUG
//#undef DEBUG
#include "debug.h"
#include "ActiveSensing.h"

#include "Delta.h"
#include "Array.h"
#include "TwoDimensionalLookup.h"
#include "MidiLearn.h"
#include "MidiUtility.h"

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

typedef int8_t i8;
typedef uint8_t u8;

struct CvOut {
    uint8_t values[4] = {0, 0, 0, 0};

    uint8_t get(int8_t row)
    {
        return values[row];
    }
    
    void set(int8_t row, uint8_t value)
    {
        values[row] = value;
        
        switch (row)
        {
            case 0: PWM6 = value; break;
            case 1: PWM9 = value; break;
            case 2: PWM10 = value; break;
            case 3: PWM13 = value; break;
            default: break;
        }

//        DEBUG_OUT_FAST("cv row: xxx", 3, row);
//        DEBUG_OUT_FAST("cv val: xxx", 3, value);
        DEBUG_OUT_SUPERFAST(Debug::Cv, row, value);
    }
} cvOut;

const uint8_t rowTo_32u4PINF_bit[4] = {0x80, 0x40, 0x20, 0x10};

Channels channels;

const int8_t addressAmount = 4;
const int8_t rowAmount = 4;
typedef Programmer <addressAmount> learn_t;
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

enum CcMode {
    Absolute,
    Rel_inc1_dec127, // 1 t/m 63 increment, 127 t/m 65 decrement.
    Rel_inc65_dec63 // 63 tot 1 decrement, 65 tot 127 increment.
};

struct LearnCc {
    bool wait_for_98 = false;
    bool wait_for_6 = false;
    bool wait_for_lsb = false;
    uint16_t nrpn = 16383;
} learnCc;

//Saved in eeprom
struct Cc {
    uint16_t nrpn = -1;
    bool precision = false;
    uint8_t msb = 255;
} cc[4];

// Voor PolyPressure welke fingers actief zijn en hoeveel.
int8_t learnToPressureMap[4] = {-1, -1, -1, -1}; //Saved in eeprom
//ActiveKeysSmall <10> [2];
const int8_t pressuresSize = 2;
typedef Array <int8_t, 10> pressure_t;
pressure_t pressures[pressuresSize]{0, 0};
//int8_t pressures[2][128];

// 5 als minimum want midi clock pulse lengte bij 360 bpm is 60000 / 360*24 = 6,94 ms
const uint8_t PULSE_LENGTH_MS = 5;

unsigned long eeprom_getCalculatedCrcChecksum()
{
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for ( uint16_t index = 0 ; index < (EEPROM.length() - sizeof(crc)) ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }

  return crc;
}

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
	PWM9=0;   // Set PWM value between 0 and 255 (0 - 10,58 V)
	DDRB|=1<<5;    // Set Output Mode B5
	TCCR1A|=0x80;  // Activate channel
	
	// Prepare pin 10 to use PWM
	PWM10=0;   // Set PWM value between 0 and 255 (0 - 10,58 V)
	DDRB|=1<<6;    // Set Output Mode B6
	TCCR1A|=0x20;  // Set PWM value
	
	// Prepare pin 6 to use PWM
	PWM6=0;   // Set PWM value between 0 and 255 (0 - 10,58 V)
	DDRD|=1<<7;    // Set Output Mode D7
	TCCR4C|=0x09;  // Activate channel D
	
	// Prepare pin 5 to use PWM
	PWM13=0;   // Set PWM value between 0 and 255 (0 - 10,58 V)
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
}

void load_learn_status()
{
    unsigned long calculatedCrcChecksum = eeprom_getCalculatedCrcChecksum();
    unsigned long savedCrcChecksum;
    EEPROM.get(EEPROM.length() - sizeof(calculatedCrcChecksum), savedCrcChecksum);
    
    if ( calculatedCrcChecksum == savedCrcChecksum ) {
        uint16_t a = 0; // eeprom address.
        EEPROM.get(a, learn); a += sizeof(learn);
        EEPROM.get(a, address); a += sizeof(address);
        EEPROM.get(a, polyphony); a += sizeof(polyphony);
        
        for ( int8_t i = 0; i < 4; ++i ) {
            EEPROM.get(a, learnToPolyMap[i]); a += sizeof(learnToPolyMap[i]);
            EEPROM.get(a, learnToPressureMap[i]); a += sizeof(learnToPressureMap[i]);
            EEPROM.get(a, cc[i]); a += sizeof(cc[i]);
        }
    
        DEBUG_OUT_FAST("eeprom read, size: xxxxxx", 6, a);
    } else {
        DEBUG_OUT_FAST("crc not equal, calc: xxxxxxxxxx", 10, calculatedCrcChecksum);
        DEBUG_OUT_FAST("crc not equal, saved: xxxxxxxxxx", 10, savedCrcChecksum);
    }
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
        EEPROM.put(a, cc[i]); a += sizeof(cc[i]);
    }

    DEBUG_OUT_FAST("eeprom saved, size: xxxxxx", 6, a);

    unsigned long calculatedCrcChecksum = eeprom_getCalculatedCrcChecksum();
    EEPROM.put(EEPROM.length() - sizeof(calculatedCrcChecksum), calculatedCrcChecksum);
}

void loop()
{
	if ( MIDI.read() ) {
        activeSensing_update();
	} else {
        task_learnDelete();
        task_learn();
	}
   
    controlThread();

    static Delta <bool, int8_t> act_delta;
    if ( act_delta(activeSensing_getTimeout(550)) > 0 ) activeSensing_onTimeout();
//    if ( activeSensing_getTimeout(500) ) activeSensing_onTimeout();
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
    for ( int8_t addr = 0; addr < keysActiveSize; ++addr ) {
        keysActive[addr].reset();
    }
}

void clearLastNotes()
{
    for ( int8_t addr = 0; addr < lastNoteSize; ++addr ) {
        lastNote[addr].reset();
    }
}

inline u8 get_bitfield_buttons()
{
    return ~PINF & B11110000;
}

void task_learnDelete()
{
    static unsigned long t_0 = 0;
    static unsigned long b_t_0 = 0;
    static Delta <bool, int8_t> b_delta;
    static int8_t count = 0;

    unsigned long t_now = millis();
    uint8_t b_now = get_bitfield_buttons();
    
    if ( b_now == 0 ) b_t_0 = t_now; // resets button timer when button is up.

    // button hold timer:
    unsigned long b_t_delta = t_now - b_t_0; // time counted from reset.
    if ( b_delta(b_t_delta >= 50) > 0 ) { // on moment that time exceeds 50 ms.
        t_0 = t_now; // reset timer that resets count;
        ++count; // increment count

//        DEBUG_OUT_FAST("learn deleted", b_t_delta);
    }

    // button counter.
    unsigned long t_delta = t_now - t_0; // time since last button push.
    if ( t_delta >= 300 ) count = 0; // reset count 500 ms after last button push.
    else if ( count == 2 ) { // if button is triggered 2 times under 500 ms.
        count = 0;
        
        for ( int8_t row = 0; row < 4; ++row ) {
            if ( (b_now & rowTo_32u4PINF_bit[row]) > 0 ) {
                learn_start();
                learn.program(row); // only first arg, so empties this learn.
                learn_finish();

                DEBUG_OUT_FAST("learn deleted row: x", 1, learn.getPercNote(row));
                break;
            }
        }
    }
}

void task_learn()
{
    static unsigned long b_t_0 = 0;
    static Delta <bool, int8_t> b_delta;

    unsigned long t_now = millis();
    uint8_t b_now = get_bitfield_buttons();
    
    if ( b_now == 0 ) b_t_0 = t_now; // resets button timer when any button is up.

    // button hold timer:
    unsigned long b_t_delta = t_now - b_t_0; // time counted from reset.
    int8_t b_deltaOut = b_delta(b_t_delta >= 200);
    if ( b_deltaOut > 0 ) { // on moment that time exceeds 200 ms.
        learn_start();
//        do { MIDI.read();
//        } while ( get_bitfield_buttons() );
//        learn_finish();
    } else if ( b_deltaOut < 0 ) { // all buttons released.
        learn_finish();
    }
}

void learn_start()
{
    globalAddrCnt = 0; // Reset global address counter.
    address.fill(); // Reset all global addresses.
    clearRowData(); // Reset global rowNote data.
    clearKeysActive();
    clearLastNotes();

    // reset learnCc.
    learnCc.wait_for_98 = false;
    learnCc.wait_for_6 = false;
    learnCc.wait_for_lsb = false;
    learnCc.nrpn = 16383;
    
    MIDI.setHandleNoteOn(learn_note);
    MIDI.disconnectCallbackFromType(midi::NoteOff);
    MIDI.setHandleControlChange(learn_control_change);
    MIDI.setHandlePitchBend(learn_pitchbend);
    MIDI.setHandleAfterTouchChannel(learn_atc);
    MIDI.setHandleAfterTouchPoly(learn_atp);
}

void learn_finish()
{
    writeGlobalAddresses();
    writeKeyAftertouchMapping();
    writePolyAddresses();
    checkPolyphony();
    save_learn_status();

    MIDI.setHandleNoteOn(note_on);
    MIDI.setHandleNoteOff(note_off);
    MIDI.setHandleControlChange(control_change);
    MIDI.setHandlePitchBend(pitchbend);
    MIDI.setHandleAfterTouchChannel(atc);
    MIDI.setHandleAfterTouchPoly(atp);
}

void writeGlobalAddresses()
{
    // Write addresses for learned types.
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        int8_t row = learn.getRow(addr);
        int8_t type = (int8_t)learn.getType(addr);
        int8_t channel = learn.getChannel(addr);
        
        if ( row > -1 && type > -1 && channel > -1 ) {
            address.set(type, channel, addr);
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
    const int8_t boundariesHigh[3][4] = { // [width][row]
        {63, 127, 127, 127},
        {42, 85, 127, 127},
        {31, 63, 95, 127}
    };   
    int8_t outputCount = 0;
    for ( int8_t a = 0; a < addressAmount; ++a ) {
        int8_t polyAddr = learnToPolyMap[address.get((int8_t)learn.getType(a), learn.getChannel(a))];
        if ( polyAddr < 0 ) continue;
        
        int8_t width = polyphony.width[polyAddr];
        if ( width > 1 ) {
            polyphony.boundary[learn.getRow(a)] = boundariesHigh[width - 2][outputCount];
            outputCount += 1;
        } else polyphony.boundary[learn.getRow(a)] = 127;
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
            case ChannelType::Keys:
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
                learn.setPerc(row, note, true);
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

    switch ( channels.getChannelType(channel) ) {
        case ChannelType::Keys:
        for ( int8_t t = (int8_t)MessageType::Keys; t <= (int8_t)MessageType::KeysPolyPressure; ++t )
        {
            MessageType type = (MessageType)t;
            if ( address.getState((int8_t)type, channel) )
            {
                int8_t globalAddr = address.get((int8_t)type, channel);
                int8_t polyAddr = learnToPolyMap[globalAddr];
                
                // Schrijf row.
                int8_t row;

                if ( polyAddr > -1 ) row = getRowFromSplit(note, polyAddr);
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
                        cvOut.set(row, keysActive[row].getKey(positionsOfKey) << 1);
                        gate_out(row, true);
                    }
                    break;
                
                    case MessageType::KeysVelocity:
                    cvOut.set(row, velocitySave << 1);
                    break;
                
                    case MessageType::KeysPolyPressure:
                    learn.setRow(globalAddr, row);
                    rowNote[row] = note; // Nog nodig, want je hebt al key keysActive?

                    keysActive[row].noteOn(note);
                    break;
                        
                    default: break;
                }
            }
        }
        break;
            
        case ChannelType::Percussion: // handle choke and current note.
        for ( int8_t row = 0; row < 4; ++row ) {
            // Test current note against learn.
            if ( note == learn.getPercNote(row) ) {
                // handle choke first.
                int8_t *chokeNotes = percGetChokeNotes(note);
                for ( int8_t cRow = 0; cRow < 4; ++cRow ) {
                    if ( learn.getPercVel(cRow) && (learn.getPercNote(cRow) == chokeNotes[0] || learn.getPercNote(cRow) == chokeNotes[1]) )
                        cvOut.set(cRow, 0); // vel out
//                    DEBUG_OUT_FAST("percVel", learn.getPercVel(cRow));
                }

                // handle current note
                if ( learn.getPercVel(row) ) // check for vel
                    cvOut.set(row, velocity << 1); //vel out
                trigCounter[row] = PULSE_LENGTH_MS; // starts the counting.
                gate_out(row, true); // gate out
                break;
            }
        }
        break;

        default:
        break;
    }
}

// base on GM percussion map in General Midi system level 1.
int8_t * percGetChokeNotes(uint8_t note)
{
    // {first note, second note}
    static int8_t out[2] = {-1, -1};
    
    switch ( note ) {
        case 42: out[0] = 44; out[1] = 46; break;
        case 44: out[0] = 42; out[1] = 46; break;
        case 46: out[0] = 42; out[1] = 44; break;
        case 62: out[0] = 63; out[1] = -1; break;
        case 63: out[0] = 62; out[1] = -1; break;
        case 78: out[0] = 79; out[1] = -1; break;
        case 79: out[0] = 78; out[1] = -1; break;
        case 80: out[0] = 81; out[1] = -1; break;
        case 81: out[0] = 80; out[1] = -1; break;
        default: out[0] = -1; out[1] = -1; break;
    }

    return out;
}

void note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    --channel;

    switch ( channels.getChannelType(channel) ) {
        case ChannelType::Keys:
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
                    static int8_t keyRow = 0;
                    // Output.
                    note_off_lastStage(globalAddr, type, note, velocity, getRowFromSplit(note, polyAddr), &keyRow);
                }
                else
                {
                    static int8_t keyRow = 0;
                    // Output.
                    note_off_lastStage(globalAddr, type, note, velocity, learn.getRow(globalAddr), &keyRow);
                }
            }
        }
        break;

        case ChannelType::Percussion:
//        for ( int8_t row = 0; row < 4; ++row ) {
//            // handle gate.
//            if ( note == learn.getPercNote(row) ) {
//                gate_out(row, false); // gate out
//                break;
//            }
//        }
        break;

        default:
        break;
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
                    if ( keyOut > -1 ) cvOut.set(row, keyOut << 1);
                    gate_out(row, lastNote[row].getState());
                }
            }
        }
        break;

        case MessageType::KeysVelocity:
            cvOut.set(row, lastNote[*keyRowPntr].getVelocity() << 1);
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
                if ( atpAddr > -1 ) cvOut.set(row, pressures[atpAddr].get(keysActive[*keyRowPntr].getPosition(lastKey)) << 1);
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
        cvOut.set(learn.getRow(globalAddr), aftertouch << 1);
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
                
                if (  note == rowNote[row] ) cvOut.set(row, aftertouch << 1);
            }
        }
        else
        {
            int8_t row = learn.getRow(globalAddr);

            int8_t atpAddr = learnToPressureMap[row];
            int8_t keyPos = keysActive[row].getPosition(note);
            if ( atpAddr > -1 and keyPos > -1 ) pressures[atpAddr].set(keyPos, aftertouch);
            if (  note == rowNote[row] ) cvOut.set(row, aftertouch << 1);
        }
    }
}

void learn_control_change(uint8_t channel, uint8_t number, uint8_t val)
{
    --channel;

    if ( number < 120 ) {
        switch ( number ) {
        case 99:
            if ( val == 127 );
            else {
                learnCc.wait_for_98 = true;
                learnCc.nrpn = val << 7;
            }
            break;
        default:
            for ( int8_t row = 0; row < 4; ++row ) {
                if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 ) {
                    switch ( number ) {
                        case 98:
                            if ( val == 127 );
                            else if ( learnCc.wait_for_98 ) {
                                learnCc.wait_for_98 = false;
                                learnCc.wait_for_6 = true;
                                learnCc.nrpn |= val;
                                cc[row].nrpn = learnCc.nrpn;
                                cc[row].msb = number;
                                cc[row].precision = false;
                                learn.program(row, MessageType::ControlChange, channel, row);
                            }
                            return;
                        case 38:
                            goto LSB;
                        case 6:
                            if ( learnCc.wait_for_6 ) {
                                learnCc.wait_for_6 = false;
                                goto MSB;
                            }
                            return;
                        default:
                            if ( channel == 15 ) { // for rotary encoder (at least with Allen&Heath) relative style.
                                cc[row].msb = number;
                                learn.program(row, MessageType::ControlChange, channel, row);
                            } else if ( number > 31 && number < 64 ) {
                                LSB:
                                if ( learnCc.wait_for_lsb && cc[row].msb == (number - 32) ) {
                                    learnCc.wait_for_lsb = false;
                                    cc[row].precision = true;
                                }
                            } else {
                                MSB:
                                learnCc.wait_for_lsb = number < 32;
                                cc[row].msb = number;
                                cc[row].precision = false;
                                learn.program(row, MessageType::ControlChange, channel, row);
    //                            sprintf(debug, "learn_cc r%d, n%d, v%d, nrpn%d, prec%d, msb%d", row, number, val, cc[row].nrpn, cc[row].precision, cc[row].msb);
    //                            MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);
                            }
                            return;
                    }
                }
            }
            break;
        }
    }
}

void control_change(uint8_t channel, uint8_t number, uint8_t val)
{
    --channel;
    
    static uint8_t msb[4] = {0, 0, 0, 0};
    static uint16_t nrpn[16] = {
        16383, 16383, 16383, 16383,
        16383, 16383, 16383, 16383,
        16383, 16383, 16383, 16383,
        16383, 16383, 16383, 16383
    };

    static bool wait_for_98 = false;
    static uint16_t pre_nrpn = 16383;

    switch ( number ) {
    case 120: // at channel all notes off gates and velocities low.
        setAtChannelAllGatesLow(channel);
        setAtChannelAllVelocitiesLow(channel);
        setAtChannelAllNotesOff(channel);
        break;
    case 123:
    case 124:
    case 125:
    case 126:
    case 127: // at channel all notes off and gates low.
        setAtChannelAllGatesLow(channel);
        setAtChannelAllNotesOff(channel);
        break;
    case 98:
        if ( wait_for_98 ) {
            wait_for_98 = false;
            pre_nrpn |= val;
            nrpn[channel] = pre_nrpn;
        }
        break;
    case 99:
        wait_for_98 = true;
        pre_nrpn = val << 7;
        break;
    default:
        for ( int8_t row = 0; row < 4; ++row ) {
            if ( channel == learn.getChannel(row) && MessageType::ControlChange == learn.getType(row) ) {
                switch ( number ) {
                case 38:
                    if ( cc[row].nrpn == nrpn[channel] ) goto LSB;
                    return;
                case 6:
                    if ( cc[row].nrpn == nrpn[channel] ) goto MSB;
                    break;
                case 96: // nrpn increment.
                    if ( cc[row].nrpn == nrpn[channel] ) {
                        uint8_t out = cvOut.get(row);
                        if ( out < 255 ) cvOut.set(row, out + 1);
                    }
                    return;
                case 97: // nrpn decrement.
                    if ( cc[row].nrpn == nrpn[channel] ) {
                        uint8_t out = cvOut.get(row);
                        if ( out > 0 ) cvOut.set(row, out - 1);
                    }
                    break;
                default:
                    if ( channel == 15 ) { // for rotary encoder (at least with Allen&Heath) relative style.
                        if ( cc[row].msb == number ) {
                            int16_t out = cvOut.get(row);
                            out += val < 64 ? val : val - 128; //inc or dec.
                            out = constrain(out, 0, 255);
                            cvOut.set(row, out);
                        }
                    } else if ( number > 31 && number < 64 ) {
                        LSB:
                        if ( cc[row].precision && cc[row].msb == (number - 32)) {
                            cvOut.set(row, ((msb[row] << 7) | val) >> 6);
                            return;
                        }
                    } else {
                        MSB:
                        if ( cc[row].msb == number ) {
                            if ( cc[row].precision ) { // kan alleen true zijn als het cc[row].msb onder de 32 is.
                                msb[row] = val;
                            } else {
                                cvOut.set(row, val << 1);
                            }
                            return;
                        }
                    }
                    break;
                }
            }
        }
        break;
    }
//    for ( int8_t row = 0; row < 4; ++row ) {
//        sprintf(debug, "play_cc r%d, n%d, v%d, nrpnC%d, nrpn%d, prec%d, msb%d",
//            row, number, val, nrpn[learn.getChannel(row)], cc[row].nrpn, cc[row].precision, cc[row].msb);
//        MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);
//    }
}

void pitchbend(uint8_t channel, int pitch)
{
    --channel;
    
	MessageType type = MessageType::PitchBend;

    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        cvOut.set(learn.getRow(globalAddr), (pitch + 8192) >> 6);
    }
}

void gate_out(int8_t row, bool state)
{
	const int8_t GATES[4] = {GATE_1_PIN, GATE_2_PIN, GATE_3_PIN, GATE_4_PIN};
	digitalWrite(GATES[row], state);

//    DEBUG_OUT_FAST("gate row: xxx", 3, row);
//    DEBUG_OUT_FAST("gate val: xxx", 3, state);
    DEBUG_OUT_SUPERFAST(Debug::Gate, row, state);
}

void clock_tick()
{
    digitalWrite(CLOCK_PIN, HIGH);
    clockCounter = PULSE_LENGTH_MS;
//    activeSensing_update();
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
    setAllClockOutputsLow();
    setAtChannelAllGatesLow(-1);
}

void activeSensing_onTimeout()
{
    setAllClockOutputsLow();
    setAtChannelAllGatesLow(-1);
    setAtChannelAllNotesOff(-1);

    DEBUG_OUT("midi timeout");
}

void setAtChannelAllNotesOff(int8_t c) // when c == -1 all notes off.
{
    for ( int8_t i = 0; i < 4; ++i ) {
        if ( c == -1 || c == learn.getChannel(i) ) {
            keysActive[i].reset();
            lastNote[i].reset();
        }
    }
}

void setAtChannelAllGatesLow(int8_t c) // when c == -1 all gates low.
{
    for ( int8_t i = 0; i < 4; ++i ) {
        if ( c == -1 || c == learn.getChannel(i) ) {
            gate_out(i, 0);
        }
    }
}

void setAllClockOutputsLow()
{
    digitalWrite(CLOCK_PIN, LOW);
    digitalWrite(RESET_PIN, LOW);
}

void setAtChannelAllVelocitiesLow(int8_t c) // when c == -1 all velocities low.
{
    for ( int8_t i = 0; i < 4; ++i ) {
        if ( (c == -1 || c == learn.getChannel(i)) && (learn.getType(i) == MessageType::KeysVelocity || learn.getPercVel(i)) ) {
            cvOut.set(i, 0);
        }
    }
}
