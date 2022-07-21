/*
16-4-19
Jason
Geen interrupts voor de knoppen. Want meerde knopindrukkingen tegelijk moeten ook geregistreerd worden.

Als note afspelen mag, herschik de notelist's dan.
*/

#include <MIDI.h>

#include <SPI.h>

#define CLOCK 2
#define RESET 3
#define GT1  4
#define GT2  5
#define GT3  6
#define GT4  7
#define DAC1  8
#define DAC2  9
#define BUTTON_1 18
#define BUTTON_2 19
#define BUTTON_3 20
#define BUTTON_4 21

//const uint8_t DAC[2] = {8,9};

bool button_states[4];

uint8_t Logic_outputs[6] = {CLOCK, RESET, GT1, GT2, GT3, GT4};

int8_t ButtonWithKey[17] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

bool maycheckpolynoteon = true;
bool maycheckpolyvelocity = true;
bool maycheckpolyaftertouch = true;

bool MayPlayNote;

int8_t CVrows_Chan_State_PolyType_PolyAddRow[4][3];
int8_t ChannelIsPoly[17];
bool RowAlreadyPlayed[4] = {0, 0, 0, 0};

int8_t PolyNoteonType[17];
int8_t PolyVelocityType[17];
int8_t PolyAftertouchType[17];

uint8_t LastPolyNoteonAtButton[17];
uint8_t LastPolyVelocityAtButton[17];
uint8_t LastPolyAftertouchAtButton[17];

uint8_t PolyNoteonCount[17][4];
uint8_t PolyVelocityCount[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t PolyAftertouchCount[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

uint8_t Firstpoly_at_button[17];

int8_t aftertouchButton[4] = {-1, -1, -1, -1};
bool set_firstpoly[17];
bool set_firstafter[17];
int8_t learn_notes_count;

bool play_vel_onetime[17];

uint8_t CV_channels[4];
uint8_t CV_states[4];
uint8_t CV_AfterNote[4];
const uint8_t CVisKey = 1;
const uint8_t CVisVel = 2;
const uint8_t CVisPB = 3;
const uint8_t CVisAfter = 4;
const uint8_t CVisDrumVel = 5;
const uint8_t CVisDrumAfter = 6;
//const uint8_t CVisPolyAfter = 5;

uint8_t Gate_channels[4];
uint8_t Gate_states[4];
uint8_t Gate_notes[4];
const uint8_t GateisTrig = 1;
const uint8_t GateisKey = 2;
const uint8_t GateisDrumGate = 3;

bool PlayDrumTrigVel[4] = {false, false, false, false};

bool program_aftertouch[4];
int8_t DrumGatePolyAfter[4] = {-1, -1, -1, -1};

uint8_t play_next_aftertouch[4];
      
unsigned long timerMIDIrecieved;

int8_t notelist[4][20]; //Alle noten hoger dan -1 in deze list zijn ingedrukt.
uint8_t LengthNotelist;

uint8_t clk_stop = 0;
uint8_t clk_start = 1;
uint8_t clk_continue = 2;
uint8_t clk_state = clk_stop;

uint8_t clock_count; //Telt de ontvangen midi clock messages.

unsigned long timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
unsigned long Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

MIDI_CREATE_DEFAULT_INSTANCE(); 

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
  pinMode(DAC1, OUTPUT); //dac 1 channel 1 and 2
  pinMode(DAC2, OUTPUT); //dac 2 channel 1 and 2
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
  digitalWrite(DAC1,HIGH);
  digitalWrite(DAC2,HIGH);

  //Sets the DAC outputs on lowest output, which translates to 0V. Which is note C-1 and midi note 0.
  setVoltage(DAC1, 0, 1, 0);
  setVoltage(DAC1, 1, 1, 0);
  setVoltage(DAC2, 0, 1, 0);
  setVoltage(DAC2, 1, 1, 0);
}

void configuremidisethandle()
{
  //When the following midi message type is recieved, the void function in between the brackets is carried out.
  MIDI.setHandleNoteOn(noteon);
  MIDI.setHandleNoteOff(noteoff);
  MIDI.setHandleControlChange(controlchange);
  MIDI.setHandlePitchBend(pitchbend);
  MIDI.setHandleAfterTouchChannel(chanafter);
  MIDI.setHandleAfterTouchPoly(afterpoly);
  MIDI.setHandleClock(recieveclock);
  MIDI.setHandleStart(recievestart);
  MIDI.setHandleContinue(recievecontinue);
  MIDI.setHandleStop(recievestop);
  MIDI.setHandleActiveSensing(active);
}

void setnotelists()
{
  LengthNotelist = sizeof(notelist[0]);
  for(uint8_t i = 0; i < 4; i++){ //Zet de alle waardes in de notelist op -1. Dit betekent geen noot ingedrukt.
    for(uint8_t n = 0; n < LengthNotelist; n++){
      notelist[i][n] = -1;
    }
  }
}

void setup()
{
  setinputsandoutputs();
  configuremidisethandle();
  setnotelists();
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
  }
}

