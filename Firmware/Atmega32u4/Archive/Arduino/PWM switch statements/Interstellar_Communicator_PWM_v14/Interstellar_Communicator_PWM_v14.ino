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
  CheckMidiIn();
  CheckMidiTimer();
  ReadButtonPins();
  CheckOutputTimers();
  erase_learn_counter();
}

void CheckMidiIn()
{
  if ( MIDI.read() ) {
    MidiTimer = millis();
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
  FillButtonStates();
  if ( ButtonStates == 0 && LearnMode == true ) {
    learn_NoteNumbers_count = 0;
    
    SetPolyArrays();
    
    CheckVoiceType();
    
    MayProgramAftertouch = ButtonStates;
    
    save_learn_status();
    
    LearnMode = false;
    
  } else if ( ButtonStates && LearnMode == false ) {
    LearnMode = true;
  }
}

void FillButtonStates() //A0 == PF7, A1 == PF6, A2 == PF5, A3 == PF4. Specific for Atmega32u4.
{
  static int8_t SavedPinRead = 0;
  int8_t PinRead = lowByte(~PINF) & B11110000;
  if ( PinRead != SavedPinRead ) {
    SavedPinRead = PinRead;
    for ( int8_t i = 0; i < 7; i++ ) {
      bitWrite(ButtonStates, i, bitRead(PinRead, -i + 7));
    }
  }
}

void SetPolyArrays()
{
  for ( int8_t i = 0; i < SizeofVoiceTypeKeys; i++ ) {
    VoiceTypeKeys[i] = NOTHING;
    LastRowKeys[i] = NOTHING;
    for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
      PolyNotes[i][n] = NOTHING;
    }
  }
}

void CheckVoiceType()
{
  for (byte i = 0; i < 4; i++) {
    if (CvStates[i] == KEYS) {
      VoiceTypeKeys[CvChannels[i]]++;
      LastRowKeys[CvChannels[i]] = i;
    }
  }
}

