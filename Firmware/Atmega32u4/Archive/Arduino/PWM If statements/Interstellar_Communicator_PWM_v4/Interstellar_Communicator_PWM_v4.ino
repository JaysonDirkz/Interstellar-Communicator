/*
1-5-19
Jason

TODO
- Aftertouch nog testen/maken.
- Erase program toevoegen. (2x snel drukken knop).

Testen
- Test clock.
- Test learn save voor alle learns (ook polyfoon keys en drums).

*/

#include <MIDI.h>
//#include <SoftwareSerial.h>
#include <EEPROM.h>

#define CLOCK 15
#define RESET 3
#define GT1  4
#define GT2  5
#define GT3  7
#define GT4  8
//#define DAC_1  8
//#define DAC_2  9
#define BUTTON_1 A0
#define BUTTON_2 A1
#define BUTTON_3 A2
#define BUTTON_4 A3

#define RX_SOFT 14
#define TX_SOFT 2
SoftwareSerial SoftSerial (RX_SOFT, TX_SOFT);

//Constants
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

//enum CV_TYPE {
//  KEYS,
//  VELOCITY,
//  AFTERTOUCH,
//  CONTROLCHANGE,
//  CC_14_BIT,
//  PITCHBEND,
//  PERVELOCITY,
//  PERCAFTERTOUCH,
//  PERCTRIGGER,
//  PERCGATE
//};

const int8_t KEYS = 0;
const int8_t VELOCITY = 1;
const int8_t AFTERTOUCH = 2;
const int8_t CONTROLCHANGE = 3;
const int8_t CC_14_BIT = 4;
const int8_t PITCHBEND = 5;
const int8_t PERCVELOCITY = 6;
const int8_t PERCAFTERTOUCH = 7;
const int8_t PERCTRIGGER = 8;
const int8_t PERCGATE = 9;

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
  SoftSerial.begin(115200);
  
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
  MIDI.setHandleNoteOn(RecievedNoteOn);
  MIDI.setHandleNoteOff(RecievedNoteOff);
  MIDI.setHandleControlChange(RecievedCC);
  MIDI.setHandlePitchBend(RecievedPitchBend);
  MIDI.setHandleAfterTouchChannel(RecievedChannelPressure);
  MIDI.setHandleAfterTouchPoly(RecievedNotePressure);
  MIDI.setHandleClock(RecievedClock);
  MIDI.setHandleStart(RecievedStart);
  MIDI.setHandleContinue(RecievedContinue);
  MIDI.setHandleStop(RecievedStop);
  MIDI.setHandleActiveSensing(RecievedActive);
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
//    Serial.println("MidiRec");
  }
}

void CheckMidiTimer()
{
  int16_t TimeDifference = millis() - MidiTimer;
  if ( TimeDifference > 500 ) {
    SoftSerial.println("MidiLate");
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
      SoftSerial.println("VoiceType:"+String(VoiceTypeKeys[CvChannels[i]])+" LastRow:"+String(LastRowKeys[CvChannels[i]]));
    }
  }
}

