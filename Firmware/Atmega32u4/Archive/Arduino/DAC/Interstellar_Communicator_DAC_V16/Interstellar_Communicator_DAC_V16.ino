/*
1-5-19
Jason

Aftertouch nog testen.

Erase program toevoegen. (2x snel drukken knop).

Update naar eeprom bij elke alle knoppen los registratie.
*/

#include <MIDI.h>

#include <SPI.h>

#define CLOCK 2
#define RESET 3
#define GT1  4
#define GT2  5
#define GT3  6
#define GT4  7
#define DAC_1  8
#define DAC_2  9
#define BUTTON_1 A0
#define BUTTON_2 A1
#define BUTTON_3 A2
#define BUTTON_4 A3

//Constants
const int8_t LOGIC_OUTPUTS[6] = {CLOCK, RESET, GT1, GT2, GT3, GT4};

const int8_t DAC_CHAN_0 = 0x10;
const int8_t DAC_CHAN_1 = 0x90;
const int8_t DAC_PIN_CHAN[4][2] = {
  {DAC_1, DAC_CHAN_0},
  {DAC_1, DAC_CHAN_1},
  {DAC_2, DAC_CHAN_0},
  {DAC_2, DAC_CHAN_1}
};

const int8_t HALFGAIN = 0x20;
const int8_t FULLGAIN = 0;
int8_t DAC_GAIN[10];

const int8_t NOTHING = -1;

const int8_t MONOVOICE = 0;
const int8_t DUOVOICE = 1;
const int8_t TRIVOICE = 2;
const int8_t QUADVOICE = 3;

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
bool SpiTransactionEnded = true;

//Learn modus stuff
int8_t ButtonsPushed;
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
bool MayPlay[3];
int8_t VoiceType[3][17];
int8_t SizeofRow_VoiceType;
int8_t LastRow[3][17];
int8_t CurrentValuePlayed[3][4] = {
  {-1, -1, -1, -1},
  {-1, -1, -1, -1},
  {-1, -1, -1, -1}
};
int8_t SizeofRow_CurrentValuePlayed;
int16_t PolyValues[3][3][10];
int8_t SizeofRow_PolyValues;

//Mono Stuff
int16_t MsbNoteLsbVel;
int16_t NoteNumbers[4][10] = {
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};
int8_t SizeofRow_NoteNumbers;

int8_t RowOfPlayedNote[17] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

//Drumstuff
bool PlayDrumTrigVel[4] = {false, false, false, false};

//Aftertouch stuff
int8_t play_next_aftertouch[4];

//Other
int8_t MidiValue[3];

//Clock stuff
int8_t clk_stop = 0;
int8_t clk_start = 1;
int8_t clk_continue = 2;
int8_t clk_state = clk_stop;

int8_t clock_count; //Telt de ontvangen midi clock messages.

//Timers
unsigned long timerMIDIrecieved;
unsigned long timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
unsigned long Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

MIDI_CREATE_DEFAULT_INSTANCE(); 

void setup()
{
  setinputsandoutputs();
  configuremidisethandle();
  ConfigureDAC_GAIN();
  CalcSizeofArrays();

  Serial.println("Interstellar Com OK");
}