void if_LastMidiLongAgo_StopClock_and_TurnNotesOff()
{
  if(millis() - timerMIDIrecieved > 500){
    timerMIDIrecieved = millis();
    recievestop();
  }
}

void ReadButtonPins()
{
  for(uint8_t i = 0; i < 4; i++){
    static uint8_t button_list[4] = {BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4};
    button_states[i] = !digitalRead(button_list[i]);
    if (!button_states[i]) program_aftertouch[i] = false;
  }
  if(!button_states[0] && !button_states[1] && !button_states[2] && !button_states[3]){
    learn_notes_count = 0;
    if (maycheckpolynoteon) checkpolynoteon();
    if (maycheckpolyvelocity) checkpolyvelocity();
    if (maycheckpolyaftertouch) checkpolyaftertouch();
  }
  else {
    maycheckpolynoteon = true;
    maycheckpolyvelocity = true;
    maycheckpolyaftertouch = true;
  }
}

void checkpolynoteon()
{
  maycheckpolynoteon = false;
  for (byte i = 0; i < 17; i++) {
//    PolyNoteonType[i][j] = 0;
//    PolyNoteonCount[i][j] = 0;
    ChannelIsPoly[i] = -1;
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisKey) {
      ChannelIsPoly[CV_channels[i]]++;
      LastPolyNoteonAtButton[CV_channels[i]] = i;
      PolyNoteonType[CV_channels[i]]++;
    }
  }
}

void checkpolyvelocity()
{
  maycheckpolyvelocity = false;
  for (byte i = 0; i < 17; i++) {
    PolyVelocityType[i] = 0;
    PolyVelocityCount[i] = 0;
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisVel) {
      if ( PolyVelocityCount[CV_channels[i]] == 0 ) PolyVelocityCount[CV_channels[i]] = i + 1;
      LastPolyVelocityAtButton[CV_channels[i]] = i;
      PolyVelocityType[CV_channels[i]]++;
    }
  }
}

