/*
Logboek:
Interstellar_Communicator_PWM_v20 [3-6-21]:
- Logboek aangemaakt.
- Midi clock ticks veranderd naar altijd output.

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

enum TYPE { NEG = -1, NOTE, VELOCITY, ATC, ATP, CONTROL, BEND, PENDING_AT };

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

//Saved in eeprom
//------
int8_t cv_type[4];
int8_t cv_channel[4];

int8_t perc_type[4];
int8_t perc_channel[4];
int8_t perc_num[4];

bool polyphony[16];

int8_t msb_numbers[4];
// int8_t lsb_numbers[4];
int8_t nrpn_msb_controls[4];
int8_t nrpn_lsb_controls[4];
bool precision[4];
//------

int note_hist[4];
int note_row_hist[16];
int vel_row_hist;
int at_row_hist;

int8_t nrpn_iter[4];
int8_t msb_controller[4];

void load_learn_status()
{
  for ( int channels = 0; channels < 16; channels++ ) {
		note_row_hist[channels] = NEG;
    polyphony[channels] = false;
	}
	vel_row_hist = NEG;
	at_row_hist = NEG;
  nrpn_iter[0] = 0;
	nrpn_iter[1] = 0;
	nrpn_iter[2] = 0;
	nrpn_iter[3] = 0;

  /*
	for ( int row = 0; row < 4; row++ ) {
		cv_type[row] = 				EEPROM.read(row);
		cv_channel[row] = 			EEPROM.read(row+4);
		perc_type[row] = 			EEPROM.read(row+8);
		perc_channel[row] = 		EEPROM.read(row+12);
		perc_num[row] = 			EEPROM.read(row+16);
		polyphony[row] = 			EEPROM.read(row+20);
		polyphony[row + 4] = 		EEPROM.read(row+24);
		polyphony[row + 8] = 		EEPROM.read(row+28);
		polyphony[row + 12] = 		EEPROM.read(row+32);
		msb_numbers[row] = 			EEPROM.read(row+36);
		// lsb_numbers[row] = 			EEPROM.read(row+40);
		nrpn_msb_controls[row] = 	EEPROM.read(row+44);
		nrpn_lsb_controls[row] = 	EEPROM.read(row+48);
		precision[row] = 			EEPROM.read(row+52);
	}
*/

	cv_type[0] = NOTE;
	cv_channel[0] = 1;

	perc_type[0] = NEG;
	perc_channel[0] = NEG;
	perc_num[0] = NEG;


	cv_type[1] = VELOCITY;
	cv_channel[1] = 1;

	perc_type[1] = NOTE;
	perc_channel[1] = 10;
	perc_num[1] = 36;


	cv_type[2] = NOTE;
	cv_channel[2] = 2;

	perc_type[2] = NEG;
	perc_channel[2] = NEG;
	perc_num[2] = NEG;


	cv_type[3] = VELOCITY;
	cv_channel[3] = 10;

	perc_type[3] = NOTE;
	perc_channel[3] = 10;
	perc_num[3] = 38;

	msb_numbers[0] = NEG;
	// lsb_numbers[0] = NEG;

	msb_numbers[1] = NEG;
	// lsb_numbers[1] = NEG;

	msb_numbers[2] = NEG;
	// lsb_numbers[2] = NEG;

	msb_numbers[3] = NEG;
	// lsb_numbers[3] = NEG;

	nrpn_msb_controls[0] = 12;
	nrpn_lsb_controls[0] = 10;

	nrpn_msb_controls[1] = 12;
	nrpn_lsb_controls[1] = 42;

	nrpn_msb_controls[2] = 12;
	nrpn_lsb_controls[2] = 74;

	nrpn_msb_controls[3] = 12;
	nrpn_lsb_controls[3] = 106;

	precision[0] = false;
	precision[1] = false;
	precision[2] = false;
	precision[3] = false;
	
}

