#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

//#define DEBUG
#undef DEBUG
#include "debug.h"
#include "ActiveSensing.h"

#include "Utility.h"
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

enum class Gate: int8_t {
    Percussion,
    Size
};

enum class Cv: int8_t {
    KeysPitch,
    KeysVelocity,
    KeysPolyPressure,
    PercussionVelocity,
    ChannelPressure,
    ControlChange,
    PitchBend,
    Size
};

const int8_t rowAmount = 4;
const int8_t channelAmount = 16;

struct CvOut {
    uint8_t values[rowAmount] = {0, 0, 0, 0};

    uint8_t get(int8_t row) {
        return values[row];
    }
    
    void set(int8_t row, uint8_t value) {
        values[row] = value;
        
        switch (row) {
            case 0: PWM6 = value; break;
            case 1: PWM9 = value; break;
            case 2: PWM10 = value; break;
            case 3: PWM13 = value; break;
            default: break;
        }
        
        DEBUG_CV_OUT
    }
} cvOut;

const uint8_t rowTo_32u4PINF_bit[rowAmount] = {0x80, 0x40, 0x20, 0x10};

Channels channels;

const int8_t addressAmount = rowAmount;
const int8_t percAddressAmount = rowAmount;
const int8_t cvAddressAmount = rowAmount;

typedef TwoDimensionalLookup <(1, channelAmount> percAddress_t;
percAddress_t percAddress; //Saved in eeprom

typedef TwoDimensionalLookup <(int8_t)Cv::Size, channelAmount> cvAddress_t;
cvAddress_t cvAddress; //Saved in eeprom

typedef Programmer <Gate, percAddressAmount> learnGate_t;
learnGate_t learnGate; //Saved in eeprom

typedef Programmer <Cv, cvAddressAmount> learnCv_t;
learnCv_t learnCv; //Saved in eeprom

const uint8_t keysActiveSize = rowAmount;
//typedef ActiveKeysSmall <10> keysActive_t;
typedef ActiveKeysFast2 <10> keysActive_t;
keysActive_t keysActive[keysActiveSize];

const uint8_t lastNoteSize = rowAmount;
//int8_t addressToLastNoteMap[4] = {-1, -1, -1, -1}; //Saved in eeprom
//MidiLastNote lastNote[4];
//LastNoteSmall <10> lastNote[4];
typedef LastNoteFast <10> lastNote_t;
lastNote_t lastNote[lastNoteSize];

// int8_t rowNote[rowAmount] = {-1, -1, -1, -1};

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


//int8_t learnToPolyMap[addressAmount] = {-1, -1, -1, -1}; //Saved in eeprom
typedef Polyphony <addressAmount, rowAmount> split_t;
split_t gateSplit;
split_t cvSplit;

// Voor PolyPressure welke fingers actief zijn en hoeveel.
int8_t addressToPressureMap[4] = {-1, -1, -1, -1}; //Saved in eeprom
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
    checkSplit();
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
    MIDI.setHandleActiveSensing(active_sensing);
}

void load_learn_status()
{
    uint16_t a = 0; // eeprom address.
    EEPROM.get(a, learnGate); a += sizeof(learnGate);
    EEPROM.get(a, learnCv); a += sizeof(learnCv);
    EEPROM.get(a, percAddress); a += sizeof(percAddress);
    EEPROM.get(a, cvAddress); a += sizeof(cvAddress);
    EEPROM.get(a, gateSplit); a += sizeof(gateSplit);
    EEPROM.get(a, cvSplit); a += sizeof(cvSplit);
    
    for ( int8_t i = 0; i < cvAddressAmount; ++i ) {
        EEPROM.get(a, addressToPressureMap[i]); a += sizeof(addressToPressureMap[i]);
        EEPROM.get(a, cc[i]); a += sizeof(cc[i]);
    }

    DEBUG_LOAD_EEPROM
}

