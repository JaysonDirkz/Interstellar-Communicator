/*
Logboek:
Interstellar_Communicator_PWM_v22 [12-1-22]:
- Uitprobeersel met functie pointers arrays.
Interstellar_Communicator_PWM_v21 [12-1-22]:
- Gekopieerd naar nieuwe map ( PWM data driven ).
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
const int CV_RSHIFT[6] = {0, -1, -1, -1, 6, 6};

int8_t cvRowOfTypeAtChannel[7][17];
int8_t gateRowOfTypeAtChannel[7][17];

void setup()
{
    for ( int type = 0; type < 7; type++ )
    {
        for ( int chan = 0; chan < 17; chan++ )
        {
            cvRowOfTypeAtChannel[type][chan] = 0;
            gateRowOfTypeAtChannel[type][chan] = 0;
        }
    }
	setinputsandoutputs();
	configuremidisethandle();
	load_learn_status();
}

void setinputsandoutputs()
{
	MIDI.begin(MIDI_CHANNEL_OMNI);
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

void r(){}

void output_ON_gate1_pwm1()
{
  pwm1 = MIDI.getData1();
  gate1 = HIGH;
}

*typeRouterLogic[4][8]
{
  // noteOn, noteOff
  { r, output_ON_gate1_pwm1, }
  ...
}

*typeRouterPwm[4][8]
{
  // noteOn, noteOff
  { r, output_ON_gate1_pwm1, }
  ...
}

*typeRouterSync[4]
{
  // clock, start, continue, stop
  { clock_tick, clock_start, clock_continue, clock_stop }
}

void checkType()
{
  typeRouterLogic[0][MIDI.getType()];
  typeRouterLogic[1][MIDI.getType()];
  typeRouterLogic[2][MIDI.getType()];
  typeRouterLogic[3][MIDI.getType()];
  
  typeRouterPwm[0][MIDI.getType()];
  typeRouterPwm[1][MIDI.getType()];
  typeRouterPwm[2][MIDI.getType()];
  typeRouterPwm[3][MIDI.getType()];

  typeRouterSync[MIDI.getType()];
}

*channelRouter[16] =
{
  r,
  checkType,
  ...
}

int* cvPointerLast[4];
int* gatePointerLast[4];

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
                //reset value at the saved memory address.
                *cvPointerLast[row] = 0;

                //save row and channel
                cvRowOfTypeAtChannel[TYPE::NOTE][channel] = row;
                for ( int8_t i = 0; i < 127; i++ ) noteChannel[i] = channel;

                // write memory address to pointer.
                cvPointerLast[row] = &cvRowOfTypeAtChannel[TYPE::NOTE][channel];
                
				++count_keys;
				break;
			case 1:
                //reset value at the saved memory address.
                *cvPointerLast[row] = 0;
                
                cvRowOfTypeAtChannel[TYPE::VELOCITY][channel] = row;

                // write memory address to pointer.
                cvPointerLast[row] = &cvRowOfTypeAtChannel[TYPE::VELOCITY][channel];
                
				++count_keys;
				break;
			case 2:
                //reset value at the saved memory address.
                *cvPointerLast[row] = 0;
                
                cvRowOfTypeAtChannel[TYPE::PENDING_AT][channel] = row;

                // write memory address to pointer.
                cvPointerLast[row] = &cvRowOfTypeAtChannel[TYPE::PENDING_AT][channel];
				break;
			default:
				break;
			}
			break;
		case PERC:
            //reset value at the saved memory address.
            *cvPointerLast[row] = 0;
            *gatePointerLast[row] = 0;
            *notePointerLast[row] = 0;

            //save row and channel.
            cvRowOfTypeAtChannel[TYPE::VELOCITY][channel] = row;
            gateRowOfTypeAtChannel[TYPE::NOTE][channel] = row;
            noteChannel[note] = channel;

            // write memory address to pointer.
            cvPointerLast[row] = cvRowOfTypeAtChannel[TYPE::VELOCITY][channel];
            gatePointerLast[row] = gateRowOfTypeAtChannel[TYPE::NOTE][channel];
            notePointerLast[row] = noteChannel[note];
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

void note_on_vel(int8_t channel, int8_t note, int8_t velocity) //Moeten 8-bits zijn, geen int (16-bits). (met int wordt de data corrupt).
{
	note_on(channel, note);
	on_vel(channel, note, velocity);
}

void anything()
{
    cv_out(cvRowOfTypeAtChannel[MIDI.getType()][MIDI.getChannel()], MIDI.getData2(), MIDI.getType());
    gate_out(gateRowOfTypeAtChannel[MIDI.getType()], gateState[MIDI.getType()]);
}

gateRowOfTypeAtChannel[7] =
{
    gateTypeRoute[0] = 0;
    gateTypeRoute[1] = 0;
    gateTypeRoute[2] = 0;
    gateTypeRoute[2] = MIDI.getChannel()]
    ... [MIDI.getData2()]
}

void note_on(int8_t channel, int8_t note)
{
    cv_out(cvRowOfTypeAtChannel[TYPE::NOTE][channel], note, TYPE::NOTE);
    gate_out(gateRowOfTypeAtChannel[TYPE::NOTE][noteChannel[note]], HIGH);
    checkPoly[];
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

void learn_atc(int8_t channel, int8_t aftertouch)
{
    for ( int row = 0; row < 4; row++ ) {
        erase_row[row] = false;

        if ( cv_channel[row] == channel && cv_type[row] == PENDING_AT && bitRead(getPINF_32u4_alt(), row) ) {
            cv_type[row] = ATC;
        }
    }
}

void atc(int8_t channel, int8_t aftertouch)
{
    int row = typeChanRow[TYPE::ATC][channel];
    cv_out(row, aftertouch, TYPE::ATC);
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

void atp(int8_t channel, int8_t note, int8_t aftertouch)
{
    int row = atpAndNoteChannelRow[channel];
    cv_out(row, aftertouch, TYPE::ATP);

    
//	int type = ATP;
//	const int8_t MONO_ATP = B001;
//	const int8_t POLY_ATP_PLAY = B111;
//
//	int row = 2;
//	do {
//		switch ( (note_hist[row - 2] == note) << 2 | polyphony[channel - 1] << 1 | cv_main_conditions(row, type, channel) ) {
//			case MONO_ATP:
//				cv_out(row, aftertouch, type);
//				goto END_LOOP;
//			case POLY_ATP_PLAY:
//				cv_out(row, aftertouch, type);
//				goto END_LOOP;
//			default:
//				break;
//		}
//	} while ( ++row < 4 );
//	END_LOOP:;
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

int pitchChannelRow[16] = {0};

void learn_pitchbend(int8_t channel, int pitch)
{   
    for ( int row = 0; row < 4; row++ )
    {
        erase_row[row] = false;

        if ( bitRead(getPINF_32u4_alt(), row) )
        {   
            int chanAtRow[4] = {0};
                     
            if ( typeChanRow[TYPE::BEND][chanAtRow[row]] == row ) typeChanRow[TYPE::BEND][chanAtRow[row]] = 0;
            
            typeChanRow[TYPE::BEND][channel] = row;
            chanAtRow[row] = channel;
        }
    }
}

void pitchbend(int8_t channel, int pitch)
{
    int row = typeChanRow[TYPE::BEND][channel];
    cv_out(row, pitch, TYPE::BEND);
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

void cv_out(int row, int number, TYPE type)
{
    uint8_t value = number >> CV_RSHIFT[type];
    switch (row) {
        case 1: PWM6 = value; break;
        case 2: PWM9 = value; break;
        case 3: PWM10 = value; break;
        case 4: PWM13 = value; break;
        default: break;
    }
}

void gate_out(int row, bool state)
{
	const int GATES[4] = {GATE_1, GATE_2, GATE_3, GATE_4};

    if ( row ) digitalWrite(GATES[row], state);
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