void loop()
{
	if ( MIDI.read() ) {
	} /*else if ( PINF < B11110000) {
		eval_buttonStates();
	}*/
}

uint16_t erase_row[4] = { 0, 0, 0, 0 };

void eval_buttonStates()
{
	int buttonStates = get_buttonStates(100);
	static int buttonStates_hist = 0;
	// MIDI.sendNoteOn(buttonStates, 120, 1);
  // MIDI.sendNoteOff(buttonStates_hist, 0, 1);
	if ( change(buttonStates, &buttonStates_hist) && buttonStates ) {
		int erase_counter[4];
		static int erase_counter_hist[4] = { 0, 0, 0, 0 };
		int erase_counter_diff[4];
		for ( int row = 0; row < 4; row++ ) {
			erase_counter[row] = millis();
			erase_counter_diff[row] = delta(erase_counter[row], &erase_counter_hist[row]);

			++erase_row[row];
		}

		MIDI.setHandleNoteOn(learn_note_on_vel);
		MIDI.disconnectCallbackFromType(midi::NoteOff);
		MIDI.setHandleControlChange(learn_control_change);
		MIDI.setHandlePitchBend(learn_pitchbend);
		MIDI.setHandleAfterTouchChannel(learn_atc);
		MIDI.setHandleAfterTouchPoly(learn_atp);
		MIDI.disconnectCallbackFromType(midi::Clock);
		MIDI.disconnectCallbackFromType(midi::Start);
		MIDI.disconnectCallbackFromType(midi::Continue);
		MIDI.disconnectCallbackFromType(midi::Stop);

		while ( 1 ) {
			buttonStates = getPINF_32u4_alt();
			if ( MIDI.read() ) {
			} else if ( buttonStates == false ) {
				if ( get_buttonStates(100) == false ) break;
			}
		}

		for ( int row = 0; row < 4; row++ ) {
			if ( erase_counter_diff < 500 && erase_row[row] > 1 && bitRead(buttonStates, row) ) {
				erase_row[row] = 0;

				cv_type[row] = NEG;
				cv_channel[row] = NEG;

				perc_type[row] = NEG;
				perc_channel[row] = NEG;
				perc_num[row] = NEG;

				polyphony[row] = false;
				polyphony[row + 4] = false;
				polyphony[row + 8] = false;
				polyphony[row + 12] = false;

				msb_numbers[row] = NEG;
				// lsb_numbers[row] = NEG;
				nrpn_msb_controls[row] = NEG;
				nrpn_lsb_controls[row] = NEG;

				precision[row] = false;
			}
		}

		save_learn_status();

		// switch ( cv_type[0] ) {
		// 	case NOTE:
		// 	MIDI.sendNoteOn(64, 64, 1); break;
		// 	case CONTROL:
		// 	MIDI.sendControlChange(64, 64, 1); break;
		// 	case ATC:
		// 	MIDI.sendAfterTouch(64, 1); break;
		// 	case ATP:
		// 	MIDI.sendAfterTouch(64, 64, 1); break;
		// 	case BEND:
		// 	MIDI.sendPitchBend(64, 1); break;
		// 	default:
		// 		break;
		// }

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
	}
}

void save_learn_status()
{
	for ( int row = 0; row < 4; row++ ) {
		EEPROM.update(row, 	  cv_type[row]);
		EEPROM.update(row+4,  cv_channel[row]);
		EEPROM.update(row+8,  perc_type[row]);
		EEPROM.update(row+12, perc_channel[row]);
		EEPROM.update(row+16, perc_num[row]);
		EEPROM.update(row+20, polyphony[row]);
		EEPROM.update(row+24, polyphony[row + 4]);
		EEPROM.update(row+28, polyphony[row + 8]);
		EEPROM.update(row+32, polyphony[row + 12]);
		EEPROM.update(row+36, msb_numbers[row]);
		// EEPROM.update(row+40, lsb_numbers[row]);
		EEPROM.update(row+44, nrpn_msb_controls[row]);
		EEPROM.update(row+48, nrpn_lsb_controls[row]);
		EEPROM.update(row+52, precision[row]);
	}
}

