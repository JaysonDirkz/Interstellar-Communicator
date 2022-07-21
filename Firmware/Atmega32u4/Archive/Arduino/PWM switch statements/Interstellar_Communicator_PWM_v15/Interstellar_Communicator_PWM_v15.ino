/*
03-2-2020
Jason

Nu gedaan:
- functie pointer arrays vervangen met switch case statements.
- PWM out functie versimpeld.
- sommige modulo's % met bit AND & vervangen en sommige met min(). Ook vergelijkingswaarde aangepast.

TODO
- Aftertouch nog testen/maken.
- Erase program toevoegen. (2x snel drukken knop).

Testen
- Test clock.
- Test learn save voor alle learns (ook polyfoon keys en drums).
*/

#include <MIDI.h>
#include <EEPROM.h>

#define CLOCK 2
#define RESET 3
#define GT1  4
#define GT2  5
#define GT3  7
#define GT4  8
#define BUTTON_1 A0
#define BUTTON_2 A1
#define BUTTON_3 A2
#define BUTTON_4 A3

//Constants
const bool MIDI_DEBUG = false;

const int8_t LOGIC_OUTPUTS[6] = {CLOCK, RESET, GT1, GT2, GT3, GT4};

const int8_t HALFGAIN = 1;
const int8_t FULLGAIN = 2;
int8_t DAC_GAIN[10];
uint8_t DAC_OFFSET[10];

const int8_t NOTHING = -1;

const int8_t MONOVOICE = 0;
const int8_t DUOVOICE = 1;
const int8_t TRIVOICE = 2;
const int8_t QUADVOICE = 3;

const int8_t KEYS = 0;
const int8_t VELOCITY = 1;
const int8_t AFTERTOUCH = 2;
const int8_t CCNORMAL = 3;
const int8_t CC14BIT = 4;
const int8_t PITCHBEND = 5;
const int8_t PERCVELOCITY = 6;
const int8_t PERCAFTERTOUCH = 7;
const int8_t PERCTRIGGER = 8;
const int8_t PERCGATE = 9;
const int8_t CCNRPN = 10;
const int8_t CCNRPN14BIT = 11;

enum CC_TYPE { NORMAL, NORMAL14BIT, NRPN, NRPN14BIT };

//Setup stuff

//Learn modus stuff
int8_t ButtonStates;
bool LearnMode = false;
int8_t learn_NoteNumbers_count;

int8_t CvChannels[4];
int8_t CvStates[4] = {-1, -1, -1, -1};
int8_t CvAfterNote[4];

int8_t GateChannels[4];
int8_t GateStates[4];
int8_t GateNotes[4];
int8_t DrumGateNoteAfter[4] = {-1, -1, -1, -1};

int8_t MayProgramAftertouch;

//Poly stuff
bool MayPlayOne[3];
//bool MayPlayCvatRow[4];
int8_t VoiceTypeKeys[17];
int8_t SizeofVoiceTypeKeys = 17;
int8_t LastRowKeys[17];
int8_t CurrentNotePlayed[4] = {-1, -1, -1, -1};
int8_t CurrentVelocityPlayed[4] = {-1, -1, -1, -1};
int8_t CurrentAftertouchPlayed[4] = {-1, -1, -1, -1};
int8_t SizeofRow_CurrentNotePlayed = 4;
int16_t PolyNotes[17][10];
int8_t SizeofRow_PolyNotes = 10;

//Mono Stuff
int16_t MsbNoteLsbVel;
int16_t NoteNumbers[4][10] = {
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};
int8_t SizeofRow_NoteNumbers = 10;