void save_learn_status()
{
    uint16_t a = 0; // eeprom address.
    EEPROM.put(a, learnGate); a += sizeof(learnGate);
    EEPROM.put(a, learnCv); a += sizeof(learnCv);
    EEPROM.put(a, percAddress); a += sizeof(percAddress);
    EEPROM.put(a, cvAddress); a += sizeof(cvAddress);
    EEPROM.put(a, gateSplit); a += sizeof(gateSplit);
    EEPROM.put(a, cvSplit); a += sizeof(cvSplit);
    
    for ( int8_t i = 0; i < cvAddressAmount; ++i ) {
        EEPROM.put(a, addressToPressureMap[i]); a += sizeof(addressToPressureMap[i]);
        EEPROM.put(a, cc[i]); a += sizeof(cc[i]);
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
    }
}

int8_t globalAddrCnt = 0;

// void clearRowData()
// {
//     for ( int8_t row = 0; row < rowAmount; ++row )
//     {
//         rowNote[row] = -1;
//     }
// }

void clearKeysActive()
{
    for ( int8_t addr = 0; addr < keysActiveSize; ++addr )
    {
        keysActive[addr].reset();
    }
}

void clearLastNotes()
{
    for ( int8_t addr = 0; addr < keysActiveSize; ++addr )
    {
        lastNote[addr].reset();
    }
}

void setupLearn()
{
    for ( int8_t row = 0; row < rowAmount; ++row )
    {
        bool buttonStates[rowAmount];
        static bool lastButtonStates[rowAmount] = {0, 0, 0, 0};

        buttonStates[row] = ~PINF & rowTo_32u4PINF_bit[row];
        int8_t deltaButtonState = buttonStates[row] - lastButtonStates[row];
        lastButtonStates[row] = buttonStates[row];
        
        if ( deltaButtonState > 0 )
        {
            delay(200); // against hysterisis

            globalAddrCnt = 0; // Reset global address counter.
            percAddress.fill(); // Reset all global addresses.
            cvAddress.fill(); // Reset all global addresses.
            // clearRowData(); // Reset global rowNote data.
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
            MIDI.disconnectCallbackFromType(midi::Clock);
            MIDI.disconnectCallbackFromType(midi::Start);
            MIDI.disconnectCallbackFromType(midi::Continue);
            MIDI.disconnectCallbackFromType(midi::Stop);
        }
        else if ( deltaButtonState < 0 )
        {
            writeGlobalAddresses();
            writeKeyAftertouchMapping();
            checkSplit();
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
    for ( int8_t addr = 0; addr < percAddressAmount; ++addr )
    {
        if ( learnGate.rows[addr] > -1 )
        {
            percAddress.set((int8_t)learnGate.types[addr], learnGate.channels[addr], addr);
        }
    }

    for ( int8_t addr = 0; addr < cvAddressAmount; ++addr )
    {
        if ( learnCv.rows[addr] > -1 )
        {
            cvAddress.set((int8_t)learnCv.types[addr], learnCv.channels[addr], addr);
        }
    }
}

void writeKeyAftertouchMapping()
{
    // Reset
    for ( int8_t addr = 0; addr < cvAddressAmount; ++addr )
    {
        addressToPressureMap[addr] = -1;
    }

    int8_t count = 0;
    
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) == ChannelType::Keys )
        {
            for ( int8_t addr = 0; addr < cvAddressAmount; ++addr )
            {
                if ( learnCv.types[addr] == Cv::KeysPolyPressure and learnCv.channels[addr] == channel )
                {
                    addressToPressureMap[addr] = count;
                    ++count;
                }
            }
        }
    }
}