//returns a debounced button state.
int get_buttonStates(int t_debounce)
{
	uint8_t state = getPINF_32u4_alt();
	static uint8_t stateHistory = state;
  
	unsigned long t0 = millis();

	// The while loop ends if:
	//  1: It reaches the debounce time: After which the readed state is returned.
	//  2: Or if the readed state is changed before the debounce time is reached: After which the previous state is returned.

	//convert to unsigned to prevent negative values;
	while ( uint32_t(millis() - t0) < t_debounce ) {
		//break the loop and return previous value if the pin value changed before the debounce time was reached.
		if ( state != getPINF_32u4_alt() ) {
		return stateHistory;
		}
	}
	
	stateHistory = state;
	return state;
}

inline uint8_t getPINF_32u4_alt()
{
	//A0 == PF7, A1 == PF6, A2 == PF5, A3 == PF4. Specific for Atmega32u4.
	// B00001111 == 15.
	//   int state = (~PINF >> 4) & 15;
	uint8_t n_PINF = ~PINF;
	uint8_t state = ( (n_PINF & 16) >> 1 | (n_PINF & 32) >> 3 | (n_PINF & 64) >> 5 | (n_PINF & 128) >> 7 ) & 15;
	return state;
}

void learn_note_on_vel(int8_t channel, int8_t note, int8_t velocity)
{
	int count_keys = 0;
	const int8_t KEYS = B01;
	const int8_t PERC = B11;

	for ( int row = 0; row < 4; row++ ) {
		erase_row[row] = false;
		
		switch ( (channel == 10) << 1 | bitRead(getPINF_32u4_alt(), row) ) {
		case KEYS:
			switch ( count_keys ) {
			case 0:
				cv_type[row] = NOTE;
				cv_channel[row] = channel;
				++count_keys;
				break;
			case 1:
				cv_type[row] = VELOCITY;
				cv_channel[row] = channel;
				++count_keys;
				break;
			case 2:
				cv_type[row] = PENDING_AT;
				cv_channel[row] = channel;
				break;
			default:
				break;
			}
			break;
		case PERC:
			cv_type[row] = VELOCITY;
			cv_channel[row] = channel;
			perc_type[row] = NOTE;
			perc_channel[row] = channel;
			perc_num[row] = note;
			break;
		default:
			break;
		}
	}

	count_keys = 0;
	for ( int row = 0; row < 4; row++ ) {
		if ( cv_type[row] == NOTE && cv_channel[row] == channel ) {
			++count_keys;
		}
	}
	polyphony[channel] = count_keys > 1;
}

void learn_atc(int8_t channel, int8_t aftertouch)
{
	for ( int row = 0; row < 4; row++ ) {
		erase_row[row] = false;

		if ( cv_channel[row] == channel && cv_type[row] == PENDING_AT && bitRead(getPINF_32u4_alt(), row) ) {
			cv_type[row] = ATC;
		}
	}
}

void learn_atp(int8_t channel, int8_t note, int8_t aftertouch)
{
	for ( int row = 0; row < 4; row++ ) {
		erase_row[row] = false;

		if ( cv_channel[row] == channel && cv_type[row] == PENDING_AT && bitRead(getPINF_32u4_alt(), row) ) {
			cv_type[row] = ATP;
		}
	}
}