int8_t AmountOfPlayedPolyNotes[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//Drumstuff
bool PlayDrumTrigVel[4] = {false, false, false, false};
bool PlayDrumGateAT[4] = {false, false, false, false};

//Aftertouch stuff
int8_t AftertouchNoteNumber[4] = {-1, -1, -1, -1};

//Other
int8_t MidiValue[3];

//Clock stuff
int8_t clk_stop = 0;
int8_t clk_start = 1;
int8_t clk_continue = 2;
int8_t clk_state = clk_stop;

int8_t clock_count; //Telt de ontvangen midi clock messages.

//Timers
int16_t MidiTimer;
uint8_t timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
uint8_t Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

MIDI_CREATE_DEFAULT_INSTANCE();

// Frequency modes for TIMER1
#define PWM62k   1   //62500 Hz

// Direct PWM change variables
#define PWM9   OCR1A
#define PWM10  OCR1B

// Frequency modes for TIMER4
#define PWM187k 1   // 187500 Hz

// Direct PWM change variables
#define PWM6        OCR4D
#define PWM13       OCR4A

// Terminal count
#define PWM6_13_MAX OCR4C

void setup()
{
	setinputsandoutputs();
	configuremidisethandle();
	load_learn_status();
}

void setinputsandoutputs()
{
	MIDI.begin(MIDI_CHANNEL_OMNI);
//  MIDI.turnThruOff();
	
	pinMode(CLOCK, OUTPUT);
	pinMode(RESET, OUTPUT);
	pinMode(GT1, OUTPUT);
	pinMode(GT2, OUTPUT);
	pinMode(GT3, OUTPUT);
	pinMode(GT4, OUTPUT);

	pinMode(BUTTON_1, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
	pinMode(BUTTON_2, INPUT_PULLUP);
	pinMode(BUTTON_3, INPUT_PULLUP);
	pinMode(BUTTON_4, INPUT_PULLUP);

	digitalWrite(CLOCK,LOW);
	digitalWrite(RESET,LOW);
	digitalWrite(GT1,LOW);
	digitalWrite(GT2,LOW);
	digitalWrite(GT3,LOW);
	digitalWrite(GT4,LOW);
	
	confpwm();
	//Sets the DAC outputs on lowest output, which translates to 0V. Which is NoteNumber C-1 and midi NoteNumber 0.
	pwm_out(0, 128);
	pwm_out(1, 128);
	pwm_out(2, 128);
	pwm_out(3, 128);
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
	MIDI.setHandleNoteOn(note_on_vel);
	MIDI.setHandleNoteOff(note_off);
//  MIDI.setHandleControlChange(RecievedCC);
//  MIDI.setHandlePitchBend(RecievedPitchBend);
//  MIDI.setHandleAfterTouchChannel(RecievedChannelPressure);
//  MIDI.setHandleAfterTouchPoly(RecievedNotePressure);
//  MIDI.setHandleClock(RecievedClock);
//  MIDI.setHandleStart(RecievedStart);
//  MIDI.setHandleContinue(RecievedContinue);
//  MIDI.setHandleStop(RecievedStop);
//  MIDI.setHandleActiveSensing(RecievedActive);
}

void loop()
{
	MIDI.read();
}

void note_on_vel(int8_t channel, int8_t note, int8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
	note_on(channel, note);
	velocity(channel, velocity);
}

bool mono_note_on_hist[4][128];
bool poly_note_on_hist[16][128];

void note_on(int8_t channel, int8_t note)
{
	int type = NOTE_ON;
	int scale = cv_scale[type];
	for ( int row = 0, row < 4; row++ ) {
		int8_t main_conditions = cv_main_conditions(row, type, channel, note);
		int8_t note_conditions = cv_note_on_conditions(row, channel, note);

		const int8_t MONO_KEYS = B001;
		const int8_t POLY_KEYS = B011;
		const int8_t PERCUSSION = B101;

		switch ( main_conditions | note_conditons ) {
			case MONO_KEYS:
				mono_note_on_hist[row][note] = HIGH;
				cv_out(row, note, scale);
				gate_out(row, HIGH);
				break;
			case POLY_KEYS:
				if ( note_linked[row] ) {
					poly_note_on_hist[channel][note] = HIGH;
				} else {
					cv_out(row, note, scale);
					gate_out(row, HIGH);
				}
				break;
			case PERCUSSION:
				gate_out(row, HIGH);
				break;
		}

		//mono keys
		if ( main_conditions && !polyphony[channel][note] && !percussion[row]) ) {
			mono_note_on_hist[row][note] = HIGH;
			cv_out(row, note, scale);
			gate_out(row, HIGH);
			break;
		}
		//poly keys
		if ( main_conditions && polyphony[channel][note] && !note_linked[row] ) {
			cv_out(row, note, scale);
			gate_out(row, HIGH);
			break;
		}
		if ( main_conditions && polyphony[channel][note] && note_linked[row] ) {
			poly_note_on_hist[channel][note] = HIGH;
		}
		//percussion;
	}
}

//
//int get_note = 127;
//while(get_note > -1) {
//  if ( waiting_notes[get_note] ) {
//    break;
//  }
//  get_note--;
//}

void velocity(int8_t channel, int8_t velocity)
{
	int type = VELOCITY;
	int scale = cv_scale[type];
	for ( int row = 0, row < 4; row++ ) {
		int8_t main_conditions = cv_main_conditions(row, type, channel, note);
		if ( main_conditions && !polyphony[channel][note] ) ) {
			mono_note_on_hist[row][note] = HIGH;
			cv_out(row, velocity, scale);
			gate_out(row, HIGH);
		} else if ( main_conditions && polyphony[channel][note] && !digitalRead(gate[row]) ) {
			poly_note_on_hist[channel][note] = HIGH;
			cv_out(row, note, scale);
			gate_out(row, HIGH);
		} else if ( main_conditions && polyphony[channel][note] && digitalRead(gate[row]) ) {
			poly_note_on_hist[channel][note] = HIGH;
		}
	}
}

int8_t cv_main_conditions(int row, int type, int8_t channel, int8_t number)
{
	return cv_type[row] == type && cv_channel[row] == channel && cv_num_low[row] <= number && cv_num_high[row] >= number;
}

int8_t cv_note_on_conditions(int row, int8_t channel, int8_t number)
{
	return polyphony[channel][number] << 1 & percussion[row] << 2;
}