void CheckOutputTimers()
{
  for(int8_t i = 0; i < 6; i++){
    uint8_t TimeDifference = uint8_t(millis()) - Output_timers[i];
    if(Output_timers[i] > 0 && TimeDifference >= 10){
      SoftSerial.println(TimeDifference);
      digitalWrite(LOGIC_OUTPUTS[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
    }
  }
}

void erase_learn_counter()
{
//  if ( LearnMode && i == 0 && ) {
//    count
//  }
}

void RecievedNoteOn(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
  typedef void (*f)(int8_t, int8_t, int8_t);
  f route[2] = {&LearnNoteOn, &Check_and_PlayNotes};
  route[LearnMode](Channel, NoteNumber, VelocityValue);
}

void LearnNoteOn(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  int8_t learn_loop_count = 0;

  struct f {
    static void nothing() {
    }
    static void learn_loop() {
      int b= 3;
    }
  };
  
  for(int8_t i = 0; i < 4; i++){
    void (*route[2])() = {&f::nothing, &f::learn_loop};
    route[bitRead(ButtonStates, i)]();
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
  SoftSerial.println("Ch:"+String(Channel)+" On:"+String(NoteNumber)+" Vel:"+String(VelocityValue));
  MidiValue[KEYS] = NoteNumber;
  MidiValue[VELOCITY] = VelocityValue;
  MidiValue[AFTERTOUCH] = NoteNumber;

  MsbNoteLsbVel = NoteNumber << 8 | VelocityValue;
//  SoftSerial.print(String(highByte(MsbNoteLsbVel))+" "+String(lowByte(MsbNoteLsbVel)));
  
  MayPlayOne[KEYS] = true;
  
  for(int8_t i = 0; i < 4; i++){
    play_drums(i, Channel, NoteNumber, VelocityValue);
    play_cv(i, Channel, NoteNumber, VelocityValue);
//    MayPlayCvatRow[i] = false;
  }
  MayPlayOne[VELOCITY] = false;
  MayPlayOne[AFTERTOUCH] = false;
  SoftSerial.println();
}

void play_drums(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  bool ChanMatch = GateChannels[i] == Channel;
  bool ChanNoteMatch = GateNotes[i] == NoteNumber && ChanMatch;
  bool TrigMatch = GateStates[i] == PERCTRIGGER && ChanNoteMatch;
  bool GateMatch = GateStates[i] == PERCGATE && ChanNoteMatch;
  
  if ( TrigMatch ) {
    SoftSerial.println(GateNotes[i]);
    digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
    Output_timers[i+2] = millis() == 0 ? -1 : millis();
    SoftSerial.println(Output_timers[i+2]);
//    MayPlayCvatRow[i] = true;
    
    if ( CvChannels[i] == Channel && CvStates[i] == PERCVELOCITY ) {
      pwm_out(i, VelocityValue + 64);
    }
  } else if ( GateMatch ) {
    digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);

    if ( CvChannels[i] == Channel && CvStates[i] == PERCAFTERTOUCH ) {
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
  if ( CvStates[i] == KEYS ) {
    for (int8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
      if (NoteNumbers[i][n] == NOTHING) {
        NoteNumbers[i][n] = MsbNoteLsbVel;
        break;
      }
    }
    SoftSerial.print("Play"+String(KEYS)+":"+String(NoteNumber));
    pwm_out(i, NoteNumber + 128);
    CheckGateState(KEYS, i, HIGH);
    MayPlayOne[VELOCITY] = true;
    MayPlayOne[AFTERTOUCH] = true;
  } else if ( CvStates[i] == VELOCITY ) {
    CheckVelocity(i, VelocityValue);
  } else if ( CvStates[i] == AFTERTOUCH ) {
    CheckAftertouch(i, NoteNumber);
  }
}

void play_poly_cv(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  if ( CvStates[i] == KEYS && MayPlayOne[KEYS] ) {
    if ( CurrentNotePlayed[i] == NOTHING ) {
      CurrentNotePlayed[i] = NoteNumber;
      SoftSerial.print("Play"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
      pwm_out(i, CurrentNotePlayed[i] + 128);
      CheckGateState(KEYS, i, HIGH);
      MayPlayOne[KEYS] = false;
      MayPlayOne[VELOCITY] = true;
      MayPlayOne[AFTERTOUCH] = true;
    } else if ( i == LastRowKeys[Channel] ) {
      SoftSerial.print("Cur"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
      FillPolyValues(KEYS, Channel, MsbNoteLsbVel);
    } else {
      SoftSerial.print("Cur"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
    }
  } else if ( CvStates[i] == KEYS ) {
    SoftSerial.print("Mp"+String(KEYS)+":"+String(MayPlayOne[KEYS]));
  } else if ( CvStates[i] == VELOCITY && CurrentVelocityPlayed[i] == NOTHING ) {
    CheckVelocity(i, VelocityValue);
  } else if ( CvStates[i] == AFTERTOUCH && CurrentAftertouchPlayed[i] == NOTHING ) {
    CheckAftertouch(i, NoteNumber);
  }
}

//bool CheckVoiceTypeKeys(int8_t Channel)
//{
//  
//}

void RecievedNoteOff(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  if ( !LearnMode ) {
    CheckNoteoff(Channel, NoteNumber, VelocityValue);
  }
}

void CheckNoteoff(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  SoftSerial.println("Ch:"+String(Channel)+" Off:"+String(NoteNumber)+" Vel:"+String(VelocityValue));
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
    }
    if ( CvChannels[i] == Channel ) {
      SoftSerial.print(String(i)+" ");
      if ( CvStates[i] == KEYS ) {
        if ( VoiceTypeKeys[Channel] > MONOVOICE && MayPlayOne[KEYS] ) {
          if ( CurrentNotePlayed[i] == NoteNumber ) {
//            SoftSerial.println("Value:"+String(highByte(PolyNotes[Channel][0])));
            if ( int8_t(highByte(PolyNotes[Channel][0])) == NOTHING ) {
  //            SoftSerial.print("CurNote:"+String(CurrentNotePlayed[i]));
              CheckGateState(KEYS, i, LOW);
              CurrentNotePlayed[i] = NOTHING;
            } else {
              CurrentNotePlayed[i] = int8_t(highByte(PolyNotes[Channel][0]));
              SoftSerial.print("Play"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
              pwm_out(i, CurrentNotePlayed[i] + 128);
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
          SoftSerial.print("List:");
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
            SoftSerial.print(String(highByte(NoteNumbers[i][n]))+" ");
          }
          if ( LastNote == NOTHING ) {
            CheckGateState(KEYS, i, LOW);
          } else {
            SoftSerial.print("Play"+String(KEYS)+":"+String(highByte(LastNote)));
            pwm_out(i, highByte(LastNote) + 128);
            MidiValue[VELOCITY] = int8_t(lowByte(LastNote));
            MayPlayOne[VELOCITY] = true;
            MidiValue[AFTERTOUCH] = int8_t(highByte(LastNote));
            MayPlayOne[AFTERTOUCH] = true;
          }
        }
      } else {
        CheckCvStates(i);
      }
      SoftSerial.println();
    }
  }
  MayPlayOne[VELOCITY] = false;
  MayPlayOne[AFTERTOUCH] = false;
  SoftSerial.println();
}

void CheckCvStates(int8_t i)
{
  if ( CvStates[i] == VELOCITY ) {
    CheckVelocity(i, MidiValue[VELOCITY]);
  } else if ( CvStates[i] == AFTERTOUCH ) {
    CheckAftertouch(i, MidiValue[AFTERTOUCH]);
  }
}

void CheckVelocity(int8_t i, int8_t Value)
{
  if ( MayPlayOne[VELOCITY] ) {
    CurrentVelocityPlayed[i] = Value;
    SoftSerial.print("PlayVEL:"+String(Value));
    pwm_out(i, Value + 64);
    MayPlayOne[VELOCITY] = false;
  } else if ( CurrentVelocityPlayed[i] == Value ) {
    CurrentVelocityPlayed[i] = NOTHING;
  }
}

void CheckAftertouch(int8_t i, int8_t NoteNumber)
{
  if ( MayPlayOne[AFTERTOUCH] ) {
    CurrentAftertouchPlayed[i] = NoteNumber;
    SoftSerial.print("PlayAT:"+String(NoteNumber));
    AftertouchNoteNumber[i] = NoteNumber;
    MayPlayOne[AFTERTOUCH] = false;
  } else if ( CurrentAftertouchPlayed[i] == NoteNumber ) {
    CurrentAftertouchPlayed[i] = NOTHING;
  }
}

void FillPolyValues(int8_t CvState, int8_t Channel, int16_t Value)
{
  SoftSerial.print(" FillPoly"+String(CvState)+":");
  for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
    if ( PolyNotes[Channel][n] == NOTHING ) {
      PolyNotes[Channel][n] = Value;
      SoftSerial.print(String(highByte(PolyNotes[Channel][n]))+" ");
      break;
    }
    SoftSerial.print(String(highByte(PolyNotes[Channel][n]))+" ");
  }
}

void CheckGateState(int8_t CvState, int8_t i, bool Set)
{
  if ( GateStates[i] == CvState ) {
    digitalWrite(LOGIC_OUTPUTS[i+2], Set);
  }
}

void SearchInPolyValues(int8_t CvState, int8_t Channel, int8_t Value)
{
//  SoftSerial.print("Search");
  for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
//    SoftSerial.print(" "+String(PolyNotes[Channel][n]));
    if ( highByte(PolyNotes[Channel][n]) == Value ) {
      ShiftPolyValues(CvState, Channel, n);
      break;
    }
  }
}

void ShiftPolyValues(int8_t CvState, int8_t Channel, int8_t n)
{
  SoftSerial.print(" ShiftPoly"+String(CvState)+":");
  for ( n; n < SizeofRow_PolyNotes; n++ ) {
    if ( n == SizeofRow_PolyNotes - 1 ) {
      PolyNotes[Channel][n] = NOTHING;
    } else if ( PolyNotes[Channel][n + 1] == NOTHING ) {
      PolyNotes[Channel][n] = NOTHING;
      SoftSerial.print(String(PolyNotes[Channel][n])+" ");
      break;
    } else {
      PolyNotes[Channel][n] = PolyNotes[Channel][n + 1];
    }
    SoftSerial.print(String(highByte(PolyNotes[Channel][n]))+" ");
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
        SoftSerial.println(String(i)+" Ch:"+String(Channel)+" Val:"+String(PressureValue));
        SoftSerial.flush();
        pwm_out(i, PressureValue + 128);
      }
    }
  }
}

void RecievedNotePressure(int8_t Channel, int8_t NoteNumber, int8_t PressureValue)
{
//  SoftSerial.println("Ch:"+String(Channel)+" Note:"+String(NoteNumber)+" PNAT:"+String(AftertouchNoteNumber[2]));
  for (int8_t i = 0; i < 4; i++) {
    if (bitRead(ButtonStates, i)
    && bitRead(MayProgramAftertouch, i)) {
      CvChannels[i] = Channel;
      CvStates[i] = AFTERTOUCH;
      if ( GateStates[i] == PERCGATE ) {
        CvStates[i] = PERCAFTERTOUCH;
      }
    }
    if ( CvChannels[i] == Channel
    && AftertouchNoteNumber[i] == NoteNumber ) {
      SoftSerial.println(String(i)+" Ch:"+String(Channel)+" Note:"+String(NoteNumber)+" Val:"+String(PressureValue));
      SoftSerial.flush();
      pwm_out(i, PressureValue + 128);
    }
  }
}

int8_t Nrpn_Msb_chans[4];
int8_t Nrpn_Msb_nums[4];
int8_t Nrpn_Msb_vals[4];

int8_t Nrpn_Lsb_chans[4];
int8_t Nrpn_Lsb_nums[4];
int8_t Nrpn_Lsb_vals[4];

int8_t CvNumbers[4];

void RecievedCC(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  //Define type which points a function.
  typedef void (*f)(int8_t, int8_t, int8_t);
  
  //Create array of function adresses.
  f route_3_to_1[2] {&play_cc, &learn_cc_states};

  //Pass data to array and routes to one of the two functions dependant on accessor value (boolean in this case).
  route_3_to_1[LearnMode](Channel, CcNumber, CcValue);
}

void learn_cc_states(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
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
      } else if ( CcNumber == 6 && LsbState[i] ) {
        LsbState[i] = false;
        goto defaultLearn;
      } else if ( CcNumber != 6 ) {
        defaultLearn:
        CvChannels[i] = Channel;
        CvNumbers[i] = CcNumber;
        CvStates[i] = CONTROLCHANGE;
      }
    }
  }
}