void learn_control_change(int8_t channel, int8_t number, int8_t control)
{
	static bool msb_ready = false;
	static bool nrpn_ready = false;

	for ( int row = 0; row < 4; row++ ) {
		erase_row[row] = false;
		precision[row] = false;
		
		if ( bitRead(getPINF_32u4_alt(), row) ) {
			switch ( number ) {
			case 38:
				if ( nrpn_ready && msb_ready ) {
					precision[row] = true;

					nrpn_ready = false;
					msb_ready = false;
				}
				break;
			case 6:
				if ( nrpn_ready ) {
					cv_type[row] = CONTROL;
					cv_channel[row] = channel;

					msb_ready = true;
				}
				break;
			case 98:
				if ( control == 127 ) {
					break;
				} else if ( msb_ready ) {
					nrpn_lsb_controls[row] = control;

					nrpn_ready = true;
				}
				break;
			case 99:
				if ( control == 127 ) {
					break;
				} else {
					nrpn_msb_controls[row] = control;
				}
				break;
			default:
				cv_type[row] = CONTROL;
				cv_channel[row] = channel;

				if ( msb_numbers[row] + 32 == number && msb_ready ) {
					precision[row] = true;
				}
				
				if ( msb_ready || msb_numbers[row] == number && cv_channel[row] == channel ) {
					msb_ready = false;
				} else {
					msb_numbers[row] = number;

					msb_ready = true;
				}

				break;
			} 
		}
	}
}

void learn_pitchbend(int8_t channel, int pitch)
{
	for ( int row = 0; row < 4; row++ ) {
		erase_row[row] = false;

		if ( bitRead(getPINF_32u4_alt(), row) ) {
			cv_type[row] = BEND;
			cv_channel[row] = channel;
		}
	}
}

void note_on_vel(int8_t channel, int8_t note, int8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
	note_on(channel, note);
	on_vel(channel, note, velocity);
  // MIDI.sendControlChange(note, note_hist[0], 1);
  // MIDI.sendControlChange(note, note_hist[1], 2);
  // MIDI.sendControlChange(note, note_hist[2], 3);
  // MIDI.sendControlChange(note, note_hist[3], 4);
}

