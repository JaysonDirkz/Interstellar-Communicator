/*
1-5-19
Jason

Aftertouch nog testen.

Erase program toevoegen. (2x snel drukken knop).

Update naar eeprom bij elke alle knoppen los registratie.
*/

#include <MIDI.h>

#define CLOCK 2
#define RESET 3
#define GT1  4
#define GT2  5
#define GT3  6
#define GT4  7
//#define DAC_1  8
//#define DAC_2  9
#define BUTTON_1 A0
#define BUTTON_2 A1
#define BUTTON_3 A2
#define BUTTON_4 A3

//Constants
const int8_t LOGIC_OUTPUTS[6] = {CLOCK, RESET, GT1, GT2, GT3, GT4};

//const int8_t DAC_CHAN_0 = 0x10;
//const int8_t DAC_CHAN_1 = 0x90;
//const int8_t DAC_PIN_CHAN[4][2] = {
//  {DAC_1, DAC_CHAN_0},
//  {DAC_1, DAC_CHAN_1},
//  {DAC_2, DAC_CHAN_0},
//  {DAC_2, DAC_CHAN_1}
//};

//const int8_t HALFGAIN = 0x20;
//const int8_t FULLGAIN = 0;
const int8_t HALFGAIN = 1;
const int8_t FULLGAIN = 2;
int8_t DAC_GAIN[10];

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
const int8_t PITCHBEND = 4;
const int8_t PERCVELOCITY = 5;
const int8_t PERCAFTERTOUCH = 6;
const int8_t PERCTRIGGER = 7;
const int8_t PERCGATE = 8;

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
bool MayPlayCvatRow[4];
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
int8_t timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
int8_t Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

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
  confpwm();
  configuremidisethandle();
//  ConfigureDAC_GAIN();
}

void setinputsandoutputs()
{
  MIDI.begin(MIDI_CHANNEL_OMNI);
  
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

  //Sets the DAC outputs on half of the max output, which translates to 0V after the amplifier stage. With Note messages the mid point will be note C3 = midi note 67 = 67*32 = 2144V.
  PWM6 = 0;
  PWM9 = 100;
  PWM10 = 200;
  PWM13 = 255;
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

void ConfigureDAC_GAIN()
{
  DAC_GAIN[KEYS] = FULLGAIN;
  DAC_GAIN[VELOCITY] = FULLGAIN;
  DAC_GAIN[AFTERTOUCH] = HALFGAIN;
  DAC_GAIN[CONTROLCHANGE] = HALFGAIN;
  DAC_GAIN[PITCHBEND] = HALFGAIN;
  DAC_GAIN[PERCVELOCITY] = FULLGAIN;
  DAC_GAIN[PERCAFTERTOUCH] = HALFGAIN;
}

void loop()
{
  CheckMidiIn();
  CheckMidiTimer();
  ReadButtonPins();
  CheckOutputTimers();
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
  if( TimeDifference > 500 ){
    MidiTimer = millis();
    RecievedStop();
//    static bool schakel = 0;
//    schakel = (schakel + 1) % 2;
//    for ( uint8_t i = 0; i < 6; i++ ) {
//      digitalWrite(LOGIC_OUTPUTS[i], schakel);
//    }
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
      Serial.println("VoiceType:"+String(VoiceTypeKeys[CvChannels[i]])+" LastRow:"+String(LastRowKeys[CvChannels[i]]));
    }
  }
}

