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

const bool SET = 0;
const bool GET = 1;

//Constants
const bool MIDI_DEBUG = true;

//struct MySettings : public midi::DefaultSettings
//{
//    static const bool UseSenderActiveSensing = false;
//    static const bool UseReceiverActiveSensing = false;
//};
//
//// Create a 'MIDI' object using MySettings bound to Serial2.
//MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial2, MIDI, MySettings);

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
//  MIDI.setHandleNoteOn(RecievedNoteOn);
//  MIDI.setHandleNoteOff(RecievedNoteOff);
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
  switch ( MIDI.read() ) {
    case true:
      break;
    case false:
      eval_buttonStates();
      CheckOutputTimers();
      break;
  }
}

void eval_buttonStates()
{
  int buttonStates = get_buttonStates();
  static int buttonStatesHistory = buttonStates;
  
  if ( change(buttonStates, &buttonStatesHistory) && buttonStates ) {
    while ( 1 ) {
      if ( MIDI.read() ) {
        learn();
      } else if ( get_buttonStates() == false ) {
        for ( int i = 0; i < 5; i++ ) {
          progState_type[i] = false;
        }
        save_learn_status();
        break; 
      }
    }
  }
}

void learn()
{
  for ( int row = 0; row < 4; row++ ) {
    if ( get_buttonStates() & (1 << row) ) {
      int savedCV[row] = { filterType(), filterData1(), MIDI.getChannel() };
      int savedTG[row] = { filterType(), filterData1(), MIDI.getChannel() };
    }
  }
}

const int MIDI_TYPES[5] = {
  midi::NoteOn,
  midi::AfterTouchPoly,
  midi::ControlChange,
  midi::AfterTouchChannel,
  midi::PitchBend
};

bool progState_type[i] {
  false, //note
  false, //atp
  false, //cc
  false, //atc
  false //pb
}

enum TYPES {
  NOTE,
  ATP,
  CC,
  ATC,
  PB
}

int filterType()
{
  int type = midi::InvalidType;

  for ( int i = 0; i < 5; i++ ) {
    if ( MIDI_TYPES[i] == MIDI.getType() ) {
      if ( progState_type[i] == false ) {
        progState_type[i] = true;
        type = MIDI.getType();
      }
      break;
    }
  }
  
  return type;
}

bool progState(bool input, int type)
{
  
  static bool types[7] = {0, 0, 0, 0, 0, 0, 0};
  int num = constrain((type >> 4) & 7, 0, 7);
  bool output = types[num] == input;
  types[num] = input;
  return output;
}

int filterData1()
{
  int data = MIDI.getData1();
  int InvalidData = -1;
  switch ( data ) {
    case midi::NoteOn:
      // channel 10 is drum channel.
      if ( MIDI.getChannel() == 10 ) {
        break;
      }
    case midi::ControlChange:
      break;
    case midi::AfterTouchPoly:
      break;
    default:
      data = InvalidData;
      break;
  }
  return data;
}

//returns a debounced button state.
int get_buttonStates()
{
  uint8_t state = getPINF_32u4_alt();
  static uint8_t stateHistory = state;

  const int T_DEBOUNCE = 100; // ms.
  unsigned long t0 = millis();

  // The while loop ends if:
  //  1: It reaches the debounce time: After which the readed state is returned.
  //  2: Or if the readed state is changed before the debounce time is reached: After which the previous state is returned.

  //convert to unsigned to prevent negative values;
  while ( unsigned long(millis() - t0) < T_DEBOUNCE ) {
    //break the loop and return previous value if the pin value changed before the debounce time was reached.
    if ( state != getPINF_32u4_alt() ) {
      return stateHistory;
    }
  }
  
  stateHistory = state;
  return state;
}

inline int getPINF_32u4_alt()
{
  //A0 == PF7, A1 == PF6, A2 == PF5, A3 == PF4. Specific for Atmega32u4.
  // B00001111 == 15.
  int state = (~PINF >> 4) & 15;
  return state;
}


void load_learn_status()
{
  for ( uint8_t i = 0; i < 4; i++ ) {
    CvChannels[i] = EEPROM.read(i);
    CvStates[i] = EEPROM.read(i+4);
    CvAfterNote[i] = EEPROM.read(i+8);

    GateChannels[i] = EEPROM.read(i+12);
    GateStates[i] = EEPROM.read(i+16);
    GateNotes[i] = EEPROM.read(i+20);
    DrumGateNoteAfter[i] = EEPROM.read(i+24);

    Nrpn_Msb_chans[i] = EEPROM.read(i+28);
    Nrpn_Msb_nums[i] = EEPROM.read(i+32);
    Nrpn_Msb_vals[i] = EEPROM.read(i+36);
    
    Nrpn_Lsb_chans[i] = EEPROM.read(i+40);
    Nrpn_Lsb_nums[i] = EEPROM.read(i+44);
    Nrpn_Lsb_vals[i] = EEPROM.read(i+48);
    
    LowRes_Msb_Nums[i] = EEPROM.read(i+52);
    HiRes_Lsb_Chans[i] = EEPROM.read(i+56);
    HiRes_Lsb_Nums[i] = EEPROM.read(i+60);
  }
}

void save_learn_status()
{
  for ( uint8_t i = 0; i < 4; i++ ) {
    EEPROM.update(i, CvChannels[i]);
    EEPROM.update(i+4, CvStates[i]);
    EEPROM.update(i+8, CvAfterNote[i]);

    EEPROM.update(i+12, GateChannels[i]);
    EEPROM.update(i+16, GateStates[i]);
    EEPROM.update(i+20, GateNotes[i]);
    EEPROM.update(i+24, DrumGateNoteAfter[i]);

    EEPROM.update(i+28, Nrpn_Msb_chans[i]);
    EEPROM.update(i+32, Nrpn_Msb_nums[i]);
    EEPROM.update(i+36, Nrpn_Msb_vals[i]);
    
    EEPROM.update(i+40, Nrpn_Lsb_chans[i]);
    EEPROM.update(i+44, Nrpn_Lsb_nums[i]);
    EEPROM.update(i+48, Nrpn_Lsb_vals[i]);
    
    EEPROM.update(i+52, LowRes_Msb_Nums[i]);
    EEPROM.update(i+56, HiRes_Lsb_Chans[i]);
    EEPROM.update(i+60, HiRes_Lsb_Nums[i]);
  }
}

bool change(int val, int *history)
{
  bool state = true;
  if ( *history == val ) {
    state = false;
  }
  *history = val;
  return state;
}