void note_on(int8_t channel, int8_t note)
{
	int type = NOTE;
	for ( int row = 0; row < 4; row++ ) {
		bool cv_main = cv_main_conditions(row, type, channel);
		const int8_t MONO_KEYS = B01;
		const int8_t POLY_KEYS = B11;

		switch ( polyphony[channel - 1] << 1 | cv_main ) {
			case MONO_KEYS:
				cv_out(row, note, type);
				gate_out(row, true);
				// note_hist_2[row] = note_hist_1[row];
				note_hist[row] = note;
				goto END_LOOP;
			case POLY_KEYS:
				if ( note_row_hist[channel-1] != row ) {
					cv_out(row, note, type);
					gate_out(row, true);
					// note_hist_2[row] = note_hist_1[row];
					note_hist[row] = note;
					note_row_hist[channel-1] = row;
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

		switch ( polyphony[channel - 1] << 1 | cv_main ) {
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
  // MIDI.sendControlChange(note, note_hist[0], 1);
  // MIDI.sendControlChange(note, note_hist[1], 2);
  // MIDI.sendControlChange(note, note_hist[2], 3);
  // MIDI.sendControlChange(note, note_hist[3], 4);
}

void note_off(int8_t channel, int8_t note)
{
	int type = NOTE;
	for ( int row = 0; row < 4; row++ ) {
		bool cv_main = cv_main_conditions(row, type, channel);
		bool perc_main = perc_main_conditions(row, type, channel, note);

    // MIDI.sendControlChange(note, note_hist[row], row+1);
    // MIDI.sendControlChange(note, perc_main, row+1);
		if ( note_hist[row] == note & (cv_main | perc_main) ) {
			gate_out(row, false);
			note_hist[row] = NEG;
			goto END_LOOP;
		}
	}
	END_LOOP:;
}

//alleen voor percussie
void off_vel(int8_t channel, int8_t note, int8_t velocity)
{
	int type = VELOCITY;
	for ( int row = 0; row < 4; row++ ) {
		// const int8_t MONO_VEL = B11;
		// const int8_t POLY_VEL = B11;

    bool cv_main = cv_main_conditions(row, type, channel);
		bool perc_main = perc_main_conditions(row, NOTE, channel, note);

    // MIDI.sendControlChange(note, note_hist[row], row+1);
    // MIDI.sendControlChange(note, cv_main_conditions(row, type, channel), row+1);
		if ( note_hist[row] == note & cv_main & perc_main ) {
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
		switch ( (note_hist[row - 2] == note) << 2 | polyphony[channel - 1] << 1 | cv_main_conditions(row, type, channel) ) {
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

		bool msb_num = msb_numbers[row] == number;
		bool lsb_num = msb_numbers[row] + 32 == number;

		bool nrpn_msb_ctrl = nrpn_msb_controls[row] == control;
		bool nrpn_lsb_ctrl = nrpn_lsb_controls[row] == control;

		switch ( nrpn_iter[row] << 7 | (nrpn_lsb_ctrl & nrpn_98) << 6 | (nrpn_msb_ctrl & nrpn_99) << 5 | lsb_38 << 4 | msb_6 << 3 | lsb_num << 2 | msb_num << 1 | cv_main ) {
			case NRPN_99_CTRL:
				nrpn_iter[row] = 1;
				goto END_LOOP;
			case NRPN_98_CTRL:
				nrpn_iter[row] = 2;
				goto END_LOOP;
			case MSB_6:
				if ( precision[row] ) {
					msb_controller[row] = control;
				} else {
					cv_out(row, (control << 6) + 64, type);
					nrpn_iter[row] = 0;
				}
				goto END_LOOP;
			case LSB_38:
				cv_out(row, msb_controller[row] << 7 | control, type);
				nrpn_iter[row] = 0;
				goto END_LOOP;
			case MSB:
				if ( precision[row] ) {
					msb_controller[row] = control;
				} else cv_out(row, (control << 6) + 64, type);
				goto END_LOOP;
			case LSB:
				cv_out(row, msb_controller[row] << 7 | control, type);
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
	const int CV_RSHIFT[6] = {0, 0, 0, 0, 6, 6};
	const int CV_OFFSET[6] = {128, 64, 64, 64, 0, 0};

	uint8_t value = (number >> CV_RSHIFT[type]) + CV_OFFSET[type];
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
	const int GATES[4] = {GATE_1, GATE_2, GATE_3, GATE_4};

	digitalWrite(GATES[row], state);
	// if ( !MIDI.getThruState() ) {
	// 	MIDI.sendNoteOn(1, state, row + 1);
	// }
}

int clock_count = 2;
int clock_count_saved = clock_count;
bool clock_active = true;

void clock_tick()
{
	switch ( clock_count << 1 | clock_active ) {
		case B001: /*digitalWrite(RESET, LOW);*/ digitalWrite(CLOCK, HIGH); clock_count = 1; break;
		case B011: digitalWrite(RESET, LOW); digitalWrite(CLOCK, LOW); clock_count = 2; break;
		case B101: clock_count = 0; break;
		default: break;
	}
}

void clock_start()
{
	digitalWrite(RESET, HIGH);
	clock_count = 0;
	//clock_active = true;
}

void clock_continue()
{
	//clock_active = true;
  clock_count = clock_count_saved;
}

void clock_stop()
{
	digitalWrite(CLOCK, LOW);
	digitalWrite(RESET, LOW);
	gate_out(0, LOW);
	gate_out(1, LOW);
	gate_out(2, LOW);
	gate_out(3, LOW);
	//clock_active = false;
  clock_count_saved = clock_count;
}

// void sense_timeout()
// {
// 	clock_stop();
// }

//Algemene functies

bool change(int val, int *history)
{
  bool state = true;
  if ( *history == val ) {
    state = false;
  }
  *history = val;
  return state;
}

int delta(int in, int *history)
{
	int out = in - *history;
	*history = in;
	return out;
}
