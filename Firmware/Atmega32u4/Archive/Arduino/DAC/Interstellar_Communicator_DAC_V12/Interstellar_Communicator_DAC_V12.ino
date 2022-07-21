/*
20-4-19
Jason

Gebruik voor VelocityValue en aftertouch gewoon de NoteChanVoiceType. Want poly VelocityValue en aftertouch zonder poly NoteNumber programmed slaat nergens op.
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
#define BUTTON_1 18
#define BUTTON_2 19
#define BUTTON_3 20
#define BUTTON_4 21

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
const uint8_t DAC_GAIN[7] = {
  FULLGAIN, //KEYS
  FULLGAIN, //VEL
  HALFGAIN, //PB
  HALFGAIN, //AT
  FULLGAIN, //PERCVEL
  HALFGAIN, //PERAT
  HALFGAIN //CC
};

const int8_t UNDEFINEDVOICE = -1;
const int8_t MONOVOICE = 0;
const int8_t DUOVOICE = 1;
const int8_t TRIVOICE = 2;
const int8_t QUADVOICE = 3;

const uint8_t KEYS = 0;
const uint8_t VELOCITY = 1;
const uint8_t PITCHBEND = 2;
const uint8_t AFTERTOUCH = 3;
const uint8_t PERCVELOCITY = 4;
const uint8_t PERCAFTERTOUCH = 5;
//const uint8_t POLYAFTERTOUCH = 5;
const uint8_t PERCTRIGGER = 6;
const uint8_t PERCGATE = 7;
const uint8_t CONTROLCHANGE = 8;

bool SpiTransactionEnded = true;



bool LearnMode = false;

bool ButtonPushed[4];

uint8_t Logic_outputs[6] = {CLOCK, RESET, GT1, GT2, GT3, GT4};

int8_t ButtonWithKey[17] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

bool maycheckpolyRecievedNoteOn = true;
bool maycheckpolyVelocityValue = true;
bool maycheckpolyaftertouch = true;

bool MayPlayNote;
int8_t NoteToBePLayed = -1;

int8_t CVrows_Chan_State_PolyType_PolyAddRow[4][3];

int8_t NoteChanVoiceType[17];
int8_t VelChanVoiceType[17];
int8_t AfterChanVoiceType[17];

int8_t CurrentNotePlayed[4] = {-1, -1, -1, -1};

uint8_t PolyNoteonCount[17][4];
uint8_t PolyVelocityCount[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t PolyAftertouchCount[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

uint8_t Firstpoly_at_button[17];

int8_t aftertouchButton[4] = {-1, -1, -1, -1};
bool set_firstpoly[17];
bool set_firstafter[17];
int8_t learn_NoteNumbers_count;

bool play_vel_onetime[17];

uint8_t CvChannels[4];
int8_t CvStates[4] = {-1, -1, -1, -1};
uint8_t CvAfterNote[4];

uint8_t GateChannels[4];
uint8_t GateStates[4];
uint8_t GateNotes[4];

bool PlayDrumTrigVel[4] = {false, false, false, false};

bool MayProgramAftertouch[4];
int8_t DrumGatePolyAfter[4] = {-1, -1, -1, -1};

uint8_t play_next_aftertouch[4];
      
unsigned long timerMIDIrecieved;

int8_t PolyNotes[10];
uint8_t SizeofPolyNotes;
int8_t NoteNumbers[4][10]; //Alle NoteNumbern hoger dan -1 in deze list zijn ingedrukt.
uint8_t SizeofNoteNumbers;

uint8_t clk_stop = 0;
uint8_t clk_start = 1;
uint8_t clk_continue = 2;
uint8_t clk_state = clk_stop;

uint8_t clock_count; //Telt de ontvangen midi clock messages.

unsigned long timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
unsigned long Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

MIDI_CREATE_DEFAULT_INSTANCE(); 

void setup()
{
  setinputsandoutputs();
  configuremidisethandle();
  setNoteNumbers();
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

void setNoteNumbers()
{
  SizeofNoteNumbers = sizeof(NoteNumbers[0]);
  for(uint8_t i = 0; i < 4; i++){ //Zet de alle waardes in de NoteNumbers op -1. Dit betekent geen noot ingedrukt.
    for(uint8_t n = 0; n < SizeofNoteNumbers; n++){
      NoteNumbers[i][n] = -1;
    }
  }
  SizeofPolyNotes = sizeof(PolyNotes);
  for ( uint8_t n = 0; n < SizeofPolyNotes; n++ ) {
    PolyNotes[n] = -1;
  }
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
  uint8_t NumButtonsPushed = 0;
  for(uint8_t i = 0; i < 4; i++){
    static const uint8_t BUTTONS[4] = {BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4};
    ButtonPushed[i] = !digitalRead(BUTTONS[i]);
    if ( ButtonPushed[i] ) {
      NumButtonsPushed++;
    } else {
      MayProgramAftertouch[i] = false;
    }
  }
  if( NumButtonsPushed == 0 && LearnMode == true ){
    learn_NoteNumbers_count = 0;
    CheckForVoiceTypes();
    LearnMode = false;
  } else if ( NumButtonsPushed > 0 && LearnMode == false){
    LearnMode = true;
  }
}

void CheckForVoiceTypes()
{
  SetChanVoiceType(NoteChanVoiceType, KEYS);
  SetChanVoiceType(VelChanVoiceType, VELOCITY);
  SetChanVoiceType(AfterChanVoiceType, AFTERTOUCH);
}

void SetChanVoiceType(int8_t ChanVoiceType[17], uint8_t CvState)
{
  for (byte i = 0; i < 17; i++) {
    ChanVoiceType[i] = UNDEFINEDVOICE;
  }
  CountChanVoiceTypeIfCvStateIsProgrammed(NoteChanVoiceType, CvState);
}

void CountChanVoiceTypeIfCvStateIsProgrammed(int8_t ChanVoiceType[17], uint8_t CvState)
{
  for (byte i = 0; i < sizeof(CvStates); i++) {
    if (CvStates[i] == CvState) {
      ChanVoiceType[CvChannels[i]]++;
      Serial.println(ChanVoiceType[CvChannels[i]]);
    }
  }
}

void CheckOutputTimers()
{
  for(uint8_t i = 0; i < 6; i++){
    if(Output_timers[i] > 0 && millis() - Output_timers[i] > 10){
      digitalWrite(Logic_outputs[i], LOW);  // Set triggers low 10 msec after HIGH.
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
    if(ButtonPushed[i]){
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
          MayProgramAftertouch[i] = true;
          DrumGatePolyAfter[i] = NoteNumber;
        }
      } else if(learn_loop_count == 1){
        if(learn_NoteNumbers_count == 0) { // Monophonic keys + [VelocityValue]
          CvChannels[i] = Channel;
          CvStates[i] = VELOCITY;
        }
      } else if(learn_loop_count == 2){
        if(learn_NoteNumbers_count == 0){ // Monophonic keys + VelocityValue + [poly/Channel aftertouch]
          MayProgramAftertouch[i] = true;
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
  MayPlayNote = true;
  for(uint8_t i = 0; i < 4; i++){
    if (GateChannels[i] == Channel && GateNotes[i] == NoteNumber) {
      if (GateStates[i] == PERCGATE) {
        digitalWrite(Logic_outputs[i+2], HIGH);
        DrumGatePolyAfter[i] = NoteNumber;
      }
      if (GateStates[i] == PERCTRIGGER) {
        digitalWrite(Logic_outputs[i+2], HIGH);
        Output_timers[i+2] = millis();
        PlayDrumTrigVel[i] = true;
      }
    }
    if (CvChannels[i] == Channel) {
      if ( CvStates[i] == KEYS ) {
        if ( NoteChanVoiceType[Channel] > MONOVOICE && MayPlayNote ) {
          Serial.print(String(i)+" ");
          if ( CurrentNotePlayed[i] == -1 ) {
            PlayNote(NoteNumber, i);
            CurrentNotePlayed[i] = NoteNumber;
            MayPlayNote = false;
          } else if ( i == NoteChanVoiceType[Channel] ) {
            Serial.print("CurNote:"+String(CurrentNotePlayed[i]));
            FillPolyNotes(NoteNumber);
          } else {
            Serial.print("CurNote:"+String(CurrentNotePlayed[i]));
          }
          Serial.println();
        } else if ( NoteChanVoiceType[Channel] ) {
          Serial.println(String(i)+" MPN:"+String(MayPlayNote));
        } else if ( NoteChanVoiceType[Channel] == MONOVOICE ) {
          for (uint8_t n = 0; n < SizeofNoteNumbers; n++) {
            if (NoteNumbers[i][n] == -1) {
              NoteNumbers[i][n] = NoteNumber;
              break;
            }
          }
          PlayNote(NoteNumber, i);
        }
        ButtonWithKey[Channel] = i;
      }
      else if ( CvStates[i] == VELOCITY && VelChanVoiceType > MONOVOICE ) {
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[VELOCITY], VelocityValue << 5); 
      }
      else if ( CvStates[i] == PERCVELOCITY && PlayDrumTrigVel[i] ) {
        FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[PERCVELOCITY], VelocityValue << 5); 
      }
      else if (CvStates[i] == AFTERTOUCH
      && PolyAftertouchCount[Channel] -1 == i) {
        play_next_aftertouch[i] = NoteNumbers[ButtonWithKey[Channel]][0];
        ButtonWithKey[Channel] = -1;
      }
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
  MayPlayNote = true;
  for (uint8_t i = 0; i < 4; i++) {
    if (GateChannels[i] == Channel
    && GateStates[i] == PERCGATE
    && GateNotes[i] == NoteNumber) {
      digitalWrite(Logic_outputs[i+2], LOW);
    }
    if (CvChannels[i] == Channel
    && CvStates[i] == KEYS) {
      if ( NoteChanVoiceType[Channel] > MONOVOICE && MayPlayNote ) {
        Serial.print(String(i)+" ");
        if ( CurrentNotePlayed[i] == NoteNumber ) {
          if ( PolyNotes[0] == -1 ) {
            CurrentNotePlayed[i] = -1;
//            Serial.print("CurNote:"+String(CurrentNotePlayed[i]));
            CheckGateStatesForKey(i);
          } else {
            PlayNote(PolyNotes[0], i);
            CurrentNotePlayed[i] = PolyNotes[0];
            MayPlayNote = false;
            ShiftPolyNotes(0);
          }
        } else if ( i == NoteChanVoiceType[Channel] ) {
          FindNoteInPolyNotes(NoteNumber);
        }
        Serial.println();
      } else if ( NoteChanVoiceType[Channel] == MONOVOICE ) {
        int8_t LastNote = -1;
        for (uint8_t n = 0; n < SizeofNoteNumbers; n++) {
          if (NoteNumbers[i][n] == NoteNumber
          || NoteNumbers[i][n] == -1) {
            if (n < SizeofNoteNumbers - 1) {             
              if(NoteNumbers[i][n + 1] >= 0){  // Shifts all NoteNumbers to the left where the NoteNumber was removed.
                NoteNumbers[i][n] = NoteNumbers[i][n+1];
                NoteNumbers[i][n + 1] = -1;
              } else if ( NoteNumbers[i][n+1] == -1 ){
                NoteNumbers[i][n] = -1;
                if(n == 0) {
                  LastNote = -1; // If the current NoteNumber is the first in the array. Then there are no NoteNumbers on.
                } else {
                  LastNote = NoteNumbers[i][n-1]; //  When the next NoteNumber in the array is also -1, the last NoteNumber will be the NoteNumber before the current one.
                }
                break;
              }
            } else if(n == SizeofNoteNumbers - 1){ // If the NoteNumber on place 19 in the array is the OFF NoteNumber. Then this will be removed and the last NoteNumber will be the NoteNumber before this.
              NoteNumbers[i][n] = -1;
              LastNote = NoteNumbers[i][n-1];
            }
          }
        }
        if ( LastNote == -1 ) {
          CheckGateStatesForKey(i);
        } else {
          PlayNote(LastNote, i);
        }
      }
    }
  }
  Serial.println();
}

void FillPolyNotes(uint8_t NoteNumber)
{
  Serial.print(" Poly: ");
  for ( uint8_t n = 0; n < SizeofPolyNotes; n++ ) {
    if ( PolyNotes[n] == -1 ) {
      PolyNotes[n] = NoteNumber;
      Serial.print(String(PolyNotes[n])+" ");
      break;
    }
    Serial.print(String(PolyNotes[n])+" ");
  }
}

void CheckGateStatesForKey(uint8_t i)
{
  if ( GateStates[i] == KEYS ) {
    digitalWrite(Logic_outputs[i+2], LOW);
  }
}

void PlayNote(uint8_t NoteNumber, uint8_t i)
{
  Serial.print("Play:"+String(NoteNumber));
  FormMsbLsb(DAC_PIN_CHAN[i], DAC_GAIN[KEYS], NoteNumber << 5); 
  if (GateStates[i] == KEYS) {
    digitalWrite(Logic_outputs[i+2], HIGH);
  }
}

void FindNoteInPolyNotes(uint8_t NoteNumber)
{
  for ( uint8_t n = 0; n < SizeofPolyNotes; n++ ) {
    if ( PolyNotes[n] == NoteNumber ) {
      ShiftPolyNotes(n);
      break;
    }
  }
}

void ShiftPolyNotes(uint8_t n)
{
  Serial.print(" Poly: ");
  for ( n; n < SizeofPolyNotes; n++ ) {
    if ( (n == SizeofPolyNotes - 1) ) {
      PolyNotes[n] = -1;
    } else if ( PolyNotes[n+1] == -1 ) {
      PolyNotes[n] = -1;
      Serial.print(String(PolyNotes[n])+" ");
      break;
    } else {
      PolyNotes[n] = PolyNotes[n+1];
    }
    Serial.print(String(PolyNotes[n])+" ");
  }
}

void RecievedChannelPressure(uint8_t Channel, uint8_t PressureValue)
{
  if ( LearnMode ) {
//  Serial.println("RecievedChannelPressure");
//  Serial.flush();
    for(uint8_t i = 0; i < 4; i++){
      if ( ButtonPushed[i]
      && MayProgramAftertouch[i] ) {
        CvChannels[i] = Channel;
        CvStates[i] = AFTERTOUCH;
      }
    }
  } else {
    for(uint8_t i = 0; i < 4; i++){
      if(CvChannels[i] == Channel
      && CvStates[i] == AFTERTOUCH){
  //      Serial.println("playRecievedChannelPressure");
  //      Serial.flush();
  //      if(i < 2) FormMsbLsb(DAC_1, i, 1, PressureValue<<4);
  //      else FormMsbLsb(DAC_2, i-2, 1, PressureValue<<4);
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
    if (ButtonPushed[i]
    && MayProgramAftertouch[i]) {
      CvChannels[i] = Channel;
      CvStates[i] = AFTERTOUCH;
      if ( GateStates[i] == PERCGATE ) {
        CvStates[i] = PERCAFTERTOUCH;
      }
    }
    if ( CvChannels[i] == Channel
    && ( ( CvStates[i] == AFTERTOUCH && play_next_aftertouch[i] == NoteNumber )
    || ( CvStates[i] == PERCAFTERTOUCH && DrumGatePolyAfter[i] == NoteNumber ) ) ) {
//        Serial.println("playRecievedNotePressure");
//        Serial.flush();
//      if (i < 2) FormMsbLsb(DAC_1, i, 1, PressureValue << 4);
//      else FormMsbLsb(DAC_2, i - 2, 1, PressureValue << 4);
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
      if (ButtonPushed[i]) {
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
        if (ButtonPushed[i]) {
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
        if(ButtonPushed[i]){
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
      if(ButtonPushed[i]){
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
  digitalWrite(Logic_outputs[1], HIGH);
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
    for(uint8_t n = 0; n < SizeofNoteNumbers; n++){ //scans for holded NoteNumber.
      NoteNumbers[i][n] = -1;
    }
    digitalWrite(Logic_outputs[i+2], LOW);
  }
}

void RecievedActive(void)
{
}
