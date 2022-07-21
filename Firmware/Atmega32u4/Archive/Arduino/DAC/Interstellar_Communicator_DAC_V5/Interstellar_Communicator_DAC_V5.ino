/*
Aanpassingen van originele (elkayem) code:
30-4-19:
Midi-cable sensing toegepast.
Last note priority toegevoegd.
timerTrig toegevoegd.
timerRes toegevoegd.
Midi learn buttons toegevoegd.

27-3-19:
Alles vervangen door sethandle manier.

26-3-19:
Lastnote werking vervangen door eigen versie.

20-3-19:
Aangepast voor Atmega32u4

11-2-19:
Range C0 - C8 (bool notes list ook veranderd naar 97)
Pitchbend range RANGE veranderd naar +- 2 semitones.
Clock_timer weggehaald.
CLock LOW toegevoegd.
Trigger en RESET lengte veranderd anar 2 ms.

6-2-19:
Range C0 - C4

2-2-19:
Lowest/highest note priority weggehaalt.

31-1-19:
Trigger 1, 2, 3 en 4 assigned naar channel 10 noten F, F#, G en G# (41, 42, 43 en 44)
Gate, Note, Velocity, Pitchbend en CC naar channel 1 assigned.
TRIG en trigTimer weggehaald.
Clock start/continue/stop functies toegevoegd.
Clock RESET output toegevoegd.

Dec 2018:
1. Clock lengte van 20 milliseconden naar 1 milliseconden.
2. Clock frequentie van kwartnoten naar 32de noten.
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
#define knob_1 18
#define knob_2 19
#define knob_3 20
#define knob_4 21

uint8_t DACnums[2] = {DAC1, DAC2};
uint8_t DACchans[2] = {0, 1};

uint8_t Logic_outputs[6] = {CLOCK, RESET, GT1, GT2, GT3,GT4};

bool button_states[4];
uint8_t learn_pressed_keys = 0;
uint8_t learn_pressed_buttons = 0;
const uint8_t learn_combo_KeysMono = 1;
const uint8_t learn_combo_KeysMonoVel = 2;
const uint8_t learn_combo_KeysMonoVelAfter = 3;
const uint8_t learn_combo_DrumTrigVel = 4;
const uint8_t learn_combo_DrumGateAfter = 5;
const uint8_t learn_button_key_combos[3][3] =
{
  {learn_combo_KeysMono, learn_combo_DrumTrigVel, learn_combo_DrumGateAfter},
  {learn_combo_KeysMonoVel, 0, 0},
  {learn_combo_KeysMonoVelAfter, 0, 0}
};

// int8_t fir_but_push = -1;
// int8_t sec_but_push = -1;
// int8_t thi_but_push = -1;

 int8_t Poly_type[17];
 uint8_t Poly_count[17];
 uint8_t Last_poly[17];
// uint8_t Poly_count = 0;

uint8_t CV_channels[4];
uint8_t CV_states[4];
uint8_t CV_numbers[4];
uint8_t CVisKey = 1;
uint8_t CVisVel = 2;
uint8_t CVisPB = 4;
uint8_t CVisCC = 3;
uint8_t CVisDrumVel = 5;
uint8_t CVisAfter = 6;

uint8_t Gate_channels[4];
uint8_t Gate_notes[4];
uint8_t Gate_states[4];
uint8_t GateisTrig = 1;
uint8_t GateisKey = 2;
uint8_t GateisDrumGate = 3;

uint8_t Nrpn_Msb_chans[4];
uint8_t Nrpn_Msb_nums[4];
uint8_t Nrpn_Msb_vals[4];
bool MSB_READY[4];
uint8_t Nrpn_Lsb_chans[4];
uint8_t Nrpn_Lsb_nums[4];
uint8_t Nrpn_Lsb_vals[4];
bool LSB_READY[4];
bool CCisOK[4];

bool program_aftertouch[4];
bool play_aftertouch[4];
//uint8_t note_for_after_bot[4];
//uint8_t note_for_after_top[4];

unsigned long timerMIDIrecieve;

int8_t notelist[4][20]; //Alle noten hoger dan -1 in deze list zijn ingedrukt.

uint8_t clk_stop = 0;
uint8_t clk_start = 1;
uint8_t clk_continue = 2;
uint8_t clk_state = clk_stop; //Zet clock status op clock stopgezet bij reset.

uint8_t clk_count; //Telt de ontvangen midi clock messages.

unsigned long timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
unsigned long Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

MIDI_CREATE_DEFAULT_INSTANCE(); 

void setup() {
  pinMode(CLOCK, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(GT1, OUTPUT);
  pinMode(GT2, OUTPUT);
  pinMode(GT3, OUTPUT);
  pinMode(GT4, OUTPUT);
  pinMode(DAC1, OUTPUT); //dac 1 channel 1 and 2
  pinMode(DAC2, OUTPUT); //dac 2 channel 1 and 2
  pinMode(knob_1, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
  pinMode(knob_2, INPUT_PULLUP);
  pinMode(knob_3, INPUT_PULLUP);
  pinMode(knob_4, INPUT_PULLUP);
  digitalWrite(CLOCK,LOW);
  digitalWrite(RESET,LOW);
  digitalWrite(GT1,LOW);
  digitalWrite(GT2,LOW);
  digitalWrite(GT3,LOW);
  digitalWrite(GT4,LOW);
  digitalWrite(DAC1,HIGH);
  digitalWrite(DAC2,HIGH);
  
  SPI.begin();
  //Recieve all MIDI channels.
  
  MIDI.begin(MIDI_CHANNEL_OMNI);
  //When the following midi message type is recieved, the void function in between the brackets is carried out.
  MIDI.setHandleNoteOn(noteon);
  MIDI.setHandleNoteOff(noteoff);
  MIDI.setHandleControlChange(controlchange);
  MIDI.setHandlePitchBend(pitchbend);
  MIDI.setHandleAfterTouchChannel(afterchan);
  MIDI.setHandleAfterTouchPoly(afterpoly);
  MIDI.setHandleClock(recieveClock);
  MIDI.setHandleStart(recieveStart);
  MIDI.setHandleContinue(recieveContinue);
  MIDI.setHandleStop(recieveStop);
  MIDI.setHandleActiveSensing(active);
  
  //Sets the DAC outputs on lowest output, which translates to 0V. Which is note C-1 and midi note 0.
  for(uint8_t i; i<4; i++) setVoltage((i<2?DACnums[0]:DACnums[1]), (i<2?DACchans[i]:DACchans[i-2]), 1, 0);

  Serial.println(sizeof(notelist[0]));
  
  resetNoteArraysLogicOutputs();
}

void loop() {
  MIDI.read();
  if(MIDI.read()){
//    Serial.println("MIDI");
    timerMIDIrecieve = millis();
  }
  if(millis() - timerMIDIrecieve > 500){
//    Serial.println("NoMIDI");
    timerMIDIrecieve = millis();
    recieveStop();
  }
  OutputTimers();

  Serial.println("yes");
  delay(100);

//  if((!PINF << 4) == 0){
//    learn_pressed_keys = 0;
//  }
  checkButtons();
}

//setVoltage -- Set DAC voltage output
//dacpin: chip select pin for DAC.  Note and velocity on DAC1, pitch bend and CC on DAC2
//channel: 0 (A) or 1 (B).  Note and pitch bend on 0, velocity and CC on 2.
//gain: 0 = 1X, 1 = 2X.  
//mV: integer 0 to 4095.  If gain is 1X, mV is in units of half mV (i.e., 0 to 2048 mV).
//If gain is 2X, mV is in units of mV

void checkButtons() {
  for(uint8_t i = 0; i < 4; i++){    //  Schrijft continu de inverse status van de knoppen 1 t/m 4. Want knop ingedrukt geeft digitalRead() == false.
    uint8_t button_array[4] = {knob_1, knob_2, knob_3, knob_4};
    button_states[i] = !digitalRead(button_array[i]);
    if(!button_states[0] && !button_states[1] && !button_states[2] && !button_states[3]){
      learn_pressed_keys = 0;
      if(maycheckpolynotes) checkpolynotes();
    }
    else maycheckpolynotes = true;
  }
}

void OutputTimers()
{
  for(uint8_t i = 0; i < 6; i++){
    if(Output_timers[i] > 0 && millis() - Output_timers[i] > 10){
      digitalWrite(Logic_outputs[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
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

void learnNoteon(uint8_t channel, uint8_t note, uint8_t velocity)
{
  learn_pressed_buttons = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (button_states[i]) {
      switch (get_learn_button_key_combo()) {
        case learn_combo_KeysMono:
          CV_channels[i] = channel;
          CV_states[i] = CVisKey;
          Gate_states[i] = GateisKey;
        case learn_combo_KeysMonoVel:
          CV_channels[i] = channel;
          CV_states[i] = CVisVel;
        case learn_combo_KeysMonoVelAfter:
          program_aftertouch[i] = true;
        case learn_combo_DrumTrigVel:
          Gate_channels[i] = channel;
          Gate_notes[i] = note;
          Gate_states[i] = GateisTrig;
          // CVisDrumVel[i] = i;
          CV_channels[i] = channel;
          CV_states[i] = CVisDrumVel;
        case learn_combo_DrumGateAfter:
          Gate_states[i] = GateisDrumGate;
          CV_states[i] = CVisAfter;
        default:
          break;
      }
      learn_pressed_buttons++;
    }
    learn_pressed_keys++;
  }
}

int get_learn_button_key_combo()
{
  for (uint8_t i; i < sizeof(learn_button_key_combos); i++) {
    for (uint8_t j; j < sizeof(learn_button_key_combos[0]); j++) {
      if (learn_pressed_buttons == i && learn_pressed_keys == j ) return learn_button_key_combos[i][j];
    }
  }
}

void checkpolynotes()
{
  maycheckpolynotes = false;
  for (byte i = 0; i < 17; i++) {
    Poly_type[i] = -1;
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisKey) {
      Last_poly[CV_channels[i]] = i;
      Poly_type[CV_channels[i]]++;
    }
  }
}

void noteon(uint8_t channel, uint8_t note, uint8_t velocity)
{
  timerMIDIrecieve = millis();
//  if (!PINF << 4) learnNoteon(channel, note, velocity);
  for (uint8_t i = 0; i < 4; i++) {
    if (Gate_channels[i] == channel) {
      if (Gate_notes[i] == note) {
        if (Gate_states[i] == GateisDrumGate) digitalWrite(Logic_outputs[i+2], HIGH);
        if (Gate_states[i] == GateisTrig) {
          digitalWrite(Logic_outputs[i+2], HIGH);
          Output_timers[i+2] = millis();
        }
      }
    }
    if (CV_channels[i] == channel) {
      if (CV_states[i] == CVisKey) {
    //   if(Poly_count[i] == 0){
    //     Poly_count[i] = Poly_type[i];
    //     if(Poly_type[channel] == Last_poly[channel]) Poly_count[channel] = 1;
        for (uint8_t n = 0; n < sizeof(notelist[0]); n++) {
          if (notelist[i][n] == -1) {
            notelist[i][n] = note;
            break;
          }
        }
        if (i < 2) setVoltage(DAC1, i, 1, note<<5);
        else setVoltage(DAC2, i-2, 1, note<<5);
        if (Gate_states[i] == GateisKey) digitalWrite(Logic_outputs[i+2], HIGH);
    //   }
    //   if(Poly_count[channel] > 0) Poly_count[channel]--;
      }
      else if (CV_states[i] == CVisVel) {
        if (i < 2) setVoltage(DAC1, i, 1, velocity<<4);
        else setVoltage(DAC2, i-2, 1, velocity<<4);
      }
//        else if(CV_states[i] == CVisAT){
//          play_aftertouch[i] = true;
//        }
    }
  }
}

void noteoff(uint8_t channel, uint8_t note, uint8_t velocity)
{
  //Zoeken naar de MIDI off noot in de notelist.
  //Als deze gevonden wordt in de notelist dan word deze positie (i) op -1 gezet.
  //Vervolgens word de noot op de volgende positie gekopieerd naar de huidige positie
  timerMIDIrecieve = millis();
  for(uint8_t i = 0; i < 4; i++){
    if(Gate_channels[i] == channel && Gate_states[i] == GateisDrumGate && Gate_notes[i] == note) digitalWrite(Logic_outputs[i+2], LOW);
    if(CV_channels[i] == channel && CV_states[i] == CVisKey){
  //        int8_t maxnote = -1;
      int8_t lastnote = -1;
      for(uint8_t n = 0; n < sizeof(notelist[0]); n++){
        if(notelist[i][n] == note || notelist[i][n] == -1){
          if(n < 19){
  //              maxnote = max(maxnote, notelist[i][n]);              
            if(notelist[i][n+1] >= 0){  // Shifts all notes to the left where the note was removed.
                notelist[i][n] = notelist[i][n+1];
                notelist[i][n+1] = -1;
            }
            else if(notelist[i][n+1] == -1){
                notelist[i][n] = -1;
                if(n == 0) lastnote = -1; // If the current note is the first in the array. Then there are no notes on.
                else lastnote = notelist[i][n-1]; //  When the next note in the array is also -1, the last note will be the note before the current one.
                break;
            }
          }
          else if(n == 19){ // If the note on place 19 in the array is the OFF note. Then this will be removed and the last note will be the note before this.
              notelist[i][n] = -1;
              lastnote = notelist[i][n-1];
          }
        }
      }
  //        if(maxnote == -1){
      if(lastnote == -1){
          if(Gate_states[i] == GateisKey) digitalWrite(Logic_outputs[i+2], LOW);
      }
      else if(i < 2) setVoltage(DAC1, i, 1, lastnote<<5);
      else setVoltage(DAC2, i-2, 1, lastnote<<5);
    }
  }
}

void controlchange(uint8_t channel, uint8_t number, uint8_t value)
{
  timerMIDIrecieve = millis();
  if(number == 99){  //NRPN MSB
    for(uint8_t i = 0; i < 4; i++){
      if(button_states[i]){
        Nrpn_Msb_chans[i] = channel;
        Nrpn_Msb_nums[i] = number;
        Nrpn_Msb_vals[i] = value;
      }
      if(Nrpn_Msb_chans[i] == channel && Nrpn_Msb_nums[i] == number && Nrpn_Msb_vals[i] == value) MSB_READY[i] = true;
      else MSB_READY[i] = false;
    }
  }
  else if(number == 98){ //NRPN LSB
    for(uint8_t i = 0; i < 4; i++){
      if(MSB_READY[i]){
        if(button_states[i]){
          Nrpn_Lsb_chans[i] = channel;
          Nrpn_Lsb_nums[i] = number;
          Nrpn_Lsb_vals[i] = value;
        }
        if(Nrpn_Lsb_chans[i] == channel && Nrpn_Lsb_nums[i] == number && Nrpn_Lsb_vals[i] == value) LSB_READY[i] = true;
        else LSB_READY[i] = false;
      }
    }
  }
  else if(number >= 0 && number <= 127){
    for(uint8_t i = 0; i < 4; i++){  //Scanned voor ingedrukte knop, en koppelt CC als knop is ingedrukt.
      if(number == 6){  //Data Entry MSB
        if(MSB_READY[i] && LSB_READY[i]) CCisOK[i] = true;
        else CCisOK[i] = false;
      }
      else CCisOK[i] = true;
      if(CCisOK[i]){
        if(button_states[i]){
          CV_channels[i] = channel;
          CV_numbers[i] = number;
          CV_states[i] = CVisCC;
        }
        //Stuur CC naar DAC. CC data is 7-bits. DAC is 12-bits. Bitshift van 4 naar links is 11 bits dus dat is de helft van de DAC output.
        if(CV_channels[i] == channel && CV_numbers[i] == number && CV_states[i] == CVisCC) setVoltage((i<2?DACnums[0]:DACnums[1]), (i<2?DACchans[i]:DACchans[i-2]), 1, value<<4);
      }
    }
  }
}

void pitchbend(uint8_t channel, int bend)
{
  timerMIDIrecieve = millis();
  //  Serial.println("PB recieved");
  if(bend >= 0 && bend <= 16383){
    for(uint8_t i = 0; i < 4; i++){
      if(button_states[i]){
        CV_channels[i] = channel;
        CV_states[i] = CVisPB;
      }
      if(CV_channels[i] == channel && CV_states[i] == CVisPB){
        if(i < 2) setVoltage(DAC1, i, 1, bend>>2);  //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
        else setVoltage(DAC2, i-2, 1, bend>>2);
      }
    }
  }
}

void afterchan(uint8_t channel, uint8_t pressure)
{
  timerMIDIrecieve = millis();
}

void afterpoly(uint8_t channel, uint8_t note, uint8_t pressure)
{
  timerMIDIrecieve = millis();
}

void recieveClock(void)
{
  timerMIDIrecieve = millis();
  //  Serial.println("clock");
  if(clk_count == 0 && (clk_state == clk_start || clk_state == clk_continue)) digitalWrite(CLOCK, HIGH); // Start clock pulse
  if(clk_state != clk_stop) clk_count++;
  if(clk_count >= 3) {  // MIDI timing clock sends 24 pulses per quarter note.  Sent pulse only once every 3 pulses (32th notes).
    digitalWrite(CLOCK, LOW);
    clk_count = 0;
  }
}

void recieveStart(void)
{
  timerMIDIrecieve = millis();
  digitalWrite(RESET, HIGH);
  timerRes = millis();
  clk_count = 0;
  clk_state = clk_start;
  //  if(clk_state == clk_start) Serial.println(clk_start);
}

void recieveContinue(void)
{
  timerMIDIrecieve = millis();
  clk_state = clk_continue;
}

void recieveStop(void)
{
  timerMIDIrecieve = millis();
  clk_state = clk_stop;
  digitalWrite(CLOCK, LOW);
  resetNoteArraysLogicOutputs();
  Serial.println("clk_stop");
}

void active(void)
{
  timerMIDIrecieve = millis();
  Serial.println("active");
}

void resetNoteArraysLogicOutputs()
{
  for(uint8_t i = 0; i < sizeof(notelist); i++){
    for(uint8_t n = 0; n < sizeof(notelist[0]); n++){ //scans for holded note.
      notelist[i][n] = -1;
    }
    digitalWrite(Logic_outputs[i+2], LOW);
  }
}