void checkpolyaftertouch()
{
  maycheckpolyaftertouch = false;
  for (byte i = 0; i < 17; i++) {
    PolyAftertouchType[i] = 0;
    PolyAftertouchCount[i] = 0;
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisAfter) {
      if ( PolyAftertouchCount[CV_channels[i]] == 0 ) PolyAftertouchCount[CV_channels[i]] = i + 1;
      LastPolyAftertouchAtButton[CV_channels[i]] = i;
      PolyAftertouchType[CV_channels[i]]++;
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

void noteon(uint8_t channel, uint8_t note, uint8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
  LearnNoteOn(channel, note, velocity);
  Check_and_PlayNotes(channel, note, velocity);
}

void LearnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
  uint8_t learn_loop_count = 0;
  for(uint8_t i = 0; i < 4; i++){
    if(button_states[i]){
      if(learn_loop_count == 0){
        if(learn_notes_count == 0){ // Monophonic keys.
          CV_channels[i] = channel;
          CV_states[i] = CVisKey;
          Gate_states[i] = GateisKey;
          Serial.print("learned");
        }
        else if(learn_notes_count == 1){ // Drum Trigger and velocity
          Gate_channels[i] = channel;
          Gate_notes[i] = note;
          Gate_states[i] = GateisTrig;

          CV_channels[i] = channel;
          CV_states[i] = CVisDrumVel;
        }
        else if(learn_notes_count == 2){ // Drum gate and aftertouch
          Gate_states[i] = GateisDrumGate;
          program_aftertouch[i] = true;
          DrumGatePolyAfter[i] = note;
        }
      }
      else if(learn_loop_count == 1){
        if(learn_notes_count == 0) { // Monophonic keys + [velocity]
          CV_channels[i] = channel;
          CV_states[i] = CVisVel;
        }
      }
      else if(learn_loop_count == 2){
        if(learn_notes_count == 0){ // Monophonic keys + velocity + [poly/channel aftertouch]
          program_aftertouch[i] = true;
        }
      }
      learn_loop_count++;
    }
  }
  learn_notes_count++;
}

void Check_and_PlayNotes(uint8_t channel, uint8_t note, uint8_t velocity)
{
  MayPlayNote = true;
  Serial.println(note);
  for(uint8_t i = 0; i < 4; i++){
    if (Gate_channels[i] == channel && Gate_notes[i] == note) {
      if (Gate_states[i] == GateisDrumGate) {
        digitalWrite(Logic_outputs[i+2], HIGH);
        DrumGatePolyAfter[i] = note;
      }
      if (Gate_states[i] == GateisTrig) {
        digitalWrite(Logic_outputs[i+2], HIGH);
        Output_timers[i+2] = millis();
        PlayDrumTrigVel[i] = true;
      }
    }
    if (CV_channels[i] == channel) {
      if ( CV_states[i] == CVisKey ) {
        for (uint8_t n = 0; n < LengthNotelist; n++) {
          if (notelist[i][n] == -1) {
           notelist[i][n] = note;
           break;
          }
        }
        PlayOrNotPlayANote(channel, note, i);
        ButtonWithKey[channel] = i;
      }
      else if ( CV_states[i] == CVisVel && PolyVelocityCount[channel] - 1 == i ) {
        if (i < 2) {
          setVoltage(DAC1, i, 1, velocity<<5);
        }
        else {
          setVoltage(DAC2, i-2, 1, velocity<<5);
        }
      }
      else if ( CV_states[i] == CVisDrumVel && PlayDrumTrigVel[i] ) {
        if (i < 2) {
          setVoltage(DAC1, i, 1, velocity<<5);
        }
        else {
          setVoltage(DAC2, i-2, 1, velocity<<5);
        }
      }
      else if (CV_states[i] == CVisAfter
      && PolyAftertouchCount[channel] -1 == i) {
        play_next_aftertouch[i] = notelist[ButtonWithKey[channel]][0];
        ButtonWithKey[channel] = -1;
      }
    }
    PlayDrumTrigVel[i] = false;
  }
}

void noteoff(uint8_t channel, uint8_t note, uint8_t velocity)
{
  MayPlayNote = true;
//  if (PolyNoteonCount[channel] > 1) {
//    PolyNoteonCount[channel]--;
//  }
  if (PolyVelocityCount[channel] > 1) {
    PolyVelocityCount[channel]--;
  }
  if (PolyAftertouchCount[channel] > 1) {
    PolyAftertouchCount[channel]--;
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (Gate_channels[i] == channel
    && Gate_states[i] == GateisDrumGate
    && Gate_notes[i] == note) digitalWrite(Logic_outputs[i+2], LOW);
    if (CV_channels[i] == channel
    && CV_states[i] == CVisKey) {
//        int8_t maxnote = -1;
      int8_t lastnote = -1;
      if ( note == notelist[i][0] ) RowAlreadyPlayed[i] = false;
      for (uint8_t n = 0; n < LengthNotelist; n++) {
        if (notelist[i][n] == note
        || notelist[i][n] == -1) {
          if (n < LengthNotelist - 1) {
//              maxnote = max(maxnote, notelist[i][n]);              
            if(notelist[i][n + 1] >= 0){  // Shifts all notes to the left where the note was removed.
              notelist[i][n] = notelist[i][n+1];
              notelist[i][n + 1] = -1;
            }
            else if(notelist[i][n+1] == -1){
              notelist[i][n] = -1;
              if(n == 0) lastnote = -1; // If the current note is the first in the array. Then there are no notes on.
              else lastnote = notelist[i][n-1]; //  When the next note in the array is also -1, the last note will be the note before the current one.
              break;
            }
          }
          else if(n == LengthNotelist - 1){ // If the note on place 19 in the array is the OFF note. Then this will be removed and the last note will be the note before this.
            notelist[i][n] = -1;
            lastnote = notelist[i][n-1];
          }
        }
      }
//        if(maxnote == -1){
      if(lastnote == -1){
        if(Gate_states[i] == GateisKey) digitalWrite(Logic_outputs[i+2], LOW);
      }
      else PlayOrNotPlayANote(channel, lastnote, i);
//      else if(i < 2) {
//        setVoltage(DAC1, i, 1, lastnote<<5);
//      }
//      else {
//        setVoltage(DAC2, i-2, 1, lastnote<<5);
//      }
    }
  }
}

void PlayOrNotPlayANote(uint8_t channel, uint8_t note, uint8_t i)
{
  if ( ( !RowAlreadyPlayed[i] && MayPlayNote ) || !ChannelIsPoly[channel] ) {
    if (i < 2) setVoltage(DAC1, i, 1, note<<5);
    else {
      setVoltage(DAC2, i-2, 1, note<<5);
    }
    if (Gate_states[i] == GateisKey) {
      Serial.println(RowAlreadyPlayed[i]);
      Serial.println(ChannelIsPoly[channel]);
      Serial.println("played");
      digitalWrite(Logic_outputs[i+2], HIGH);
    }
    RowAlreadyPlayed[i] = true;
    MayPlayNote = false;
  }
}

void chanafter(uint8_t channel, uint8_t pressure)
{
//  Serial.println("chanafter");
//  Serial.flush();
  for(uint8_t i = 0; i < 4; i++){
    if(button_states[i]
    && program_aftertouch[i]){
      CV_channels[i] = channel;
      CV_states[i] = CVisAfter;
    }
    if(CV_channels[i] == channel
    && CV_states[i] == CVisAfter){
//      Serial.println("playchanafter");
//      Serial.flush();
      if(i < 2) setVoltage(DAC1, i, 1, pressure<<4);
      else setVoltage(DAC2, i-2, 1, pressure<<4);
    }
  }
}

void afterpoly(uint8_t channel, uint8_t note, uint8_t pressure)
{
//  Serial.print("playnextafter ");
//  Serial.println(play_next_aftertouch[i]);
//  Serial.flush();
//  Serial.print("PolyAftertouchCount ");
//  Serial.println(PolyAftertouchCount[channel]);
//  Serial.flush();

//  Serial.print("note ");
//  Serial.println(note);
//  Serial.flush();
  for (uint8_t i = 0; i < 4; i++) {
    if (button_states[i]
    && program_aftertouch[i]) {
      CV_channels[i] = channel;
      CV_states[i] = CVisAfter;
      if ( Gate_states[i] == GateisDrumGate ) {
        CV_states[i] = CVisDrumAfter;
      }
    }
    if ( CV_channels[i] == channel
    && ( ( CV_states[i] == CVisAfter && play_next_aftertouch[i] == note )
    || ( CV_states[i] == CVisDrumAfter && DrumGatePolyAfter[i] == note ) ) ) {
//        Serial.println("playafterpoly");
//        Serial.flush();
      if (i < 2) setVoltage(DAC1, i, 1, pressure << 4);
      else setVoltage(DAC2, i - 2, 1, pressure << 4);
    }
  }
}

void controlchange(uint8_t channel, uint8_t number, uint8_t value)
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
  
  static uint8_t CV_numbers[4];
  uint8_t CVisCC = 3;

  if (number == 99) {  //NRPN MSB
    for (uint8_t i = 0; i < 4; i++) {
      if (button_states[i]) {
        Nrpn_Msb_chans[i] = channel;
        Nrpn_Msb_nums[i] = number;
        Nrpn_Msb_vals[i] = value;
      }
      if (Nrpn_Msb_chans[i] == channel
      && Nrpn_Msb_nums[i] == number
      && Nrpn_Msb_vals[i] == value) MSB_READY[i] = true;
      else MSB_READY[i] = false;
    }
  }
  else if (number == 98) { //NRPN LSB
    for (uint8_t i = 0; i < 4; i++) {
      if (MSB_READY[i]) {
        if (button_states[i]) {
          Nrpn_Lsb_chans[i] = channel;
          Nrpn_Lsb_nums[i] = number;
          Nrpn_Lsb_vals[i] = value;
        }
        if (Nrpn_Lsb_chans[i] == channel
        && Nrpn_Lsb_nums[i] == number
        && Nrpn_Lsb_vals[i] == value) LSB_READY[i] = true;
        else LSB_READY[i] = false;
      }
    }
  }
  else {
    for (uint8_t i = 0; i < 4; i++) {  //Scanned voor ingedrukte knop, en koppelt CC als knop is ingedrukt.
      if (number == 6) {  //Data Entry MSB
        if (MSB_READY[i]&& LSB_READY[i]) CCisOK[i] = true;
        else CCisOK[i] = false;
      }
      else CCisOK[i] = true;
      if (CCisOK[i]) {
        if(button_states[i]){
          CV_channels[i] = channel;
          CV_numbers[i] = number;
          CV_states[i] = CVisCC;
        }
        if(CV_channels[i] == channel
        && CV_numbers[i] == number
        && CV_states[i] == CVisCC){ //Stuur CC naar DAC. CC data is 7-bits. DAC is 12-bits. Bitshift van 4 naar links is 11 bits dus dat is de helft van de DAC output.
          if(i < 2) setVoltage(DAC1, i, 1, value<<4);
          else setVoltage(DAC2, i-2, 1, value<<4);
        }
      }
    }
  }
}

