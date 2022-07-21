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

void setinputsandoutputs()
{ 
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

void setup()
{
  MIDI.begin(MIDI_CHANNEL_OMNI);
//  MIDI.turnThruOff();

  setinputsandoutputs();
  
  confpwm();
  //Sets the DAC outputs on lowest output, which translates to 0V. Which is NoteNumber C-1 and midi NoteNumber 0.
  pwm_out(0, 128);
  pwm_out(1, 128);
  pwm_out(2, 128);
  pwm_out(3, 128);
}

void loop()
{
  CheckMidiIn();
//  CheckMidiTimer();
  ReadButtonPins();
}

void CheckMidiIn()
{
  if ( MIDI.read() ) {
//    MidiTimer = millis();
  }
}

void CheckMidiTimer()
{
  int16_t TimeDifference = millis() - MidiTimer;
  if ( TimeDifference > 500 ) {
    MidiTimer = millis();
    RecievedStop();
  }
}

void ReadButtonPins()
{
  //A0 == PF7, A1 == PF6, A2 == PF5, A3 == PF4. Specific for Atmega32u4.
  ButtonStates = ~PINF & B11110000;
  static int ButtonStatesHistory = ButtonStates;
  
  if ( change(ButtonStates, &ButtonStatesHistory) ) {
    if ( ButtonStates == 0 ) {     
    }
  }
}

//int pwm_out[4] = {PWM6, PWM9, PWM10, PWM13};
//int pwm_out[4] = {&PWM6, &PWM9, &PWM10, &PWM13};
//int* pwm_out[4] = {PWM6, PWM9, PWM10, PWM13};
//int* pwm_out[4] = {&PWM6, &PWM9, &PWM10, &PWM13};
//pwm_out[i] = pwmVal;
void pwm_out(uint8_t i, uint8_t pwmVal)
{
  switch ( i ) {
    case 0: PWM6 = pwmVal; break;
    case 1: PWM9 = pwmVal; break;
    case 2: PWM10 = pwmVal; break;
    case 3: PWM13 = pwmVal; break;
    default: break;
  }
  if (MIDI_DEBUG) MIDI.sendControlChange(1, pwmVal >> 7, i+1); MIDI.sendControlChange(33, pwmVal & 127, i+1);
}

//Nevenfuncties.
//-------------------------------------------------------
bool change(int val, int *history)
{
  bool state = true;
  if ( *history == val ) {
    state = false;
  }
  *history = val;
  return state;
}