void play_cc(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  for (int8_t i = 0; i < 4; i++) {
    static bool MsbState[4] = { false, false, false, false };
    static bool LsbState[4] = { false, false, false, false };
    if ( CcNumber == 99 && Nrpn_Msb_chans[i] == Channel && Nrpn_Msb_nums[i] == CcNumber && Nrpn_Msb_vals[i] == CcValue ) {
      MsbState[i] = true;
    } else if ( CcNumber == 98 && MsbState[i] && Nrpn_Lsb_chans[i] == Channel && Nrpn_Lsb_nums[i] == CcNumber && Nrpn_Lsb_vals[i] == CcValue ) {
      SoftSerial.println(Nrpn_Lsb_vals[i]);
      MsbState[i] = false;
      LsbState[i] = true;
    } else if ( CcNumber == 6 && LsbState[i] && CvChannels[i] == Channel && CvNumbers[i] == CcNumber && CvStates[i] == CONTROLCHANGE ) {
      SoftSerial.println(LsbState[i]);
      LsbState[i] = false;
      pwm_out(i, CcValue + 64);
    } else if ( CcNumber != 6 && CvChannels[i] == Channel && CvNumbers[i] == CcNumber && CvStates[i] == CONTROLCHANGE ) {
      pwm_out(i, CcValue + 64);
    }
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
        pwm_out(i, (Bend + 8192) >> 4); //Bend data is 14-bits. PWM is 8-bit. Dus bitshift van 4 naar rechts.
      }
    }
  }
}