void CheckOutputTimers()
{
  for(int8_t i = 0; i < 6; i++){
    int8_t TimeDifference = millis() - Output_timers[i];
    if(Output_timers[i] > 0 && TimeDifference > 10){
      digitalWrite(LOGIC_OUTPUTS[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
    }
  }
}

void RecievedNoteOn(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
  if ( LearnMode ) {
    LearnNoteOn(Channel, NoteNumber, VelocityValue);
  } else {
    Check_and_PlayNotes(Channel, NoteNumber, VelocityValue);
  }
}

void LearnNoteOn(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  int8_t learn_loop_count = 0;
  for(int8_t i = 0; i < 4; i++){
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
  Serial.println("Ch:"+String(Channel)+" On:"+String(NoteNumber)+" Vel:"+String(VelocityValue));
  MidiValue[KEYS] = NoteNumber;
  MidiValue[VELOCITY] = VelocityValue;
  MidiValue[AFTERTOUCH] = NoteNumber;

  MsbNoteLsbVel = NoteNumber << 8 | VelocityValue;
//  Serial.print(String(highByte(MsbNoteLsbVel))+" "+String(lowByte(MsbNoteLsbVel)));
  
  MayPlayOne[KEYS] = true;
  
  for(int8_t i = 0; i < 4; i++){
    CheckGateChannelsForPerc(i, Channel, NoteNumber);
    CheckCvChannels(i, Channel, NoteNumber, VelocityValue);
    MayPlayCvatRow[i] = false;
  }
  MayPlayOne[VELOCITY] = false;
  MayPlayOne[AFTERTOUCH] = false;
  Serial.println();
}

void CheckGateChannelsForPerc(int8_t i, int8_t Channel, int8_t NoteNumber)
{
  if ( GateChannels[i] == Channel ) {
    CheckGateNotesForPerc(i, NoteNumber);
  }
}

void CheckGateNotesForPerc(int8_t i, int8_t NoteNumber)
{
  if ( GateNotes[i] == NoteNumber ) {
    CheckGateStatesForPerc(i);
  }
}

void CheckGateStatesForPerc(int8_t i)
{
  switch ( GateStates[i] ) {
    case PERCTRIGGER : {
      digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
      Output_timers[i+2] = millis();
      MayPlayCvatRow[i] = true;
      break;
    }
    case PERCGATE : {
      digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
      MayPlayCvatRow[i] = true;
      break;
    }
  }
}

void CheckCvChannels(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  if (CvChannels[i] == Channel) {
    Serial.print(String(i)+" ");
    CheckCvStatesForPerc(i, Channel, NoteNumber, VelocityValue);
    CheckVoiceTypeKeys(i, Channel, NoteNumber, VelocityValue);
    Serial.println();
  }
}

void CheckCvStatesForPerc(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  switch ( CvStates[i] ) {
    case PERCVELOCITY : {
      if ( MayPlayCvatRow[i] ) {
//        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[PERCVELOCITY], VelocityValue << 5);
        PwmOut(i, DAC_GAIN[PERCVELOCITY], VelocityValue << 1);
      }
      break;
    }
    case PERCAFTERTOUCH : {
      if ( MayPlayCvatRow[i] ) {
        AftertouchNoteNumber[i] = NoteNumber;
      }
      break;
    }
  }
}

void CheckVoiceTypeKeys(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  if ( VoiceTypeKeys[Channel] == MONOVOICE ) {
    CheckCvStatesForMonoKeys(i, Channel, NoteNumber, VelocityValue);
  } else if ( VoiceTypeKeys[Channel] > MONOVOICE ) {
    CheckCvStatesForPolyKeys(i, Channel, NoteNumber, VelocityValue);
  }
}

void CheckCvStatesForMonoKeys(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  switch ( CvStates[i] ) {
    case KEYS : {
      for (int8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
        if (NoteNumbers[i][n] == NOTHING) {
          NoteNumbers[i][n] = MsbNoteLsbVel;
          break;
        }
      }
      Serial.print("Play"+String(KEYS)+":"+String(NoteNumber));
//      FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], NoteNumber << 5);
      PwmOut(i, DAC_GAIN[KEYS], NoteNumber << 1);
      CheckGateState(KEYS, i, HIGH);
      MayPlayOne[VELOCITY] = true;
      MayPlayOne[AFTERTOUCH] = true;
      break;
    }
    case VELOCITY : {
      CheckVelocity(i, VelocityValue);
      break;
    }
    case AFTERTOUCH : {
      CheckAftertouch(i, NoteNumber);
      break;
    }
  }
}

void CheckCvStatesForPolyKeys(int8_t i, int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  switch ( CvStates[i] ) {
    case KEYS : {
      if ( MayPlayOne[KEYS] ) {
        if ( CurrentNotePlayed[i] == NOTHING ) {
          CurrentNotePlayed[i] = NoteNumber;
          Serial.print("Play"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
//          FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], CurrentNotePlayed[i] << 5);
          PwmOut(i, DAC_GAIN[KEYS], CurrentNotePlayed[i] << 1);
          CheckGateState(KEYS, i, HIGH);
          MayPlayOne[KEYS] = false;
          MayPlayOne[VELOCITY] = true;
          MayPlayOne[AFTERTOUCH] = true;
        } else if ( i == LastRowKeys[Channel] ) {
          Serial.print("Cur"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
          FillPolyValues(KEYS, Channel, MsbNoteLsbVel);
        } else {
          Serial.print("Cur"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
        }
      } else {
        Serial.print("Mp"+String(KEYS)+":"+String(MayPlayOne[KEYS]));
      }
      break;
    }
    case VELOCITY : {
      if ( CurrentVelocityPlayed[i] == NOTHING ) {
        CheckVelocity(i, VelocityValue);
      }
      break;
    }
    case AFTERTOUCH : {
      if ( CurrentAftertouchPlayed[i] == NOTHING ) {
        CheckAftertouch(i, NoteNumber);
      }
      break;
    }
  }
}

bool CheckVoiceTypeKeys(int8_t Channel)
{
  
}

void RecievedNoteOff(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  if ( !LearnMode ) {
    CheckNoteoff(Channel, NoteNumber, VelocityValue);
  }
}

void CheckNoteoff(int8_t Channel, int8_t NoteNumber, int8_t VelocityValue)
{
  Serial.println("Ch:"+String(Channel)+" Off:"+String(NoteNumber)+" Vel:"+String(VelocityValue));
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
      Serial.print(String(i)+" ");
      if ( CvStates[i] == KEYS ) {
        if ( VoiceTypeKeys[Channel] > MONOVOICE && MayPlayOne[KEYS] ) {
          if ( CurrentNotePlayed[i] == NoteNumber ) {
//            Serial.println("Value:"+String(highByte(PolyNotes[Channel][0])));
            if ( int8_t(highByte(PolyNotes[Channel][0])) == NOTHING ) {
  //            Serial.print("CurNote:"+String(CurrentNotePlayed[i]));
              CheckGateState(KEYS, i, LOW);
              CurrentNotePlayed[i] = NOTHING;
            } else {
              CurrentNotePlayed[i] = int8_t(highByte(PolyNotes[Channel][0]));
              Serial.print("Play"+String(KEYS)+":"+String(CurrentNotePlayed[i]));
//              FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], CurrentNotePlayed[i] << 5);
              PwmOut(i, DAC_GAIN[KEYS], CurrentNotePlayed[i] << 1);
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
          Serial.print("List:");
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
            Serial.print(String(highByte(NoteNumbers[i][n]))+" ");
          }
          if ( LastNote == NOTHING ) {
            CheckGateState(KEYS, i, LOW);
          } else {
            Serial.print("Play"+String(KEYS)+":"+String(highByte(LastNote)));
//            FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], highByte(LastNote) << 5);
            PwmOut(i, DAC_GAIN[KEYS], highByte(LastNote) << 1);
            MidiValue[VELOCITY] = int8_t(lowByte(LastNote));
            MayPlayOne[VELOCITY] = true;
            MidiValue[AFTERTOUCH] = int8_t(highByte(LastNote));
            MayPlayOne[AFTERTOUCH] = true;
          }
        }
      } else {
        CheckCvStates(i);
      }
      Serial.println();
    }
  }
  MayPlayOne[VELOCITY] = false;
  MayPlayOne[AFTERTOUCH] = false;
  Serial.println();
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
    Serial.print("PlayVEL:"+String(Value));
//    FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[VELOCITY], Value << 5);
    PwmOut(i, DAC_GAIN[VELOCITY], Value << 1);
    MayPlayOne[VELOCITY] = false;
  } else if ( CurrentVelocityPlayed[i] == Value ) {
    CurrentVelocityPlayed[i] = NOTHING;
  }
}

void CheckAftertouch(int8_t i, int8_t NoteNumber)
{
  if ( MayPlayOne[AFTERTOUCH] ) {
    CurrentAftertouchPlayed[i] = NoteNumber;
    Serial.print("PlayAT:"+String(NoteNumber));
    AftertouchNoteNumber[i] = NoteNumber;
    MayPlayOne[AFTERTOUCH] = false;
  } else if ( CurrentAftertouchPlayed[i] == NoteNumber ) {
    CurrentAftertouchPlayed[i] = NOTHING;
  }
}

void FillPolyValues(int8_t CvState, int8_t Channel, int16_t Value)
{
  Serial.print(" FillPoly"+String(CvState)+":");
  for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
    if ( PolyNotes[Channel][n] == NOTHING ) {
      PolyNotes[Channel][n] = Value;
      Serial.print(String(highByte(PolyNotes[Channel][n]))+" ");
      break;
    }
    Serial.print(String(highByte(PolyNotes[Channel][n]))+" ");
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
//  Serial.print("Search");
  for ( int8_t n = 0; n < SizeofRow_PolyNotes; n++ ) {
//    Serial.print(" "+String(PolyNotes[Channel][n]));
    if ( highByte(PolyNotes[Channel][n]) == Value ) {
      ShiftPolyValues(CvState, Channel, n);
      break;
    }
  }
}

void ShiftPolyValues(int8_t CvState, int8_t Channel, int8_t n)
{
  Serial.print(" ShiftPoly"+String(CvState)+":");
  for ( n; n < SizeofRow_PolyNotes; n++ ) {
    if ( n == SizeofRow_PolyNotes - 1 ) {
      PolyNotes[Channel][n] = NOTHING;
    } else if ( PolyNotes[Channel][n + 1] == NOTHING ) {
      PolyNotes[Channel][n] = NOTHING;
      Serial.print(String(PolyNotes[Channel][n])+" ");
      break;
    } else {
      PolyNotes[Channel][n] = PolyNotes[Channel][n + 1];
    }
    Serial.print(String(highByte(PolyNotes[Channel][n]))+" ");
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
        Serial.println(String(i)+" Ch:"+String(Channel)+" Val:"+String(PressureValue));
        Serial.flush();
//        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[AFTERTOUCH], PressureValue << 5);
        PwmOut(i, DAC_GAIN[AFTERTOUCH], PressureValue << 1);
      }
    }
  }
}

void RecievedNotePressure(int8_t Channel, int8_t NoteNumber, int8_t PressureValue)
{
//  Serial.println("Ch:"+String(Channel)+" Note:"+String(NoteNumber)+" PNAT:"+String(AftertouchNoteNumber[2]));
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
      Serial.println(String(i)+" Ch:"+String(Channel)+" Note:"+String(NoteNumber)+" Val:"+String(PressureValue));
      Serial.flush();
//      FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[AFTERTOUCH], PressureValue << 5);
      PwmOut(i, DAC_GAIN[AFTERTOUCH], PressureValue << 1);
    }
  }
}