void CheckOutputTimers()
{
  for(int8_t i = 0; i < 6; i++){
    uint8_t TimeDifference = uint8_t(millis()) - Output_timers[i];
    if(Output_timers[i] > 0 && TimeDifference > 9){
      digitalWrite(LOGIC_OUTPUTS[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
      if ( i > 1 ) {
        if (MIDI_DEBUG) MIDI.sendNoteOff(i-1, 0, i-1);
      }
    }
  }
}

void erase_learn_counter()
{
//  if ( LearnMode && i == 0 && ) {
//    count
//  }
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
    bool main_conditions = cv_main_conditions(row, type, channel, note);
    //mono keys
    if ( main_conditions && !polyphony[channel][note] && !percussion[row] && ) ) {
      mono_note_on_hist[row][note] = HIGH;
      cv_out(row, note, scale);
      gate_out(row, HIGH);
      break;x
    }
    //poly keys
    if ( main_conditions && polyphony[channel][note] && !digitalRead(gate[row]) ) {
      cv_out(row, note, scale);
      gate_out(row, HIGH);
      break;
    }
    if ( main_conditions && polyphony[channel][note] && digitalRead(gate[row]) ) {
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
    bool main_conditions = cv_main_conditions(row, type, channel, note);
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

bool cv_main_conditions(int row, int type, int8_t channel, int8_t number)
{
  return cv_type[row] == type && cv_channel[row] == channel && cv_num_low[row] <= number && cv_num_high[row] >= number;
}

void learn_note_on(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  int8_t learn_loop_count = 0;

//  struct f {
//    static void nothing() {
//    }
//    static void learn_loop() {
//      int b= 3;
//    }
//  };
  
  for(int8_t i = 0; i < 4; i++){
//    void (*route[2])() = {&f::nothing, &f::learn_loop};
//    route[bitRead(ButtonStates, i)]();
    if(bitRead(ButtonStates, i)){
      if(learn_loop_count == 0){
        if(learn_NoteNumbers_count == 0){ // Monophonic keys.
          CvChannels[i] = Channel;
          CvStates[i] = KEYS;
          GateStates[i] = KEYS;
        } else if(learn_NoteNumbers_count == 1){ // Drum Trigger and VelocityValue
          GateChannels[i] = Channel;
          GateNotes[i] = NoteNumber;
          GateStates[i] = PERCTRIGGER;
          CvChannels[i] = Channel;
          CvStates[i] = PERCVELOCITY;
        } else if(learn_NoteNumbers_count == 2){ // Drum gate and aftertouch
          GateStates[i] = PERCGATE;
          bitSet(MayProgramAftertouch, i);
          DrumGateNoteAfter[i] = NoteNumber;
        }
      } else if(learn_loop_count == 1){
        if(learn_NoteNumbers_count == 0) { // Monophonic keys + [VelocityValue]
          CvChannels[i] = Channel;
          CvStates[i] = VELOCITY;
        }
      } else if(learn_loop_count == 2){
        if(learn_NoteNumbers_count == 0){ // Monophonic keys + VelocityValue + [poly/Channel aftertouch]
          bitSet(MayProgramAftertouch, i);
        }
      }
      learn_loop_count++;
    }
  }
  learn_NoteNumbers_count++;
}

void Check_and_PlayNotes(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  MidiValue[KEYS] = NoteNumber;
  MidiValue[VELOCITY] = VelocityValue;
  MidiValue[AFTERTOUCH] = NoteNumber;

  MsbNoteLsbVel = NoteNumber << 8 | VelocityValue;
  
  MayPlayOne[KEYS] = true;
  
  for(int8_t i = 0; i < 4; i++){
    play_drums(i, Channel, NoteNumber, VelocityValue);
    play_cv(i, Channel, NoteNumber, VelocityValue);
//    MayPlayCvatRow[i] = false;
  }
  MayPlayOne[VELOCITY] = false;
  MayPlayOne[AFTERTOUCH] = false;
}

void play_drums(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  bool GateChanMatch = GateChannels[i] == Channel;
  bool CvChanMatch = CvChannels[i] == Channel;
  bool ChanNoteMatch = GateNotes[i] == NoteNumber && GateChanMatch;
  bool TrigMatch = GateStates[i] == PERCTRIGGER && ChanNoteMatch;
  bool GateMatch = GateStates[i] == PERCGATE && ChanNoteMatch;
  bool VelChanMatch = CvChanMatch && CvStates[i] == PERCVELOCITY;
  bool AfterChanMatch = CvChanMatch && CvStates[i] == PERCAFTERTOUCH;
  
  if ( TrigMatch ) {
    digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
    if (MIDI_DEBUG) MIDI.sendNoteOn(i+1, 1, i+1);
    Output_timers[i+2] = millis() == 0 ? -1 : millis();
//    MayPlayCvatRow[i] = true;
    if ( VelChanMatch ) {
      pwm_out(i, VelocityValue + 64);
//      MIDI.sendNoteOn(i, VelocityValue, i);
    }
  } else if ( GateMatch ) {
    digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
    if (MIDI_DEBUG) MIDI.sendNoteOn(i+1, 1, i+1);
    Output_timers[i+2] = millis() == 0 ? -1 : millis();
    if ( AfterChanMatch ) {
      AftertouchNoteNumber[i] = NoteNumber;
    }
//    MayPlayCvatRow[i] = true;
  }
}

void play_cv(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  
  if ( CvChannels[i] == Channel && VoiceTypeKeys[Channel] == MONOVOICE ) {
    play_mono_cv(i, Channel, NoteNumber, VelocityValue);
  } else if ( CvChannels[i] == Channel && VoiceTypeKeys[Channel] > MONOVOICE ) {
    play_poly_cv(i, Channel, NoteNumber, VelocityValue);
  }
}

void play_mono_cv(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  switch ( CvStates[i] ) {
    case KEYS:
      for (int8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
        if (NoteNumbers[i][n] == NOTHING) {
          NoteNumbers[i][n] = MsbNoteLsbVel;
          break;
        }
      }
      pwm_out(i, NoteNumber + 128);
  //    MIDI.sendNoteOn(i, 0, i);
      CheckGateState(KEYS, i, HIGH);
      MayPlayOne[VELOCITY] = true;
      MayPlayOne[AFTERTOUCH] = true;
      break;
    case VELOCITY:
      CheckVelocity(i, VelocityValue);
      break;
    case AFTERTOUCH:
      CheckAftertouch(i, NoteNumber);
      break;
    default: break;
  }
}

void play_poly_cv(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  switch ( CvStates[i] ) {
    case KEYS:
      if ( MayPlayOne[KEYS] ) {
        if ( CurrentNotePlayed[i] == NOTHING ) {
          CurrentNotePlayed[i] = NoteNumber;
          pwm_out(i, CurrentNotePlayed[i] + 128);
    //      MIDI.sendNoteOn(i, 0, i);
          CheckGateState(KEYS, i, HIGH);
          MayPlayOne[KEYS] = false;
          MayPlayOne[VELOCITY] = true;
          MayPlayOne[AFTERTOUCH] = true;
        } else if ( i == LastRowKeys[Channel] ) {
          FillPolyValues(KEYS, Channel, MsbNoteLsbVel);
        } else {
        }
      }
      break;
    case VELOCITY:
      if ( CurrentVelocityPlayed[i] == NOTHING ) {
        CheckVelocity(i, VelocityValue);
      }
      break;
    case AFTERTOUCH:
      if ( CurrentAftertouchPlayed[i] == NOTHING ) {
        CheckAftertouch(i, NoteNumber);
      }
      break;
    default: break;
  }
}

//bool CheckVoiceTypeKeys(int8_t Channel)
//{
//  
//}

void RecievedNoteOff(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  if ( LearnMode ) {
    return;
  } else CheckNoteoff(Channel, NoteNumber, VelocityValue);
}

void CheckNoteoff(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  MidiValue[KEYS] = NoteNumber;
  MidiValue[VELOCITY] = VelocityValue;
  MidiValue[AFTERTOUCH] = NoteNumber;

  MsbNoteLsbVel = NoteNumber << 8 | VelocityValue;

  MayPlayOne[KEYS] = true;
  for (int8_t i = 0; i < 4; i++) {
    if (GateChannels[i] == Channel
    && GateStates[i] == PERCGATE
    && GateNotes[i] == NoteNumber) {
      digitalWrite(LOGIC_OUTPUTS[i+2], LOW);
      if (MIDI_DEBUG) MIDI.sendNoteOff(i+1, 0, i+1);
    }
    if ( CvChannels[i] == Channel ) {
      if ( CvStates[i] == KEYS ) {
        if ( VoiceTypeKeys[Channel] > MONOVOICE && MayPlayOne[KEYS] ) {
          if ( CurrentNotePlayed[i] == NoteNumber ) {
            if ( int8_t(highByte(PolyNotes[Channel][0])) == NOTHING ) {
              CheckGateState(KEYS, i, LOW);
              CurrentNotePlayed[i] = NOTHING;
            } else {
              CurrentNotePlayed[i] = int8_t(highByte(PolyNotes[Channel][0]));
              pwm_out(i, CurrentNotePlayed[i] + 128);
//              MIDI.sendNoteOn(i, 0, i);
              MayPlayOne[KEYS] = false;
              MidiValue[VELOCITY] = int8_t(lowByte(PolyNotes[Channel][0]));
              MayPlayOne[VELOCITY] = true;
              MidiValue[AFTERTOUCH] = CurrentNotePlayed[i];
              MayPlayOne[AFTERTOUCH] = true;
              ShiftPolyValues(KEYS, Channel, 0);
            }
          } else if ( i == LastRowKeys[Channel] ) {
            SearchInPolyValues(KEYS, Channel, highByte(MsbNoteLsbVel));
          }
        } else if ( VoiceTypeKeys[Channel] == MONOVOICE ) {
          int16_t LastNote = NOTHING;
          for (int8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
            if (highByte(NoteNumbers[i][n]) == NoteNumber
            || NoteNumbers[i][n] == NOTHING) {
              if (n < SizeofRow_NoteNumbers - 1) {             
                if(NoteNumbers[i][n + 1] != NOTHING){  // Shifts all NoteNumbers to the left where the NoteNumber was removed.
                  NoteNumbers[i][n] = NoteNumbers[i][n + 1];
                  NoteNumbers[i][n + 1] = NOTHING;
                } else if ( NoteNumbers[i][n + 1] == NOTHING ){
                  NoteNumbers[i][n] = NOTHING;
                  if(n == 0) {
                    LastNote = NOTHING; // If the current NoteNumber is the first in the array. Then there are no NoteNumbers on.
                  } else {
                    LastNote = NoteNumbers[i][n - 1]; //  When the next NoteNumber in the array is also NOTHING, the last NoteNumber will be the NoteNumber before the current one.
                  }
                  break;
                }
              } else if(n == SizeofRow_NoteNumbers - 1){ // If the NoteNumber on place 19 in the array is the OFF NoteNumber. Then this will be removed and the last NoteNumber will be the NoteNumber before this.
                NoteNumbers[i][n] = NOTHING;
                LastNote = NoteNumbers[i][n - 1];
              }
            }
          }
          if ( LastNote == NOTHING ) {
            CheckGateState(KEYS, i, LOW);
          } else {
            pwm_out(i, highByte(LastNote) + 128);
//            MIDI.sendNoteOn(i, 0, i);
            MidiValue[VELOCITY] = int8_t(lowByte(LastNote));
            MayPlayOne[VELOCITY] = true;
            MidiValue[AFTERTOUCH] = int8_t(highByte(LastNote));
            MayPlayOne[AFTERTOUCH] = true;
          }
        }
      } else {
        CheckCvStates(i);
      }
    }
  }
  MayPlayOne[VELOCITY] = false;
  MayPlayOne[AFTERTOUCH] = false;
}

void CheckCvStates(int8_t i)
{
  switch (CvStates[i]) {
    case VELOCITY: CheckVelocity(i, MidiValue[VELOCITY]); break;
    case AFTERTOUCH: CheckAftertouch(i, MidiValue[AFTERTOUCH]); break;
    default: break;
  }
}

void CheckVelocity(int8_t i, int8_t Value)
{
  if ( MayPlayOne[VELOCITY] ) {
    CurrentVelocityPlayed[i] = Value;
    pwm_out(i, Value + 64);
//    MIDI.sendNoteOn(i, Value, i);
    MayPlayOne[VELOCITY] = false;
  } else if ( CurrentVelocityPlayed[i] == Value ) {
    CurrentVelocityPlayed[i] = NOTHING;
  }
}

void CheckAftertouch(int8_t i, int8_t NoteNumber)
{
  if ( MayPlayOne[AFTERTOUCH] ) {
    CurrentAftertouchPlayed[i] = NoteNumber;
    AftertouchNoteNumber[i] = NoteNumber;
    MayPlayOne[AFTERTOUCH] = false;
  } else if ( CurrentAftertouchPlayed[i] == NoteNumber ) {
    CurrentAftertouchPlayed[i] = NOTHING;
  }
}

void FillPolyValues(int8_t CvState, int8_t Channel, int16_t Value)
{
  for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
    if ( PolyNotes[Channel][n] == NOTHING ) {
      PolyNotes[Channel][n] = Value;
      break;
    }
  }
}

void CheckGateState(int8_t CvState, int8_t i, bool Status)
{
  if ( GateStates[i] == CvState ) {
    digitalWrite(LOGIC_OUTPUTS[i+2], Status);
    if ( Status ) {
      if (MIDI_DEBUG) MIDI.sendNoteOn(i+1, 1, i+1);
    } else {
      if (MIDI_DEBUG) MIDI.sendNoteOff(i+1, 0, i+1);
    }
  }
}

void SearchInPolyValues(int8_t CvState, int8_t Channel, int8_t Value)
{
  for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
    if ( highByte(PolyNotes[Channel][n]) == Value ) {
      ShiftPolyValues(CvState, Channel, n);
      break;
    }
  }
}

void ShiftPolyValues(int8_t CvState, int8_t Channel, int8_t n)
{
  for ( n; n < SizeofRow_PolyNotes; n++ ) {
    if ( n == SizeofRow_PolyNotes - 1 ) {
      PolyNotes[Channel][n] = NOTHING;
    } else if ( PolyNotes[Channel][n + 1] == NOTHING ) {
      PolyNotes[Channel][n] = NOTHING;
      break;
    } else {
      PolyNotes[Channel][n] = PolyNotes[Channel][n + 1];
    }
  }
}

void RecievedChannelPressure(int8_t Channel, int8_t PressureValue)
{
  if ( LearnMode ) {
    for(int8_t i = 0; i < 4; i++){
      if ( bitRead(ButtonStates, i)
      && bitRead(MayProgramAftertouch, i) ) {
        CvChannels[i] = Channel;
        CvStates[i] = AFTERTOUCH;
      }
    }
  } else {
    for(int8_t i = 0; i < 4; i++){
      if(CvChannels[i] == Channel
      && CvStates[i] == AFTERTOUCH){
        pwm_out(i, PressureValue + 128);
//        MIDI.sendAfterTouch(PressureValue, Channel);
      }
    }
  }
}

void RecievedNotePressure(int8_t Channel, int8_t NoteNumber, int8_t PressureValue)
{
  for (int8_t i = 0; i < 4; i++) {
    if (bitRead(ButtonStates, i)
    && bitRead(MayProgramAftertouch, i)) {
      CvChannels[i] = Channel;
      CvStates[i] = AFTERTOUCH;
      if ( GateStates[i] == PERCGATE ) {
        CvStates[i] = PERCAFTERTOUCH;
      }
    }
    if ( CvChannels[i] == Channel ) {
      // Algoritme: 'speel hoogste waarde':
        // als NoteNumber voorkomt in poly lijst ga verder:
          // als PressureValue hoger is dan vorige value: Output. En sla NoteNumber op als 'laatstGespeeld'
          // als PressureValue lager is dan vorige value: Speel alleen als de NoteNumber van de laatst gespeelde PressureValue hetzelfde is als de ontvangen NoteNumber.
      
      pwm_out(i, PressureValue + 128);
//      MIDI.sendAfterTouch(NoteNumber, PressureValue, Channel);
    }
  }
}

int8_t Nrpn_Msb_chans[4];
int8_t Nrpn_Msb_nums[4];
int8_t Nrpn_Msb_vals[4];

int8_t Nrpn_Lsb_chans[4];
int8_t Nrpn_Lsb_nums[4];
int8_t Nrpn_Lsb_vals[4];

int8_t LowRes_Msb_Nums[4];

int8_t HiRes_Lsb_Chans[4];
int8_t HiRes_Lsb_Nums[4];

void RecievedCC(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  switch (LearnMode) {
    case 0: play_cc(Channel, CcNumber, CcValue); break;
    case 1: learn_cc_states(Channel, CcNumber, CcValue); break;
    default: break;
  }
}

void learn_cc_states(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  static bool Watch14bitLsb;
  for (int8_t i = 0; i < 4; i++) {
    if (bitRead(ButtonStates, i)) {
      static bool MsbState[4] = { false, false, false, false };
      static bool LsbState[4] = { false, false, false, false };
      if ( CcNumber == 99 ) {
        Nrpn_Msb_chans[i] = Channel;
        Nrpn_Msb_nums[i] = CcNumber;
        Nrpn_Msb_vals[i] = CcValue;
        MsbState[i] = true;
      } else if ( CcNumber == 98 && MsbState[i] ) {
        MsbState[i] = false;
        Nrpn_Lsb_chans[i] = Channel;
        Nrpn_Lsb_nums[i] = CcNumber;
        Nrpn_Lsb_vals[i] = CcValue;
        LsbState[i] = true;
      } else if ( CcNumber < 32 ) {
        Watch14bitLsb = true;
        CvChannels[i] = Channel;
        LowRes_Msb_Nums[i] = CcNumber;
        if ( CcNumber == 6 && LsbState[i] ) {
          LsbState[i] = false;
          CvStates[i] = CCNRPN;
        } else CvStates[i] = CCNORMAL;
      } else if ( CcNumber == LowRes_Msb_Nums[i] + 32 && Watch14bitLsb ) {
        Watch14bitLsb = false;
        HiRes_Lsb_Chans[i] = Channel;
        HiRes_Lsb_Nums[i] = CcNumber;
        CvStates[i] = CcNumber == 38 ? CCNRPN14BIT : CC14BIT;
      } else if ( CcNumber > 63 ) {
        Watch14bitLsb = false;
        CvChannels[i] = Channel;
        LowRes_Msb_Nums[i] = CcNumber;
        CvStates[i] = CCNORMAL;
      }
    }
  }
}

void play_cc(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  for (int8_t i = 0; i < 4; i++) {
    if ( CvStates[i] == CCNORMAL ) {
      play_cc_normal(i, Channel, CcNumber, CcValue);
    } else if ( CvStates[i] == CC14BIT ) {
      play_cc_14bit(i, Channel, CcNumber, CcValue);
    } else if ( CvStates[i] == CCNRPN ) {
      play_cc_nrpn(i, Channel, CcNumber, CcValue);
    } else if ( CvStates[i] == CCNRPN14BIT ) {
      play_cc_nrpn_14bit(i, Channel, CcNumber, CcValue);
    }
  }
}

void play_cc_normal(int8_t i, int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  if ( CvChannels[i] == Channel && LowRes_Msb_Nums[i] == CcNumber ) {
    pwm_out(i, CcValue + 64);
//    MIDI.sendControlChange(CcNumber, CcValue, Channel);
  }
}

void play_cc_14bit(int8_t i, int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  static uint16_t Cc14bitMsb[4];
  static bool MsbState14bit[4] = { false, false, false, false };
  if ( CvChannels[i] == Channel && LowRes_Msb_Nums[i] == CcNumber ) {
    Cc14bitMsb[i] = CcValue << 7;
    MsbState14bit[i] = true;
//    MIDI.sendControlChange(CcNumber, CcValue, Channel);
  } else if ( HiRes_Lsb_Chans[i] == Channel && HiRes_Lsb_Nums[i] == CcNumber && MsbState14bit[i] ) {
    MsbState14bit[i] = false;
    pwm_out(i, (Cc14bitMsb[i] + CcValue) >> 6);
//    MIDI.sendControlChange(CcNumber, CcValue, Channel);
  }
}

void play_cc_nrpn(int8_t i, int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  static bool MsbState[4] = { false, false, false, false };
  static bool LsbState[4] = { false, false, false, false };
  if ( CcNumber == 99 && Nrpn_Msb_chans[i] == Channel && Nrpn_Msb_nums[i] == CcNumber && Nrpn_Msb_vals[i] == CcValue ) {
    MsbState[i] = true;
//    MIDI.sendNrpnValue(CcValue, Channel);
  } else if ( CcNumber == 98 && MsbState[i] && Nrpn_Lsb_chans[i] == Channel && Nrpn_Lsb_nums[i] == CcNumber && Nrpn_Lsb_vals[i] == CcValue ) {
    MsbState[i] = false;
    LsbState[i] = true;
//    MIDI.sendNrpnValue(CcValue, Channel);
  } else if ( CcNumber == 6 && LsbState[i] && CvChannels[i] == Channel && LowRes_Msb_Nums[i] == CcNumber ) {
    LsbState[i] = false;
    pwm_out(i, CcValue + 64);
//    MIDI.sendControlChange(CcNumber, CcValue, Channel);
  }
}

void play_cc_nrpn_14bit(int8_t i, int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  static bool MsbState[4] = { false, false, false, false };
  static bool LsbState[4] = { false, false, false, false };
  static uint16_t Cc14bitMsb[4];
  static bool MsbState14bit[4] = { false, false, false, false };
  if ( CcNumber == 99 && Nrpn_Msb_chans[i] == Channel && Nrpn_Msb_nums[i] == CcNumber && Nrpn_Msb_vals[i] == CcValue ) {
    MsbState[i] = true;
//    MIDI.sendNrpnValue(CcValue, Channel);
  } else if ( CcNumber == 98 && MsbState[i] && Nrpn_Lsb_chans[i] == Channel && Nrpn_Lsb_nums[i] == CcNumber && Nrpn_Lsb_vals[i] == CcValue ) {
    MsbState[i] = false;
    LsbState[i] = true;
//    MIDI.sendNrpnValue(CcValue, Channel);
  } else if ( CcNumber == 6 && LsbState[i] && CvChannels[i] == Channel && LowRes_Msb_Nums[i] == CcNumber ) {
    LsbState[i] = false;
    MsbState14bit[i] = true;
    Cc14bitMsb[i] = CcValue << 7;
//    MIDI.sendControlChange(CcNumber, CcValue, Channel);
  } else if ( HiRes_Lsb_Chans[i] == Channel && HiRes_Lsb_Nums[i] == CcNumber && MsbState14bit[i] ) {
    MsbState14bit[i] = false;
    pwm_out(i, (Cc14bitMsb[i] + CcValue) >> 6);
//    MIDI.sendControlChange(CcNumber, Cc14bitMsb[i], i);
  }
}
  
void RecievedPitchBend(int8_t Channel, int Bend)
{
  if(Bend >= -8192 && Bend <= 8191){
    for(int8_t i = 0; i < 4; i++){
      if(bitRead(ButtonStates, i)){
        CvChannels[i] = Channel;
        CvStates[i] = PITCHBEND;
      }
      if ( CvChannels[i] == Channel && CvStates[i] == PITCHBEND ) {
        pwm_out(i, (Bend + 8192) >> 6); //Bend data is 14-bits. PWM is 8-bit. Dus bitshift van 6 naar rechts.
//        MIDI.sendPitchBend(Bend, i);
      }
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
  if (MIDI_DEBUG) MIDI.sendControlChange(1, pwmVal >> 7, i+1);
  if (MIDI_DEBUG) MIDI.sendControlChange(33, pwmVal & 127, i+1);
}

void RecievedClock(void)
{
//  if(clock_count == 0 && (clk_state == clk_start || clk_state == clk_continue)) digitalWrite(CLOCK, HIGH); // Start clock pulse
//  if(clk_state != clk_stop) clock_count++;
//  if(clock_count >= 3) {  
//    digitalWrite(CLOCK, LOW);
//    clock_count = 0;
//  }
  uint8_t clock_output = clock_count < 2 && (clk_state != clk_stop); // Clock pulse high only on clock pulse 0 and 1 and when clk_state is not clt_stop.
  digitalWrite(CLOCK, clock_output);
  if (MIDI_DEBUG) MIDI.sendRealTime(midi::Clock);
  clock_count = min(clock_count + 1, 2); // MIDI timing clock sends 24 pulses per quarter NoteNumber. Sent pulse only once every 3 pulses (32th NoteNumbers).
}

void RecievedStart(void)
{
  digitalWrite(RESET, HIGH);
  if (MIDI_DEBUG) MIDI.sendRealTime(midi::Start);
  Output_timers[1] = millis() == 0 ? -1 : millis();;
  clock_count = 0;
  clk_state = clk_start;
}

void RecievedContinue(void)
{
  if (MIDI_DEBUG) MIDI.sendRealTime(midi::Continue);
  clk_state = clk_continue;
}

void RecievedStop(void)
{
  clk_state = clk_stop;
  digitalWrite(CLOCK, LOW);
  if (MIDI_DEBUG) MIDI.sendRealTime(midi::Stop);
  for(int8_t i = 0; i < 4; i++){
    for(int8_t n = 0; n < SizeofRow_NoteNumbers; n++){ //scans for holded NoteNumber.
      NoteNumbers[i][n] = NOTHING;
    }
    if (MIDI_DEBUG) MIDI.sendNoteOff(i+1, 0, i+1);
    digitalWrite(LOGIC_OUTPUTS[i+2], LOW);
  }
}

void RecievedActive(void)
{
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