void pitchbend(uint8_t channel, int bend)
{
  static uint8_t CV_channels[4];
  if(bend >= -8192 && bend <= 8191){
    for(uint8_t i = 0; i < 4; i++){
      if(button_states[i]){
        CV_channels[i] = channel;
        CV_states[i] = CVisPB;
      }
      if(CV_channels[i] == channel && CV_states[i] == CVisPB){
        if(i < 2) setVoltage(DAC1, i, 0, (bend + 8192)>>2);  //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
        else setVoltage(DAC2, i-2, 0, (bend + 8192)>>2);
      }
    }
  }
}

void setVoltage(uint8_t dacpin, bool channel, bool gain, unsigned int mV)
{
  unsigned int command = channel ? 0x9000 : 0x1000;

  command |= gain ? 0x0000 : 0x2000;
  command |= (mV & 0x0FFF);
  
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(dacpin,LOW);
  SPI.transfer(command>>8);
  SPI.transfer(command&0xFF);
  digitalWrite(dacpin,HIGH);
  SPI.endTransaction();
}

void recieveclock(void)
{
  if(clock_count == 0 && (clk_state == clk_start || clk_state == clk_continue)) digitalWrite(CLOCK, HIGH); // Start clock pulse
  if(clk_state != clk_stop) clock_count++;
  if(clock_count >= 3) {  // MIDI timing clock sends 24 pulses per quarter note.  Sent pulse only once every 3 pulses (32th notes).
    digitalWrite(CLOCK, LOW);
    clock_count = 0;
  }
}

void recievestart(void)
{
  digitalWrite(Logic_outputs[1], HIGH);
  Output_timers[1] = millis();
  clock_count = 0;
  clk_state = clk_start;
}

void recievecontinue(void)
{
  clk_state = clk_continue;
}

void recievestop(void)
{
  clk_state = clk_stop;
  digitalWrite(CLOCK, LOW);
  for(uint8_t i = 0; i < 4; i++){
    for(uint8_t n = 0; n < LengthNotelist; n++){ //scans for holded note.
      notelist[i][n] = -1;
    }
    digitalWrite(Logic_outputs[i+2], LOW);
  }
}

void active(void)
{
}