void checkSplit()
{
    for ( int8_t a = 0; a < percAddressAmount; ++a ) {
        gateSplit.width[a] = 0; // Reset width.
        // Reset count and row info.
        for ( int8_t count = 0; count < rowAmount; ++count ) {
            gateSplit.rowAtCount[a][count] = -1;
//            int8_t row = count;
//            gateSplit.countAtRow[row] = 0;
        }
    }

    for ( int8_t a = 0; a < cvAddressAmount; ++a ) {
        cvSplit.width[a] = 0; // Reset width.
        // Reset count and row info.
        for ( int8_t count = 0; count < rowAmount; ++count ) {
            cvSplit.rowAtCount[a][count] = -1;
//            int8_t row = count;
//            cvSplit.countAtRow[row] = 0;
        }
    }

    // Write.
    for ( int8_t channel = 0; channel < 16; ++channel ) {
        if ( channels.getChannelType(channel) != ChannelType::Keys ) continue;
        
        Gate gateType = Gate::Keys;
        int8_t keyAddr = address.get((int8_t)gateType, channel);
        if ( keyAddr < 0 ) continue;
        
        for ( int8_t addr = 0; addr < percAddressAmount; ++addr ) {
            if ( learnGate.types[addr] == gateType and learnGate.channels[addr] == channel ) {
                gateSplit.rowAtCount[keyAddr][gateSplit.width[keyAddr]] = learnGate.rows[addr];
                ++gateSplit.width[keyAddr];
            }
        }

        if ( gateSplit.width[keyAddr] < 1 ) continue;

        fillSplitBoundary <learnGate_t, Gate> (&gateSplit, learnGate, gateType, channel);
        
        
        for ( int8_t type = (int8_t)Cv::KeysPitch; type <= (int8_t)Cv::KeysPolyPressure; ++type ) {
            int8_t cvAddr = address.get(type, channel);
            if ( cvAddr < 0 ) continue;
            
            for ( int8_t addr = 0; addr < cvAddressAmount; ++addr ) {
                if ( learnCv.types[addr] == (Cv)type and learnCv.channels[addr] == channel ) {
                    cvSplit.rowAtCount[cvAddr][cvSplit.width[keyAddr]] = learnCv.rows[addr];
                    ++cvSplit.width[cvAddr];
                }
            }

            if ( cvSplit.width[keyAddr] < 1 ) continue;
            fillSplitBoundary <learnCv_t, Cv> (&cvSplit, learnCv, type, channel);
            
            if ( cvSplit.width[cvAddr] != gateSplit.width[keyAddr] ) {
                cvSplit.width[cvAddr] = 1;
            }
        }
    }
}

template <typename learn_t, typename type_t>
void fillSplitBoundary(split_t *split, learn_t learn, type_t type, int8_t channel)
{
    const int8_t boundariesHigh[4][4] = { // [width][outputCount]
        {127, 127, 127, 127},
        {63, 127, 127, 127},
        {42, 85, 127, 127},
        {31, 63, 95, 127}
    };
        
    int8_t outputCount = 0;
    
    for ( int8_t r = 0; r < rowAmount; ++r ) {
        if ( learn.types[r] != type ) continue;

        int8_t a = address.get((int8_t)type, channel);
        if ( a < 0 ) continue;
        int8_t width = split->width[a];
        if ( width < 1 ) continue;

        split->boundary[learn.rows[r]] = boundariesHigh[width - 1][outputCount];
        outputCount += 1;
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
                        learn.programGate(row, MessageType::Keys, channel, row);
                        learn.programCv(row, MessageType::KeysPitch, channel, row);
                    break;
                    
                    case 1:
                        learn.programCv(row, MessageType::KeysVelocity, channel, row);
                    break;
                    
                    case 2:
                    // Bij ontvangst van polyaftertouch wordt channelaftertouch hier naar toe veranderd.
                    // Maar kan niet terug veranderd worden naar channelpressure. Dus polypressure heeft prio.
                        learn.programCv(row, MessageType::ChannelPressure, channel, row, note);
                    break;

                    case 3:
                        learn.programCv(row, MessageType::PitchBend, channel, row);
                    break;
                    
                    default:
                    break;
                }
            break;
            
            case ChannelType::Percussion:
                learn.programGate(row, MessageType::Percussion, channel, row, note);
                learn.programCv(row, MessageType::PercussionVelocity, channel, row, note);
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
//            learn.channels[row] == channel
//            and
//            learn.types[row] == MessageType::ChannelPressure
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
            learn.channels[row] == channel
            and
            learn.types[row] == MessageType::ChannelPressure
            and
            (~PINF & rowTo_32u4PINF_bit[row]) > 0
            and learnCv.notes[row] == note
        ) {
            learn.programCv(row, MessageType::KeysPolyPressure, channel, row);
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
                learn.programCv(row, MessageType::PitchBend, channel, row);
            }
        }
    }
}

