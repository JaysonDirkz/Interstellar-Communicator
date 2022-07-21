#include <Arduino.h>
#include <EEPROM.h>
#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

bool debugState = 1;

#include "MidiUtility.h"

#include "global.c"
#include "cv_write.c"
#include "keys_writes.c"
#include "learn.cpp"

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

// 5 als minimum want midi clock pulse lengte bij 360 bpm is 60000 / 360*24 = 6,94 ms
const uint8_t PULSE_LENGTH_MS = 5;

void setup()
{
    // MIDI.
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff();


    // Output pins for gates.
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(RESET_PIN, OUTPUT);
    pinMode(GATE_1_PIN, OUTPUT);
    pinMode(GATE_2_PIN, OUTPUT);
    pinMode(GATE_3_PIN, OUTPUT);
    pinMode(GATE_4_PIN, OUTPUT);
    
    digitalWrite(CLOCK_PIN,LOW);
    digitalWrite(RESET_PIN,LOW);
    digitalWrite(GATE_1_PIN,LOW);
    digitalWrite(GATE_2_PIN,LOW);
    digitalWrite(GATE_3_PIN,LOW);
    digitalWrite(GATE_4_PIN,LOW);


    // Input pins for buttons.
    pinMode(BUTTON_1_PIN, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
    pinMode(BUTTON_3_PIN, INPUT_PULLUP);
    pinMode(BUTTON_4_PIN, INPUT_PULLUP);
    

    // Output pins for CV (pwm).
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
    

    // Read switch params from EEPROM.
    // cvBases.
    // keysBases.
    // keysGlobal.
    // all addresses.
}

inline uint8_t get_bitfield_buttons()
{
    return ~PINF & B11110000;
}

void setOutputs()
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
                case 0: gate_pins[row] = false;
                default: --trigCounter[row]; break;
            }
        }
    }

    for ( int8_t i = 0; i < 4; ++i ) {
        cv_out(i);
    }

    for ( int8_t i = 0; i < 4; ++i ) {
        gate_out(i);
    }
}

void cv_out(int8_t row)
{
	// uint8_t value = number >> rightShift;
	switch (row)
	{
		case 0: PWM6 = cv_pins[row]; break;
		case 1: PWM9 = cv_pins[row]; break;
		case 2: PWM10 = cv_pins[row]; break;
		case 3: PWM13 = cv_pins[row]; break;
        default: break;
	}

     // debug
	 if ( ! MIDI.getThruState() ) {
	 	MIDI.sendControlChange(0, cv_pins[row] >> 7, row + 1);
	 	MIDI.sendControlChange(32, cv_pins[row] & 127, row + 1);
	 }
}

void gate_out(int8_t row)
{
	const int8_t GATES[4] = {GATE_1_PIN, GATE_2_PIN, GATE_3_PIN, GATE_4_PIN};

	digitalWrite(GATES[row], gate_pins[row]);

    // debug
	 if ( ! MIDI.getThruState() ) {
	 	MIDI.sendNoteOn(0, gate_pins[row], row + 1);
	 }
}

void loop()
{
    // Check buttons.
    auto b_now = get_bitfield_buttons();
    if ( b_now ) {
        static unsigned long t_0 = 0;
        unsigned long t_now = millis();

        static uint8_t b_last = 0;
        if ( b_now != b_last ) t_0 = t_now;
        b_last = b_now;

        unsigned long t_delta = t_now - t_0;
        if ( t_delta > 300 ) {
            do {
                b_last = learn_parse_midi();
            } while ( b_last );
        }
    }


    // check midi for playing.
	if ( MIDI.read() ) {
        switch ( MIDI.getType() ) {
            case NoteOn:
            case NoteOff:
                parseKey(keysAddresses[MIDI.getChannel()]);
            break;

            case AfterTouchPoly:
                learnCv(atpAddresses[MIDI.getChannel()]);
            break;

            case AfterTouchChannel:
                learnCv(atcAddresses[MIDI.getChannel()]);
            break;

            case PitchBend:
                learnCv(pbAddresses[MIDI.getChannel()]);
            break;

            case ControlChange:
                learnCv(ccAddresses[MIDI.getChannel()]);
            break;

            case SystemExclusive:
                // Learn. Can change setting during playing. (not writing to EEPROM).
            break;

            case Clock:
                digitalWrite(CLOCK_PIN, HIGH);
                clockCounter = PULSE_LENGTH_MS;
            //    MIDI.sendRealTime(midi::Clock);
            break;

            case Start:
                digitalWrite(RESET_PIN, HIGH);	
                resetCounter = PULSE_LENGTH_MS;
            break;

            case Continue:
                //nothing:
            break;

            case Stop:
                digitalWrite(CLOCK_PIN, LOW);
                digitalWrite(RESET_PIN, LOW);
                gate_pins[0] = false;
                gate_pins[1] = false;
                gate_pins[2] = false;
                gate_pins[3] = false;
            break;

            case ActiveSensing: // if timeout then goto Stop;
            default: break;
        }
    }

    setOutputs();
}