inline void pwm_out(uint8_t i, uint8_t pwmVal)
{
  typedef void (*f)(uint8_t);
  f router[4] = {&pwm_out_6, &pwm_out_9, &pwm_out_11, &pwm_out_13};
  router[i](pwmVal);
}
  inline void pwm_out_6(uint8_t pwmVal) {PWM6 = pwmVal;}
  inline void pwm_out_9(uint8_t pwmVal) {PWM9 = pwmVal;}
  inline void pwm_out_11(uint8_t pwmVal) {PWM10 = pwmVal;}
  inline void pwm_out_13(uint8_t pwmVal) {PWM13 = pwmVal;}

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
  clock_count = (clock_count + 1) % 3; // MIDI timing clock sends 24 pulses per quarter NoteNumber. Sent pulse only once every 3 pulses (32th NoteNumbers).
}

void RecievedStart(void)
{
  digitalWrite(RESET, HIGH);
  Output_timers[1] = millis() == 0 ? -1 : millis();;
  clock_count = 0;
  clk_state = clk_start;
}

void RecievedContinue(void)
{
  clk_state = clk_continue;
}

void RecievedStop(void)
{
  clk_state = clk_stop;
  digitalWrite(CLOCK, LOW);
  for(int8_t i = 0; i < 4; i++){
    for(int8_t n = 0; n < SizeofRow_NoteNumbers; n++){ //scans for holded NoteNumber.
      NoteNumbers[i][n] = NOTHING;
    }
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
    
    CvNumbers[i] = EEPROM.read(i+52);
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
    
    EEPROM.update(i+52, CvNumbers[i]);
  }
}