void note_on(uint8_t channel, uint8_t note, uint8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
    --channel;

    switch ( channels.getChannelType(channel) ) {
        case ChannelType::Keys:
        for ( int8_t t = (int8_t)Cv::KeysPitch; t <= (int8_t)Cv::KeysPolyPressure; ++t )
        {
            Cv type = (Cv)t;
            if ( address.getState((int8_t)type, channel) )
            {
                int8_t globalAddr = address.get((int8_t)type, channel);
                int8_t row = getRowFromSplit(note, polyAddr);

                // Output.
                static int8_t velocitySave = 0;                
                
                switch ( type )
                {
                    case Cv::KeysPitch:
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
                
                    case Cv::KeysVelocity:
                    cvOut.set(row, velocitySave << 1);
                    break;
                
                    case Cv::KeysPolyPressure:
                    learnCv.rows[globalAddr] = row;;
                    rowNote[row] = note; // Nog nodig, want je hebt al key keysActive?

                    keysActive[row].noteOn(note);
                    break;
                        
                    default: break;
                }
            }
        }
        break;
            
        case ChannelType::Percussion:
        Cv type = Cv::PercussionVelocity;
        if ( address.getState((int8_t)type, channel) ) {
            // handle velocity.
            for ( int8_t row = 0; row < 4; ++row ) {
                if ( note == learnCv.notes(row) ) {
                    cvOut.set(row, velocity << 1);
                    break;
                }
            }
        }

        Gate type = Gate::Percussion;
        if ( address.getState((int8_t)type, channel) ) {
            // handle choke and gate output.
            int8_t *chokeNotes = percGetChokeNotes(note);
            for ( int8_t row = 0; row < 4; ++row ) {
                // handle cv/gate choke: cv choke first: because of the cv slewing (ongeveer 0,5 ms).
                if ( chokeNotes[0] == learnCv.notes(row) ) {
                    if ( chokeNotes[0] == learnCv.notes(row) ) cvOut.set(row, 0);
                    gate_out(row, false);
                } else if ( chokeNotes[1] == learnCv.notes(row) ) {
                    if ( chokeNotes[1] == learnCv.notes(row) ) cvOut.set(row, 0);
                    gate_out(row, false);
                }

                // handle gate.
                if ( note == learnCv.notes(row) ) {
                    gate_out(row, true);
                    break;
                }
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
        for ( int8_t t = (int8_t)Cv::KeysPitch; t <= (int8_t)Cv::KeysPolyPressure; t = t + 1 ) {
            Cv type = (Cv)t;
            
            if ( address.getState((int8_t)type, channel) ) {
                int8_t globalAddr = address.get((int8_t)type, channel);
                static int8_t keyRow = 0;
                // Output.
                note_off_lastStage <Cv> (globalAddr, type, note, velocity, getRowFromSplit(note, polyAddr), &keyRow);
            }
        }
        break;

        case ChannelType::Percussion:
        Cv type = Cv::PercussionVelocity;
        if ( address.getState((int8_t)type, channel) ) {
            // handle velocity.
            for ( int8_t row = 0; row < 4; ++row ) {
                if ( note == learnCv.notes(row) ) {
                    cvOut.set(row, 0);
                    break;
                }
            }
        }

        Gate type = Gate::Percussion;
        if ( address.getState((int8_t)type, channel) ) {
            // handle gate.
            for ( int8_t row = 0; row < 4; ++row ) {
                // handle gate.
                if ( note == learnCv.notes(row) ) {
                    gate_out(row, false);
                    break;
                }
            }
        }
        break;

        default:
        break;
    }
}

template <typename type_t>
void note_off_lastStage(int8_t globalAddr, type_t type, int8_t note, int8_t velocity, int8_t row, int8_t* keyRowPntr)
{
    switch ( type )
    {
        case Cv::KeysPitch:
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

        case Cv::KeysVelocity:
            cvOut.set(row, lastNote[*keyRowPntr].getVelocity() << 1);
        break;

        case Cv::KeysPolyPressure:
        {
            keysActive[row].noteOff(note);
            
            if ( lastNote[*keyRowPntr].getState() )
            {
                learnCv.rows[globalAddr] = row;
                int8_t positionOfLastKey = lastNote[*keyRowPntr].getPitch();
                int8_t lastKey = keysActive[*keyRowPntr].getKey(positionOfLastKey);
                learnCv.notes[row] = lastKey;

                int8_t atpAddr = addressToPressureMap[row];
                if ( atpAddr > -1 ) cvOut.set(row, pressures[atpAddr].get(keysActive[*keyRowPntr].getPosition(lastKey)) << 1);
            }
            else rowNote[row] = -1; // Nog nodig, want je hebt al key keysActive?
        }
        default: break;
    }
}

int8_t getRowFromSplit(int8_t note, int8_t polyAddr)
{
    for ( int8_t c = 0; split.width[polyAddr]; ++c )
    {
        int8_t rowTemp = split.rowAtCount[polyAddr][c];
        
        if ( note <= split.boundary[rowTemp] )
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
        cvOut.set(learnCv.rows[globalAddr], aftertouch << 1);
    }
}

void atp(uint8_t channel, uint8_t note, uint8_t aftertouch)
{
    --channel;

    MessageType type = MessageType::KeysPolyPressure;
    
    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        int8_t polyAddr = addressToSplitMap[globalAddr];
        
        if ( polyAddr > -1 )
        {
            for ( int8_t c = 0; c < split.width[polyAddr]; ++c )
            {
                int8_t row = split.rowAtCount[polyAddr][c];

                int8_t atpAddr = addressToPressureMap[row];
                int8_t keyPos = keysActive[row].getPosition(note);
                if ( atpAddr > -1 and keyPos > -1 ) pressures[atpAddr].set(keyPos, aftertouch);
                
                if (  note == rowNote[row] ) cvOut.set(row, aftertouch << 1);
            }
        }
        else
        {
            int8_t row = learnCv.rows[globalAddr];

            int8_t atpAddr = addressToPressureMap[row];
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
                                learn.programCv(row, Cv::ControlChange, channel, row);
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
                                learn.programCv(row, Cv::ControlChange, channel, row);
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
                                learn.programCv(row, Cv::ControlChange, channel, row);
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
            if ( channel == learn.channels[row] && Cv::ControlChange == learn.types[row] ) {
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
//            row, number, val, nrpn[learn.channels[row]], cc[row].nrpn, cc[row].precision, cc[row].msb);
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
        cvOut.set(learnCv.rows[globalAddr], (pitch + 8192) >> 6);
    }
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
    setAllClockOutputsLow();
    setAtChannelAllGatesLow(-1);
}

void active_sensing()
{
    activeSensing_update();
}

void activeSensing_onTimeout()
{
    setAllClockOutputsLow();
    setAtChannelAllGatesLow(-1);
    setAtChannelAllNotesOff(-1);

    DEBUG_ACTIVE_TIMEOUT
}

void setAtChannelAllNotesOff(int8_t c) // when c == -1 all notes off.
{
    for ( int8_t i = 0; i < 4; ++i ) {
        if ( c == -1 || c == learn.channels[i] ) {
            keysActive[i].reset();
            lastNote[i].reset();
        }
    }
}

void setAtChannelAllGatesLow(int8_t c) // when c == -1 all gates low.
{
    for ( int8_t i = 0; i < 4; ++i ) {
        if ( c == -1 || c == learn.channels[i] ) {
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
        if ( (c == -1 || c == learn.channels[i]) && (learn.types[i] == Cv::KeysVelocity || learn.types[i] == Cv::PercussionVelocity) ) {
            cvOut.set(i, 0);
        }
    }
}