void RecievedCC(int8_t Channel, int8_t CcNumber, int8_t CcValue)
{
  static int8_t Nrpn_Msb_chans[4];
  static int8_t Nrpn_Msb_nums[4];
  static int8_t Nrpn_Msb_vals[4];
  static bool MSB_READY[4];
  
  static int8_t Nrpn_Lsb_chans[4];
  static int8_t Nrpn_Lsb_nums[4];
  static int8_t Nrpn_Lsb_vals[4];
  static bool LSB_READY[4];

  static bool CCisOK[4];
  static int8_t CvNumbers[4];

  if (CcNumber == 99) {  //NRPN MSB
    for (int8_t i = 0; i < 4; i++) {
      if (bitRead(ButtonStates, i)) {
        Nrpn_Msb_chans[i] = Channel;
        Nrpn_Msb_nums[i] = CcNumber;
        Nrpn_Msb_vals[i] = CcValue;
      }
      if (Nrpn_Msb_chans[i] == Channel
      && Nrpn_Msb_nums[i] == CcNumber
      && Nrpn_Msb_vals[i] == CcValue) MSB_READY[i] = true;
      else MSB_READY[i] = false;
    }
  }
  else if (CcNumber == 98) { //NRPN LSB
    for (int8_t i = 0; i < 4; i++) {
      if (MSB_READY[i]) {
        if (bitRead(ButtonStates, i)) {
          Nrpn_Lsb_chans[i] = Channel;
          Nrpn_Lsb_nums[i] = CcNumber;
          Nrpn_Lsb_vals[i] = CcValue;
        }
        if (Nrpn_Lsb_chans[i] == Channel
        && Nrpn_Lsb_nums[i] == CcNumber
        && Nrpn_Lsb_vals[i] == CcValue) LSB_READY[i] = true;
        else LSB_READY[i] = false;
      }
    }
  }
  else {
    for (int8_t i = 0; i < 4; i++) {  //Scanned voor ingedrukte knop, en koppelt CC als knop is ingedrukt.
      if (CcNumber == 6) {  //Data Entry MSB
        if (MSB_READY[i]&& LSB_READY[i]) CCisOK[i] = true;
        else CCisOK[i] = false;
      }
      else CCisOK[i] = true;
      if (CCisOK[i]) {
        if(bitRead(ButtonStates, i)){
          CvChannels[i] = Channel;
          CvNumbers[i] = CcNumber;
          CvStates[i] = CONTROLCHANGE;
        }
        if(CvChannels[i] == Channel
        && CvNumbers[i] == CcNumber
        && CvStates[i] == CONTROLCHANGE){ //Stuur CC naar DAC. CC data is 7-bits. DAC is 12-bits. Bitshift van 4 naar links is 11 bits dus dat is de helft van de DAC output.
//          FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[VELOCITY], CcValue << 5);
          PwmOut(i, DAC_GAIN[VELOCITY], CcValue << 1);
        }
      }
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
//        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[PITCHBEND], Bend + 8192 >> 2); //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
        PwmOut(i, DAC_GAIN[PITCHBEND], (Bend + 8192) >> 4); //Bend data is 14-bits. PWM is 8-bit. Dus bitshift van 4 naar rechts.
      }
    }
  }
}

