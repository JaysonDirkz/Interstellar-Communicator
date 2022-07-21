/*
24-2-2021
Jason

TODO:
- Test op glitches wanneer je veel midi ontvangt.
	- Glitches? Kijk of dit verdwijnt met midiThruOff.
		- Ja? Kijk of je midithru in hardware kan door-routen.
- Kijk of je condensators kan vervangen voor minder slew op de pwm outputs.
- Active sensing timeout nu niet gebruikt. Blijf testen om te kijken of er noten blijven hangen. Zo niet? Dan kan het zo blijven.
*/

#include <MIDI.h>
#include <EEPROM.h>

#define CLOCK 2
#define RESET 3
#define GATE_1 4
#define GATE_2 5
#define GATE_3 7
#define GATE_4 8
#define BUTTON_1 A0
#define BUTTON_2 A1
#define BUTTON_3 A2
#define BUTTON_4 A3

//Constants

// const int8_t MONOVOICE = 0;
// const int8_t DUOVOICE = 1;
// const int8_t TRIVOICE = 2;
// const int8_t QUADVOICE = 3;

// const int8_t KEYS = 0;
// const int8_t VELOCITY = 1;
// const int8_t AFTERTOUCH = 2;
// const int8_t CCNORMAL = 3;
// const int8_t CC14BIT = 4;
// const int8_t PITCHBEND = 5;
// const int8_t PERCVELOCITY = 6;
// const int8_t PERCAFTERTOUCH = 7;
// const int8_t PERCTRIGGER = 8;
// const int8_t PERCGATE = 9;
// const int8_t CCNRPN = 10;
// const int8_t CCNRPN14BIT = 11;


// const int NOTE = 0;
// const int VELOCITY = 1;
// const int ATC = 2;
// const int ATP = 3;
// const int CC = 4;
// const int PB = 5;


const int GATES[4] = {GATE_1, GATE_2, GATE_3, GATE_4};

enum TYPE { NEG = -1, NOTE, VELOCITY, ATC, ATP, CONTROL, BEND };

const int cv_rightshift[6] = {0, 0, 0, 0, 6, 6};
const int cv_offset[6] = {128, 64, 64, 64, 0, 0};

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

void setup()
{
	setinputsandoutputs();
	configuremidisethandle();
	load_learn_status();
}

