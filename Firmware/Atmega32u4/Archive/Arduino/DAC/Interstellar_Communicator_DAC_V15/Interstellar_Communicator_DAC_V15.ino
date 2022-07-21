/*
20-4-19
Jason

Gebruik voor VelocityValue en aftertouch gewoon de NoteChanVoiceType. Want poly VelocityValue en aftertouch zonder poly NoteNumber programmed slaat nergens op.
En velocity en afrtouch MOETEN afhankelijk zijn van note. Want in een 0,note 1,vel 2,note 3,vel situatie mag de vel van de 2e note nooit op de plek van de vel van note 1 spelen.
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
const uint8_t LOGIC_OUTPUTS[6] = {CLOCK, RESET, GT1, GT2, GT3, GT4};

const uint8_t DAC_CHAN_0 = 0x10;
const uint8_t DAC_CHAN_1 = 0x90;
const uint8_t DAC_PIN_CHAN[4][2] = {
  {DAC_1, DAC_CHAN_0},
  {DAC_1, DAC_CHAN_1},
  {DAC_2, DAC_CHAN_0},
  {DAC_2, DAC_CHAN_1}
};

const uint8_t HALFGAIN = 0x20;
const uint8_t FULLGAIN = 0;
uint8_t DAC_GAIN[10];

const int8_t NOTHING = -1;

const int8_t MONOVOICE = 0;
const int8_t DUOVOICE = 1;
const int8_t TRIVOICE = 2;
const int8_t QUADVOICE = 3;

const uint8_t KEYS = 0;
const uint8_t VELOCITY = 1;
const uint8_t AFTERTOUCH = 2;
const uint8_t CONTROLCHANGE = 3;
const uint8_t PITCHBEND = 4;
const uint8_t PERCVELOCITY = 5;
const uint8_t PERCAFTERTOUCH = 6;
const uint8_t PERCTRIGGER = 7;
const uint8_t PERCGATE = 8;

//Setup stuff
bool SpiTransactionEnded = true;

//Learn modus stuff
uint8_t ButtonsPushed;
bool LearnMode = false;
int8_t learn_NoteNumbers_count;

uint8_t CvChannels[4];
int8_t CvStates[4] = {-1, -1, -1, -1};
uint8_t CvAfterNote[4];

uint8_t GateChannels[4];
uint8_t GateStates[4];
uint8_t GateNotes[4];
int8_t DrumGateNoteAfter[4] = {-1, -1, -1, -1};

uint8_t MayProgramAftertouch;

uint8_t play_next_aftertouch[4];

//Poly stuff
bool MayPlay[3];
int8_t VoiceType[3][17];
uint8_t SizeofRow_VoiceType;
int8_t LastRow[3][17];
int8_t CurrentValuePlayed[3][4] = {
  {-1, -1, -1, -1},
  {-1, -1, -1, -1},
  {-1, -1, -1, -1}
};
uint8_t SizeofRow_CurrentValuePlayed;
int8_t PolyValues[3][10] = {
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};
uint8_t SizeofRow_PolyValues;

//Mono Stuff
int8_t NoteNumbers[4][10] = {
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};
uint8_t SizeofRow_NoteNumbers;

int8_t RowOfPlayedNote[17] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

bool PlayDrumTrigVel[4] = {false, false, false, false};

//Clock stuff
uint8_t clk_stop = 0;
uint8_t clk_start = 1;
uint8_t clk_continue = 2;
uint8_t clk_state = clk_stop;

uint8_t clock_count; //Telt de ontvangen midi clock messages.

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
  SizeofRow_PolyValues = sizeof(PolyValues[0]);
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
  static uint8_t LastTemporaryButtonsPushed = 0;
  uint8_t TemporaryButtonsPushed = lowByte(~PINF) & B11110000;
  if ( TemporaryButtonsPushed != LastTemporaryButtonsPushed ) {
    LastTemporaryButtonsPushed = TemporaryButtonsPushed;
    for ( uint8_t i = 0; i < 7; i++ ) {
      bitWrite(ButtonsPushed, i, bitRead(TemporaryButtonsPushed, -i + 7));
    }
  }
}

void SetPolyArrays()
{
  for ( uint8_t i = 0; i < 3; i++ ) {
    for ( uint8_t c = 0; c < SizeofRow_VoiceType; c++ ) {
      VoiceType[i][c] = NOTHING;
      LastRow[i][c] = NOTHING;
    }
  }
}

void CheckForVoiceTypes()
{
  CountVoiceTypeForCvState(KEYS);
  CountVoiceTypeForCvState(VELOCITY);
  CountVoiceTypeForCvState(AFTERTOUCH);
}

void CountVoiceTypeForCvState(uint8_t CvState)
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
  for(uint8_t i = 0; i < 6; i++){
    if(Output_timers[i] > 0 && millis() - Output_timers[i] > 10){
      digitalWrite(LOGIC_OUTPUTS[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
    }
  }
}

void RecievedNoteOn(uint8_t Channel, uint8_t NoteNumber, uint8_t VelocityValue) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
  if ( LearnMode ) {
    LearnNoteOn(Channel, NoteNumber, VelocityValue);
  } else {
    Check_and_PlayNotes(Channel, NoteNumber, VelocityValue);
  }
}

void LearnNoteOn(uint8_t Channel, uint8_t NoteNumber, uint8_t VelocityValue)
{
  uint8_t learn_loop_count = 0;
  for(uint8_t i = 0; i < 4; i++){
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

void Check_and_PlayNotes(uint8_t Channel, uint8_t NoteNumber, uint8_t VelocityValue)
{
  Serial.println("Ch:"+String(Channel)+" On:"+String(NoteNumber)+" Vel:"+String(VelocityValue));
  uint8_t MidiValue[3];
  MidiValue[KEYS] = NoteNumber;
  MidiValue[VELOCITY] = VelocityValue;
  MidiValue[AFTERTOUCH] = NoteNumber;

  MayPlay[KEYS] = true;
  MayPlay[VELOCITY] = true;
  MayPlay[AFTERTOUCH] = true;
  for(uint8_t i = 0; i < 4; i++){
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
      if ( VoiceType[KEYS][Channel] > MONOVOICE ) {
        for ( uint8_t CvState = KEYS; true; ){
          if ( CvStates[i] == CvState && MayPlay[CvState] ) {
            Serial.print(String(i)+" ");
            if ( CurrentValuePlayed[CvState][i] == NOTHING ) {
              Serial.print("Play"+String(CvState)+":"+String(MidiValue[CvState]));
              if ( CvState != AFTERTOUCH ) FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[CvState], MidiValue[CvState] << 5);
              CheckGateState(CvState, i, HIGH);
              CurrentValuePlayed[CvState][i] = MidiValue[CvState];
              MayPlay[CvState] = false;
            } else if ( i == LastRow[CvState][Channel] ) {
              Serial.print("Cur"+String(CvState)+":"+String(CurrentValuePlayed[CvState][i]));
              FillPolyValues(CvState, MidiValue[CvState]);
            } else {
              Serial.print("Cur"+String(CvState)+":"+String(CurrentValuePlayed[CvState][i]));
            }
            Serial.println();
            break;
          } else if ( CvStates[i] == CvState ) {
            Serial.println(String(i)+" Mp"+String(CvState)+":"+String(MayPlay[CvState]));
            break;
          } else if ( CvState == KEYS ) {
            CvState = VELOCITY;
          } else if ( CvState == VELOCITY ) {
            CvState = AFTERTOUCH;
          } else break;
        };
      } else if ( VoiceType[KEYS][Channel] == MONOVOICE ) {
        for (uint8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
          if (NoteNumbers[i][n] == NOTHING) {
            NoteNumbers[i][n] = NoteNumber;
            break;
          }
        }
        Serial.print("Play:"+String(NoteNumber));
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], NoteNumber << 5);
        CheckGateState(KEYS, i, HIGH);
        RowOfPlayedNote[Channel] = i;
      }
      else if ( CvStates[i] == VELOCITY && VoiceType[KEYS][Channel] > MONOVOICE ) {
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[VELOCITY], VelocityValue << 5); 
      }
      else if ( CvStates[i] == PERCVELOCITY && PlayDrumTrigVel[i] ) {
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[PERCVELOCITY], VelocityValue << 5); 
      }
//      else if ( CvStates[i] == AFTERTOUCH
//      && CurrentValuePlayed[AFTERTOUCH] > NOTHING ) {
//        play_next_aftertouch[i] = NoteNumbers[RowOfPlayedNote[Channel]][0];
//        RowOfPlayedNote[Channel] = NOTHING;
//      }
    }
    PlayDrumTrigVel[i] = false;
  }
  Serial.println();
}

void RecievedNoteOff(uint8_t Channel, uint8_t NoteNumber, uint8_t VelocityValue)
{
  if ( !LearnMode ) {
    CheckNoteoff(Channel, NoteNumber, VelocityValue);
  }
}

void CheckNoteoff(uint8_t Channel, uint8_t NoteNumber, uint8_t VelocityValue)
{
  Serial.println("Ch:"+String(Channel)+" Off:"+String(NoteNumber)+" Vel:"+String(VelocityValue));
  uint8_t MidiValue[3];
  MidiValue[KEYS] = NoteNumber;
  MidiValue[VELOCITY] = VelocityValue;
  MidiValue[AFTERTOUCH] = NoteNumber;

  MayPlay[KEYS] = true;
  MayPlay[VELOCITY] = true;
  MayPlay[AFTERTOUCH] = true;
  for (uint8_t i = 0; i < 4; i++) {
    if (GateChannels[i] == Channel
    && GateStates[i] == PERCGATE
    && GateNotes[i] == NoteNumber) {
      digitalWrite(LOGIC_OUTPUTS[i+2], LOW);
    }
    if ( CvChannels[i] == Channel ) {
      if ( VoiceType[KEYS][Channel] > MONOVOICE ) {
        for ( uint8_t CvState = KEYS; true; ){
          if ( CvStates[i] == CvState && MayPlay[CvState] ) {
            Serial.print(String(i)+" ");
            if ( CurrentValuePlayed[CvState][i] == MidiValue[CvState] || CvState == VELOCITY ) {
              if ( PolyValues[CvState][0] == NOTHING ) {
    //            Serial.print("CurNote:"+String(CurrentValuePlayed[CvState][i]));
                CheckGateState(CvState, i, LOW);
                CurrentValuePlayed[CvState][i] = NOTHING;
              } else {
                Serial.print("Play"+String(CvState)+":"+String(PolyValues[CvState][0]));
                FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[CvState], PolyValues[CvState][0] << 5);
                CurrentValuePlayed[CvState][i] = PolyValues[CvState][0];
                MayPlay[CvState] = false;
                ShiftPolyValues(CvState, 0);
              }
            } else if ( i == LastRow[CvState][Channel] ) {
              FindNoteInPolyValues(CvState, MidiValue[CvState]);
            }
            Serial.println();
            break;
          } else if ( CvState == KEYS ) {
            CvState = VELOCITY;
          } else if ( CvState == VELOCITY ) {
            CvState = AFTERTOUCH;
          } else break;
        };
      } else if ( VoiceType[KEYS][Channel] == MONOVOICE ) {
        if ( CvStates[i] == KEYS && MayPlay[KEYS] ) {
          int8_t LastNote = NOTHING;
          for (uint8_t n = 0; n < SizeofRow_NoteNumbers; n++) {
            if (NoteNumbers[i][n] == NoteNumber
            || NoteNumbers[i][n] == NOTHING) {
              if (n < SizeofRow_NoteNumbers - 1) {             
                if(NoteNumbers[i][n + 1] >= 0){  // Shifts all NoteNumbers to the left where the NoteNumber was removed.
                  NoteNumbers[i][n] = NoteNumbers[i][n+1];
                  NoteNumbers[i][n + 1] = NOTHING;
                } else if ( NoteNumbers[i][n+1] == NOTHING ){
                  NoteNumbers[i][n] = NOTHING;
                  if(n == 0) {
                    LastNote = NOTHING; // If the current NoteNumber is the first in the array. Then there are no NoteNumbers on.
                  } else {
                    LastNote = NoteNumbers[i][n-1]; //  When the next NoteNumber in the array is also NOTHING, the last NoteNumber will be the NoteNumber before the current one.
                  }
                  break;
                }
              } else if(n == SizeofRow_NoteNumbers - 1){ // If the NoteNumber on place 19 in the array is the OFF NoteNumber. Then this will be removed and the last NoteNumber will be the NoteNumber before this.
                NoteNumbers[i][n] = NOTHING;
                LastNote = NoteNumbers[i][n-1];
              }
            }
          }
          if ( LastNote == NOTHING ) {
            CheckGateState(KEYS, i, LOW);
          } else {
            Serial.print("Play:"+String(LastNote));
            FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], LastNote << 5);
          }
        }
      }
    }
  }
  Serial.println();
}

void FillPolyValues(uint8_t CvState, uint8_t Value)
{
  Serial.print(" Poly"+String(CvState)+":");
  for ( uint8_t n = 0; n < SizeofRow_PolyValues; n++ ) {
    if ( PolyValues[CvState][n] == NOTHING ) {
      PolyValues[CvState][n] = Value;
      Serial.print(String(PolyValues[CvState][n])+" ");
      break;
    }
    Serial.print(String(PolyValues[CvState][n])+" ");
  }
}

void CheckGateState(uint8_t CvState, uint8_t i, bool Set)
{
  if ( GateStates[i] == CvState ) {
    digitalWrite(LOGIC_OUTPUTS[i+2], Set);
  }
}

void FindNoteInPolyValues(uint8_t CvState, uint8_t Value)
{
  for ( uint8_t n = 0; n < SizeofRow_PolyValues; n++ ) {
    if ( PolyValues[CvState][n] == Value ) {
      ShiftPolyValues(CvState, n);
      break;
    }
  }
}

void ShiftPolyValues(uint8_t CvState, uint8_t n)
{
  Serial.print(" Poly"+String(CvState)+":");
  for ( n; n < SizeofRow_PolyValues; n++ ) {
    if ( (n == SizeofRow_PolyValues - 1) ) {
      PolyValues[CvState][n] = NOTHING;
    } else if ( PolyValues[CvState][n+1] == NOTHING ) {
      PolyValues[CvState][n] = NOTHING;
      Serial.print(String(PolyValues[CvState][n])+" ");
      break;
    } else {
      PolyValues[CvState][n] = PolyValues[CvState][n+1];
    }
    Serial.print(String(PolyValues[CvState][n])+" ");
  }
}

void RecievedChannelPressure(uint8_t Channel, uint8_t PressureValue)
{
  if ( LearnMode ) {
//  Serial.println("RecievedChannelPressure");
//  Serial.flush();
    for(uint8_t i = 0; i < 4; i++){
      if ( bitRead(ButtonsPushed, i)
      && bitRead(MayProgramAftertouch, i) ) {
        CvChannels[i] = Channel;
        CvStates[i] = AFTERTOUCH;
      }
    }
  } else {
    for(uint8_t i = 0; i < 4; i++){
      if(CvChannels[i] == Channel
      && CvStates[i] == AFTERTOUCH){
        Serial.println("ChanAT");
        Serial.flush();
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[AFTERTOUCH], PressureValue << 5);
      }
    }
  }
}

void RecievedNotePressure(uint8_t Channel, uint8_t NoteNumber, uint8_t PressureValue)
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
  for (uint8_t i = 0; i < 4; i++) {
    if (bitRead(ButtonsPushed, i)
    && bitRead(MayProgramAftertouch, i)) {
      CvChannels[i] = Channel;
      CvStates[i] = AFTERTOUCH;
      if ( GateStates[i] == PERCGATE ) {
        CvStates[i] = PERCAFTERTOUCH;
      }
    }
    if ( CvChannels[i] == Channel
    && ( ( CvStates[i] == AFTERTOUCH && CurrentValuePlayed[AFTERTOUCH][i] == NoteNumber )
    || ( CvStates[i] == PERCAFTERTOUCH && DrumGateNoteAfter[i] == NoteNumber ) ) ) {
        Serial.println("NoteAT");
        Serial.flush();
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[AFTERTOUCH], PressureValue << 5);
    }
  }
}

void RecievedCC(uint8_t Channel, uint8_t CcNumber, uint8_t CcValue)
{
  static uint8_t Nrpn_Msb_chans[4];
  static uint8_t Nrpn_Msb_nums[4];
  static uint8_t Nrpn_Msb_vals[4];
  static bool MSB_READY[4];
  
  static uint8_t Nrpn_Lsb_chans[4];
  static uint8_t Nrpn_Lsb_nums[4];
  static uint8_t Nrpn_Lsb_vals[4];
  static bool LSB_READY[4];

  static bool CCisOK[4];
  static uint8_t CvNumbers[4];

  if (CcNumber == 99) {  //NRPN MSB
    for (uint8_t i = 0; i < 4; i++) {
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
    for (uint8_t i = 0; i < 4; i++) {
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
    for (uint8_t i = 0; i < 4; i++) {  //Scanned voor ingedrukte knop, en koppelt CC als knop is ingedrukt.
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
//          if(i < 2) FormMsbLsb(DAC_1, i, 1, CcValue<<4);
//          else FormMsbLsb(DAC_2, i-2, 1, CcValue<<4);
        }
      }
    }
  }
}

void RecievedPitchBend(uint8_t Channel, int Bend)
{
  if(Bend >= -8192 && Bend <= 8191){
    for(uint8_t i = 0; i < 4; i++){
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

void FormMsbLsb(uint8_t Dac[2], uint8_t gain, unsigned int mV)
{
  bool Pin = 0;
  bool Chan = 1;
  uint8_t Msb = Dac[Chan] | gain | highByte(mV);
  uint8_t Lsb = lowByte(mV);
  
  SendSpi(Dac[Pin], Msb, Lsb);
}

void SendSpi(uint8_t dacpin, uint8_t Msb, uint8_t Lsb)
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
  for(uint8_t i = 0; i < 4; i++){
    for(uint8_t n = 0; n < SizeofRow_NoteNumbers; n++){ //scans for holded NoteNumber.
      NoteNumbers[i][n] = NOTHING;
    }
    digitalWrite(LOGIC_OUTPUTS[i+2], LOW);
  }
}

void RecievedActive(void)
{
}