//void FormMsbLsb(int8_t Dac[2], int8_t gain, unsigned int mV)
//{
//  bool Pin = 0;
//  bool Chan = 1;
//  int8_t Msb = Dac[Chan] | gain | highByte(mV);
//  int8_t Lsb = lowByte(mV);
//  
////  SendSpi(Dac[Pin], Msb, Lsb);
//}

void PwmOut(uint8_t i, int8_t gain, uint8_t mV)
{
  uint8_t pwmVal = min(mV, 127) * gain;
  switch (i) {
    case 0:
      PWM6 = pwmVal;
      break;
    case 1:
      PWM9 = pwmVal;
      break;
    case 2:
      PWM10 = pwmVal;
      break;
    case 3:
      PWM13 = pwmVal;
      break;
  }
}

void RecievedClock(void)
{
  if(clock_count == 0 && (clk_state == clk_start || clk_state == clk_continue)) digitalWrite(CLOCK, HIGH); // Start clock pulse
  if(clk_state != clk_stop) clock_count++;
  if(clock_count >= 3) {  // MIDI timing clock sends 24 pulses per quarter NoteNumber.  Sent pulse only once every 3 pulses (32th NoteNumbers).
    digitalWrite(CLOCK, LOW);
    clock_count = 0;
  }
}

void RecievedStart(void)
{
  digitalWrite(LOGIC_OUTPUTS[1], HIGH);
  Output_timers[1] = millis();
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