void setinputsandoutputs()
{
	MIDI.begin(MIDI_CHANNEL_OMNI);
 	// MIDI.turnThruOff();
	
	pinMode(CLOCK, OUTPUT);
	pinMode(RESET, OUTPUT);
	pinMode(GATE_1, OUTPUT);
	pinMode(GATE_2, OUTPUT);
	pinMode(GATE_3, OUTPUT);
	pinMode(GATE_4, OUTPUT);

	pinMode(BUTTON_1, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
	pinMode(BUTTON_2, INPUT_PULLUP);
	pinMode(BUTTON_3, INPUT_PULLUP);
	pinMode(BUTTON_4, INPUT_PULLUP);

	digitalWrite(CLOCK,LOW);
	digitalWrite(RESET,LOW);
	digitalWrite(GATE_1,LOW);
	digitalWrite(GATE_2,LOW);
	digitalWrite(GATE_3,LOW);
	digitalWrite(GATE_4,LOW);
	
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
	MIDI.setHandleNoteOn(note_on_vel);
	MIDI.setHandleNoteOff(note_off_vel);
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

int8_t cv_type[4];
int8_t cv_channel[4];

int8_t perc_type[4];
int8_t perc_channel[4];
int8_t perc_num[4];

int16_t polyphony[128];

int note_hist[4];
int note_row_hist[16];
int vel_row_hist;
// bool atp_note[128];
// int atp_row[4];
int at_row_hist;

int8_t msb_numbers[4];
int8_t lsb_numbers[4];
int8_t nrpn_msb_controls[4];
int8_t nrpn_lsb_controls[4];
int8_t nrpn_iter[4];
bool precision[4];
int8_t msb_control[4];

void load_learn_status()
{
	cv_type[0] = NOTE;
	cv_channel[0] = 1;

	perc_type[0] = NEG;
	perc_channel[0] = NEG;
	perc_num[0] = NEG;

	// atp_row[0] = NEG;


	cv_type[1] = NOTE;
	cv_channel[1] = 1;

	perc_type[1] = NEG;
	perc_channel[1] = NEG;
	perc_num[1] = NEG;

	// atp_row[1] = NEG;


	cv_type[2] = ATP;
	cv_channel[2] = 1;

	perc_type[2] = NOTE;
	perc_channel[2] = 10;
	perc_num[2] = 40;

	// atp_row[2] = NEG;


	cv_type[3] = ATP;
	cv_channel[3] = 1;

	perc_type[3] = NOTE;
	perc_channel[3] = 10;
	perc_num[3] = 41;

	// atp_row[3] = NEG;


	for ( int notes = 0; notes < 128; notes++ ) {
		int channels = 1;
		polyphony[notes] = channels;
		// atp_note[notes] = false;
	}

	for ( int channels = 0; channels < 16; channels++ ) {
		note_row_hist[channels] = NEG;
	}
	vel_row_hist = NEG;
	at_row_hist = NEG;

	msb_numbers[0] = NEG;
	lsb_numbers[0] = NEG;

	msb_numbers[1] = NEG;
	lsb_numbers[1] = NEG;

	msb_numbers[2] = NEG;
	lsb_numbers[2] = NEG;

	msb_numbers[3] = NEG;
	lsb_numbers[3] = NEG;

	nrpn_msb_controls[0] = 12;
	nrpn_lsb_controls[0] = 10;

	nrpn_msb_controls[1] = 12;
	nrpn_lsb_controls[1] = 42;

	nrpn_msb_controls[2] = 12;
	nrpn_lsb_controls[2] = 74;

	nrpn_msb_controls[3] = 12;
	nrpn_lsb_controls[3] = 106;

	nrpn_iter[0] = 0;
	nrpn_iter[1] = 0;
	nrpn_iter[2] = 0;
	nrpn_iter[3] = 0;

	precision[0] = false;
	precision[1] = false;
	precision[2] = false;
	precision[3] = false;
}

void loop()
{
	if ( !MIDI.read() ) {
		if ( PINF < B11110000) {
			learn_midi();
		}
	}
}

void learn_midi()
{
	while ( 1 ) {
		break;
	}
}

void note_on_vel(int8_t channel, int8_t note, int8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
	note_on(channel, note);
	on_vel(channel, note, velocity);
}

void note_on(int8_t channel, int8_t note)
{
	int type = NOTE;
	for ( int row = 0; row < 4; row++ ) {
		bool cv_main = cv_main_conditions(row, type, channel);
		const int8_t MONO_KEYS = B01;
		const int8_t POLY_KEYS = B11;

		switch ( bitRead(polyphony[note], channel - 1) << 1 | cv_main ) {
			case MONO_KEYS:
				cv_out(row, note, type);
				gate_out(row, true);
				note_hist[row] = note;
				goto END_LOOP;
			case POLY_KEYS:
				if ( note_row_hist[channel-1] != row ) {
					cv_out(row, note, type);
					gate_out(row, true);
					note_hist[row] = note;
					note_row_hist[channel-1] = row;
					// atp_note[note] = true;
					goto END_LOOP;
				}
				break;
			default:
				break;
		}

		if ( perc_main_conditions(row, type, channel, note) ) {
			gate_out(row, true);
			note_hist[row] = note;
			goto END_LOOP;
		}
	}
	END_LOOP:;
}

void on_vel(int8_t channel, int8_t note, int8_t velocity)
{
	int type = VELOCITY;
	for ( int row = 0; row < 4; row++ ) {
		bool cv_main = cv_main_conditions(row, type, channel);

		const int8_t MONO_VEL_OR_PERC_VEL = B01;
		const int8_t POLY_VEL = B11;

		switch ( bitRead(polyphony[note], channel - 1) << 1 | cv_main ) {
			case MONO_VEL_OR_PERC_VEL:
				cv_out(row, velocity, type);
				note_hist[row] = note;
				goto END_LOOP;
			case POLY_VEL:
				if ( vel_row_hist != row ) {
					cv_out(row, velocity, type);
					note_hist[row] = note;
					vel_row_hist = row;
					goto END_LOOP;
				}
				break;
			default:
				break;
		}
	}
	END_LOOP:;
}

void note_off_vel(int8_t channel, int8_t note, int8_t velocity)
{
	note_off(channel, note);
	off_vel(channel, note, velocity);
}

void note_off(int8_t channel, int8_t note)
{
	int type = NOTE;
	for ( int row = 0; row < 4; row++ ) {
		bool cv_main = cv_main_conditions(row, type, channel);
		bool perc_main = perc_main_conditions(row, type, channel, note);

		if ( note_hist[row] == note & (cv_main | perc_main) ) {
			gate_out(row, false);
			note_hist[row] = NEG;
			goto END_LOOP;
		}
	}
	END_LOOP:;
}

void off_vel(int8_t channel, int8_t note, int8_t velocity)
{
	int type = VELOCITY;
	for ( int row = 0; row < 4; row++ ) {
		const int8_t MONO_VEL = B11;
		const int8_t POLY_VEL = B11;

		if ( note_hist[row] == note & cv_main_conditions(row, type, channel) ) {
			cv_out(row, velocity, type);
			note_hist[row] = NEG;
			goto END_LOOP;
		}
	}
	END_LOOP:;
}

void atc(int8_t channel, int8_t aftertouch)
{
	int type = ATC;
	int row = 0;
	do {
		if ( cv_main_conditions(row, type, channel ) ) {
			cv_out(row, aftertouch, type);
			break;
		}
	} while ( ++row < 4 );
}

void atp(int8_t channel, int8_t note, int8_t aftertouch)
{
	int type = ATP;
	const int8_t MONO_ATP = B001;
	const int8_t POLY_ATP_PLAY = B111;

	int row = 2;
	do {
		switch ( (note_hist[row - 2] == note) << 2 | bitRead(polyphony[note], channel - 1) << 1 | cv_main_conditions(row, type, channel) ) {
			case MONO_ATP:
				cv_out(row, aftertouch, type);
				goto END_LOOP;
			case POLY_ATP_PLAY:
				cv_out(row, aftertouch, type);
				goto END_LOOP;
			default:
				break;
		}
	} while ( ++row < 4 );

	// const int8_t MONO_ATP = B0001;
	// const int8_t POLY_ATP_INIT = B0111;
	// const int8_t POLY_ATP_PLAY = B1011;
	// row = 0;
	// do {
	// 	switch ( /*(atp_row[row] == note) << 3 |*/ atp_note[note] << 2 | bitRead(polyphony[note], channel - 1) << 1 | cv_main_conditions(row, type, channel) ) {
	// 		case MONO_ATP:
	// 			cv_out(row, aftertouch, type);
	// 			goto END_LOOP;
	// 		case POLY_ATP_INIT:
	// 			if ( at_row_hist != row ) {
	// 				atp_note[note] = false;
	// 				// atp_row[row] = note;
	// 				at_row_hist = row;

	// 				case POLY_ATP_PLAY:
	// 					cv_out(row, aftertouch, type);
	// 					goto END_LOOP;
	// 			}
	// 			break;
	// 		default:
	// 			// atp_row[row] = NEG;
	// 			break;
	// 	}
	// } while ( ++row < 4 );
	END_LOOP:;
}

//test deze

void control_change(int8_t channel, int8_t number, int8_t control)
{
	int type = CONTROL;
	
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

	int row = 0;
	do {
		bool cv_main = cv_main_conditions(row, type, channel);

		bool msb_number = msb_numbers[row] == number;
		bool lsb_number = lsb_numbers[row] == number;

		bool nrpn_msb_ctrl = nrpn_msb_controls[row] == control;
		bool nrpn_lsb_ctrl = nrpn_lsb_controls[row] == control;

		switch ( nrpn_iter[row] << 7 | (nrpn_lsb_ctrl & nrpn_98) << 6 | (nrpn_msb_ctrl & nrpn_99) << 5 | lsb_38 << 4 | msb_6 << 3 | lsb_number << 2 | msb_number << 1 | cv_main ) {
			case NRPN_99_CTRL:
				nrpn_iter[row] = 1;
				goto END_LOOP;
			case NRPN_98_CTRL:
				nrpn_iter[row] = 2;
				goto END_LOOP;
			case MSB_6:
				if ( precision[row] ) {
					msb_control[row] = control;
				} else {
					cv_out(row, (control << 6) + 64, type);
					nrpn_iter[row] = 0;
				}
				goto END_LOOP;
			case LSB_38:
				cv_out(row, msb_control[row] << 7 | control, type);
				nrpn_iter[row] = 0;
				goto END_LOOP;
			case MSB:
				if ( precision[row] ) {
					msb_control[row] = control;
				} else cv_out(row, (control << 6) + 64, type);
				goto END_LOOP;
			case LSB:
				cv_out(row, msb_control[row] << 7 | control, type);
				goto END_LOOP;
			default:
				break;
		}
	} while ( ++row < 4 );
	END_LOOP:;
}

void pitchbend(int8_t channel, int pitch)
{
	int type = BEND;
	int row = 0;
	do {
		if ( cv_main_conditions(row, type, channel ) ) {
			cv_out(row, pitch, type);
			break;
		}
	} while ( ++row < 4 );
	END_LOOP:;
}

inline bool cv_main_conditions(int row, int type, int8_t channel)
{
	return
	cv_type[row] == type &
	cv_channel[row] == channel;
}

inline bool perc_main_conditions(int row, int type, int8_t channel, int8_t number)
{
	return
	perc_type[row] == type &
	perc_channel[row] == channel &
	perc_num[row] == number;
}

inline void cv_out(int row, int number, int type)
{
	uint8_t value = (number >> cv_rightshift[type]) + cv_offset[type];
	switch (row) {
		case 0: PWM6 = value; break;
		case 1: PWM9 = value; break;
		case 2: PWM10 = value; break;
		case 3: PWM13 = value; break;
	}
	// if ( !MIDI.getThruState() ) {
	// 	MIDI.sendControlChange(0, value >> 7, row + 1);
	// 	MIDI.sendControlChange(32, value & 127, row + 1);
	// }
}

inline void gate_out(int row, bool state)
{
	digitalWrite(GATES[row], state);
	// if ( !MIDI.getThruState() ) {
	// 	MIDI.sendNoteOn(1, state, row + 1);
	// }
}

int clock_count = 2;
bool clock_active = false;

void clock_tick()
{
	switch ( clock_count << 1 | clock_active ) {
		case 1: /*digitalWrite(RESET, LOW);*/ digitalWrite(CLOCK, HIGH); clock_count = 1; break;
		case 3: digitalWrite(RESET, LOW); digitalWrite(CLOCK, LOW); clock_count = 2; break;
		case 5: clock_count = 0; break;
		default: break;
	}
}

void clock_start()
{
	digitalWrite(RESET, HIGH);
	clock_count = 0;
	clock_active = true;
}

void clock_continue()
{
	clock_active = true;
}

void clock_stop()
{
	digitalWrite(CLOCK, LOW);
	digitalWrite(RESET, LOW);
	gate_out(0, LOW);
	gate_out(1, LOW);
	gate_out(2, LOW);
	gate_out(3, LOW);
	clock_active = false;
}

// void sense_timeout()
// {
// 	clock_stop();
// }