void setinputsandoutputs()
{
  SPI.begin();
  MIDI.begin(MIDI_CHANNEL_OMNI);
  
  pinMode(CLOCK, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(GT1, OUTPUT);
  pinMode(GT2, OUTPUT);
  pinMode(GT3, OUTPUT);
  pinMode(GT4, OUTPUT);
  pinMode(DAC_1, OUTPUT); //dac 1 Channel 1 and 2
  pinMode(DAC_2, OUTPUT); //dac 2 Channel 1 and 2
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
  digitalWrite(DAC_1,HIGH);
  digitalWrite(DAC_2,HIGH);

  //Sets the DAC outputs on lowest output, which translates to 0V. Which is NoteNumber C-1 and midi NoteNumber 0.
//   FormMsbLsb(1, 0x00, DacMv[1]); 
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

void CalcSizeofArrays()
{
  SizeofRow_NoteNumbers = sizeof(NoteNumbers[0]);
  SizeofRow_PolyValues = sizeof(PolyValues[0][0]) / sizeof(PolyValues[0][0][0]);
  SizeofRow_VoiceType = sizeof(VoiceType[0]);
  SizeofRow_CurrentValuePlayed = sizeof(CurrentValuePlayed[0]);
}

void loop()
{
  ReadMidi_and_if_True_StartTimer();
  if_LastMidiLongAgo_StopClock_and_TurnNotesOff();
  ReadButtonPins();
  CheckOutputTimers();
}

void ReadMidi_and_if_True_StartTimer()
{
  if(MIDI.read()){
    timerMIDIrecieved = millis();
    if ( SpiTransactionEnded ) {
      SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
      SpiTransactionEnded = false;
      Serial.println("SPI started");
    }
  }
}

void if_LastMidiLongAgo_StopClock_and_TurnNotesOff()
{
  if(millis() - timerMIDIrecieved > 500){
    timerMIDIrecieved = millis();
    RecievedStop();
    if ( SpiTransactionEnded == false ) {
      SPI.endTransaction();
      SpiTransactionEnded = true;
      Serial.println("SPI ended");
    }
  }
}

void ReadButtonPins()
{
  ReadButtonPins_Atmega32u4_PINF();
  if( ButtonsPushed == 0 && LearnMode == true ){
    learn_NoteNumbers_count = 0;
    SetPolyArrays();
    CheckForVoiceTypes();
    MayProgramAftertouch = ButtonsPushed;
    LearnMode = false;
  } else if ( ButtonsPushed && LearnMode == false){
    LearnMode = true;
  }
}

void ReadButtonPins_Atmega32u4_PINF() //A0 == PF7, A1 == PF6, A2 == PF5, A3 == PF4.
{
  static int8_t LastTemporaryButtonsPushed = 0;
  int8_t TemporaryButtonsPushed = lowByte(~PINF) & B11110000;
  if ( TemporaryButtonsPushed != LastTemporaryButtonsPushed ) {
    LastTemporaryButtonsPushed = TemporaryButtonsPushed;
    for ( int8_t i = 0; i < 7; i++ ) {
      bitWrite(ButtonsPushed, i, bitRead(TemporaryButtonsPushed, -i + 7));
    }
  }
}

void SetPolyArrays()
{
  for ( int8_t i = 0; i < 3; i++ ) {
//  Serial.println(SizeofRow_VoiceType);
//  Serial.println(SizeofRow_PolyValues);
    for ( int8_t c = 0; c < SizeofRow_VoiceType; c++ ) {
      VoiceType[i][c] = NOTHING;
      LastRow[i][c] = NOTHING;
      if ( c < 3 ) {
        for ( int8_t v = 0; v < SizeofRow_PolyValues; v++ ) {
          PolyValues[i][c][v] = NOTHING;
//          if ( v + 1 == SizeofRow_PolyValues ) {
//            Serial.println("gelukt");
//          }
        }
      }
    }
  }
}

void CheckForVoiceTypes()
{
  CountVoiceTypeForCvState(KEYS);
  CountVoiceTypeForCvState(VELOCITY);
  CountVoiceTypeForCvState(AFTERTOUCH);
}

void CountVoiceTypeForCvState(int8_t CvState)
{
  for (byte i = 0; i < sizeof(CvStates); i++) {
    if (CvStates[i] == CvState) {
      VoiceType[CvState][CvChannels[i]]++;
      LastRow[CvState][CvChannels[i]] = i;
      Serial.println(String(CvState)+" VoiceType:"+String(VoiceType[CvState][CvChannels[i]])+" LastRow:"+String(LastRow[CvState][CvChannels[i]]));
    }
  }
}

void CheckOutputTimers()
{
  for(int8_t i = 0; i < 6; i++){
    if(Output_timers[i] > 0 && millis() - Output_timers[i] > 10){
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
    if(bitRead(ButtonsPushed, i)){
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
  
  MayPlay[KEYS] = true;
  
  for(int8_t i = 0; i < 4; i++){
    if (GateChannels[i] == Channel && GateNotes[i] == NoteNumber) {
      if (GateStates[i] == PERCGATE) {
        digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
        DrumGateNoteAfter[i] = NoteNumber;
      }
      if (GateStates[i] == PERCTRIGGER) {
        digitalWrite(LOGIC_OUTPUTS[i+2], HIGH);
        Output_timers[i+2] = millis();
        PlayDrumTrigVel[i] = true;
      }
    }
    if (CvChannels[i] == Channel) {
      Serial.print(String(i)+" ");
      if ( CvStates[i] == KEYS ) {
        if ( VoiceType[KEYS][Channel] > MONOVOICE && MayPlay[KEYS] ) {
          if ( CurrentValuePlayed[KEYS][i] == NOTHING ) {
            CurrentValuePlayed[KEYS][i] = NoteNumber;
            Serial.print("Play"+String(KEYS)+":"+String(CurrentValuePlayed[KEYS][i]));
            FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], CurrentValuePlayed[KEYS][i] << 5);
            CheckGateState(KEYS, i, HIGH);
            MayPlay[KEYS] = false;
            MayPlay[VELOCITY] = true;
            MayPlay[AFTERTOUCH] = true;
          } else if ( i == LastRow[KEYS][Channel] ) {
            Serial.print("Cur"+String(KEYS)+":"+String(CurrentValuePlayed[KEYS][i]));
            FillPolyValues(KEYS, Channel, MsbNoteLsbVel);
          } else {
            Serial.print("Cur"+String(KEYS)+":"+String(CurrentValuePlayed[KEYS][i]));
          }
        } else if ( VoiceType[KEYS][Channel] > MONOVOICE ) {
          Serial.print("Mp"+String(KEYS)+":"+String(MayPlay[KEYS]));
        } else if ( VoiceType[KEYS][Channel] == MONOVOICE ) {
          for (int8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
            if (NoteNumbers[i][n] == NOTHING) {
              NoteNumbers[i][n] = MsbNoteLsbVel;
//              CurrentValuePlayed[KEYS][i] = NoteNumber;
              break;
            }
          }
          Serial.print("Play"+String(KEYS)+":"+String(NoteNumber));
          FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], NoteNumber << 5);
          CheckGateState(KEYS, i, HIGH);
          MayPlay[VELOCITY] = true;
          MayPlay[AFTERTOUCH] = true;
//          RowOfPlayedNote[Channel] = i;
        }
      } else if ( CvStates[i] == VELOCITY ) {
        CheckVelocity(i, VelocityValue);
      } else if ( CvStates[i] == AFTERTOUCH ) {
        CheckAftertouch(i);
      } else if ( CvStates[i] == PERCVELOCITY && PlayDrumTrigVel[i] ) {
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[PERCVELOCITY], VelocityValue << 5); 
      }
      Serial.println();
    }
    PlayDrumTrigVel[i] = false;
  }
  MayPlay[VELOCITY] = false;
  MayPlay[AFTERTOUCH] = false;
  Serial.println();
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

  MayPlay[KEYS] = true;
  for (int8_t i = 0; i < 4; i++) {
    if (GateChannels[i] == Channel
    && GateStates[i] == PERCGATE
    && GateNotes[i] == NoteNumber) {
      digitalWrite(LOGIC_OUTPUTS[i+2], LOW);
    }
    if ( CvChannels[i] == Channel ) {
      Serial.print(String(i)+" ");
      if ( CvStates[i] == KEYS ) {
        if ( VoiceType[KEYS][Channel] > MONOVOICE && MayPlay[KEYS] ) {
          if ( CurrentValuePlayed[KEYS][i] == NoteNumber ) {
//            Serial.println("Value:"+String(highByte(PolyValues[KEYS][Channel][0])));
            if ( int8_t(highByte(PolyValues[KEYS][Channel][0])) == NOTHING ) {
  //            Serial.print("CurNote:"+String(CurrentValuePlayed[KEYS][i]));
              CheckGateState(KEYS, i, LOW);
              CurrentValuePlayed[KEYS][i] = NOTHING;
            } else {
              CurrentValuePlayed[KEYS][i] = int8_t(highByte(PolyValues[KEYS][Channel][0]));
              Serial.print("Play"+String(KEYS)+":"+String(CurrentValuePlayed[KEYS][i]));
              FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], CurrentValuePlayed[KEYS][i] << 5);
              MayPlay[KEYS] = false;
              MidiValue[VELOCITY] = int8_t(lowByte(PolyValues[KEYS][Channel][0]));
              MayPlay[VELOCITY] = true;
              MidiValue[AFTERTOUCH] = CurrentValuePlayed[KEYS][i];
              MayPlay[AFTERTOUCH] = true;
              ShiftPolyValues(KEYS, Channel, 0);
            }
          } else if ( i == LastRow[KEYS][Channel] ) {
            SearchInPolyValues(KEYS, Channel, highByte(MsbNoteLsbVel));
          }
        } else if ( VoiceType[KEYS][Channel] == MONOVOICE ) {
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
            FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], highByte(LastNote) << 5);
            MidiValue[VELOCITY] = int8_t(lowByte(LastNote));
            MayPlay[VELOCITY] = true;
            MidiValue[AFTERTOUCH] = int8_t(highByte(LastNote));
            MayPlay[AFTERTOUCH] = true;
          }
        }
      } else if ( CvStates[i] == VELOCITY ) {
        CheckVelocity(i, MidiValue[VELOCITY]);
      } else if ( CvStates[i] == AFTERTOUCH ) {
        CheckAftertouch(i);
      }
      Serial.println();
    }
  }
  MayPlay[VELOCITY] = false;
  MayPlay[AFTERTOUCH] = false;
  Serial.println();
}

void CheckVelocity(int8_t i, int8_t Value)
{
  if ( MayPlay[VELOCITY] ) {
    Serial.print("Play"+String(VELOCITY)+":"+String(Value));
    FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[VELOCITY], Value << 5); 
    MayPlay[VELOCITY] = false;
  }
}

void CheckAftertouch(int8_t i)
{
  if ( MayPlay[AFTERTOUCH] ) {
    Serial.print("Play"+String(AFTERTOUCH)+":"+String(MidiValue[AFTERTOUCH]));
    play_next_aftertouch[i] = MidiValue[AFTERTOUCH];
    MayPlay[AFTERTOUCH] = false;
  }
}

void FillPolyValues(int8_t CvState, int8_t Channel, int16_t Value)
{
  Serial.print(" FillPoly"+String(CvState)+":");
  for ( int8_t n = 0; n < SizeofRow_PolyValues; n++ ) {
    if ( PolyValues[CvState][Channel][n] == NOTHING ) {
      PolyValues[CvState][Channel][n] = Value;
      Serial.print(String(highByte(PolyValues[CvState][Channel][n]))+" ");
      break;
    }
    Serial.print(String(highByte(PolyValues[CvState][Channel][n]))+" ");
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
  for ( int8_t n = 0; n < SizeofRow_PolyValues; n++ ) {
//    Serial.print(" "+String(PolyValues[CvState][Channel][n]));
    if ( highByte(PolyValues[CvState][Channel][n]) == Value ) {
      ShiftPolyValues(CvState, Channel, n);
      break;
    }
  }
}

void ShiftPolyValues(int8_t CvState, int8_t Channel, int8_t n)
{
  Serial.print(" ShiftPoly"+String(CvState)+":");
  for ( n; n < SizeofRow_PolyValues; n++ ) {
    if ( n == SizeofRow_PolyValues - 1 ) {
      PolyValues[CvState][Channel][n] = NOTHING;
    } else if ( PolyValues[CvState][Channel][n + 1] == NOTHING ) {
      PolyValues[CvState][Channel][n] = NOTHING;
      Serial.print(String(PolyValues[CvState][Channel][n])+" ");
      break;
    } else {
      PolyValues[CvState][Channel][n] = PolyValues[CvState][Channel][n + 1];
    }
    Serial.print(String(highByte(PolyValues[CvState][Channel][n]))+" ");
  }
}

void RecievedChannelPressure(int8_t Channel, int8_t PressureValue)
{
  if ( LearnMode ) {
//  Serial.println("RecievedChannelPressure");
//  Serial.flush();
    for(int8_t i = 0; i < 4; i++){
      if ( bitRead(ButtonsPushed, i)
      && bitRead(MayProgramAftertouch, i) ) {
        CvChannels[i] = Channel;
        CvStates[i] = AFTERTOUCH;
      }
    }
  } else {
    for(int8_t i = 0; i < 4; i++){
      if(CvChannels[i] == Channel
      && CvStates[i] == AFTERTOUCH){
        Serial.println("ChanAT:"+String(PressureValue));
        Serial.flush();
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[AFTERTOUCH], PressureValue << 5);
      }
    }
  }
}

void RecievedNotePressure(int8_t Channel, int8_t NoteNumber, int8_t PressureValue)
{
//  Serial.print("playnextafter ");
//  Serial.println(play_next_aftertouch[i]);
//  Serial.flush();
//  Serial.print("PolyAftertouchCount ");
//  Serial.println(PolyAftertouchCount[Channel]);
//  Serial.flush();

//  Serial.print("NoteNumber ");
//  Serial.println(NoteNumber);
//  Serial.flush();
  for (int8_t i = 0; i < 4; i++) {
    if (bitRead(ButtonsPushed, i)
    && bitRead(MayProgramAftertouch, i)) {
      CvChannels[i] = Channel;
      CvStates[i] = AFTERTOUCH;
      if ( GateStates[i] == PERCGATE ) {
        CvStates[i] = PERCAFTERTOUCH;
      }
    }
    if ( CvChannels[i] == Channel
    && ( ( CvStates[i] == AFTERTOUCH && play_next_aftertouch[i] == NoteNumber )
    || ( CvStates[i] == PERCAFTERTOUCH && DrumGateNoteAfter[i] == NoteNumber ) ) ) {
        Serial.println("NoteAT:"+String(PressureValue));
        Serial.flush();
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[AFTERTOUCH], PressureValue << 5);
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
      if (bitRead(ButtonsPushed, i)) {
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
        if (bitRead(ButtonsPushed, i)) {
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
        if(bitRead(ButtonsPushed, i)){
          CvChannels[i] = Channel;
          CvNumbers[i] = CcNumber;
          CvStates[i] = CONTROLCHANGE;
        }
        if(CvChannels[i] == Channel
        && CvNumbers[i] == CcNumber
        && CvStates[i] == CONTROLCHANGE){ //Stuur CC naar DAC. CC data is 7-bits. DAC is 12-bits. Bitshift van 4 naar links is 11 bits dus dat is de helft van de DAC output.
          FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[VELOCITY], CcValue << 5);
        }
      }
    }
  }
}

void RecievedPitchBend(int8_t Channel, int Bend)
{
  if(Bend >= -8192 && Bend <= 8191){
    for(int8_t i = 0; i < 4; i++){
      if(bitRead(ButtonsPushed, i)){
        CvChannels[i] = Channel;
        CvStates[i] = PITCHBEND;
      }
      if ( CvChannels[i] == Channel && CvStates[i] == PITCHBEND ) {
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[PITCHBEND], Bend + 8192 >> 2); //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
      }
    }
  }
}

void FormMsbLsb(int8_t Dac[2], int8_t gain, unsigned int mV)
{
  bool Pin = 0;
  bool Chan = 1;
  int8_t Msb = Dac[Chan] | gain | highByte(mV);
  int8_t Lsb = lowByte(mV);
  
  SendSpi(Dac[Pin], Msb, Lsb);
}

void SendSpi(int8_t dacpin, int8_t Msb, int8_t Lsb)
{
  digitalWrite(dacpin,LOW);
  SPI.transfer(Msb);
  SPI.transfer(Lsb);
  digitalWrite(dacpin,HIGH);
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
