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
#define KNOB_1 18
#define KNOB_2 19
#define KNOB_3 20
#define KNOB_4 21

//const uint8_t DAC[2] = {8,9};

bool knob_states[4];

uint8_t Logic_outputs[6] = {CLOCK, RESET, GT1, GT2, GT3,GT4};

int8_t ButtonWithKey[17] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

bool maycheckpolynoteon = true;
bool maycheckpolyvelocity = true;
bool maycheckpolyaftertouch = true;

int8_t PolyNoteonType[17];
int8_t PolyVelocityType[17];
int8_t PolyAftertouchType[17];

uint8_t LastPolyNoteonAtButton[17];
uint8_t LastPolyVelocityAtButton[17];
uint8_t LastPolyAftertouchAtButton[17];

uint8_t PolyNoteonCount[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
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
      
unsigned long timerMIDIrecieve; //Opslag van millis() voor ontvangen MIDI bericht.

int8_t notelist[4][20]; //Alle noten hoger dan -1 in deze list zijn ingedrukt.
uint8_t LengthNotelist;

uint8_t clk_stop = 0;
uint8_t clk_start = 1;
uint8_t clk_continue = 2;
uint8_t clk_state = clk_stop; //Zet clock status op clock stopgezet bij reset.

uint8_t clock_count; //Telt de ontvangen midi clock messages.

unsigned long timerClock, timerTrig1, timerTrig2, timerTrig3, timerTrig4, timerRes; //Slaat millis() op vanaf het aanzetten van een trigger.
unsigned long Output_timers[6] = {timerClock, timerRes, timerTrig1, timerTrig2, timerTrig3, timerTrig4};

MIDI_CREATE_DEFAULT_INSTANCE(); 

void setup()
{
  pinMode(CLOCK, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(GT1, OUTPUT);
  pinMode(GT2, OUTPUT);
  pinMode(GT3, OUTPUT);
  pinMode(GT4, OUTPUT);
  pinMode(DAC1, OUTPUT); //dac 1 channel 1 and 2
  pinMode(DAC2, OUTPUT); //dac 2 channel 1 and 2
  pinMode(KNOB_1, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
  pinMode(KNOB_2, INPUT_PULLUP);
  pinMode(KNOB_3, INPUT_PULLUP);
  pinMode(KNOB_4, INPUT_PULLUP);
  
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
  MIDI.setHandleAfterTouchChannel(chanafter);
  MIDI.setHandleAfterTouchPoly(afterpoly);
  MIDI.setHandleClock(recieveclock);
  MIDI.setHandleStart(recievestart);
  MIDI.setHandleContinue(recievecontinue);
  MIDI.setHandleStop(recievestop);
  MIDI.setHandleActiveSensing(active);
  
  //Sets the DAC outputs on lowest output, which translates to 0V. Which is note C-1 and midi note 0.
  setVoltage(DAC1, 0, 1, 0);
  setVoltage(DAC1, 1, 1, 0);
  setVoltage(DAC2, 0, 1, 0);
  setVoltage(DAC2, 1, 1, 0);

  LengthNotelist = sizeof(notelist[0]);

  //Zet de alle waardes in de notelist op -1. Dit betekent geen noot ingedrukt.
  for(uint8_t i = 0; i < 4; i++){
    for(uint8_t n = 0; n < LengthNotelist; n++){
      notelist[i][n] = -1;
    }
  }
}

void loop()
{
//  Schrijft continu de inverse status van de knoppen 1 t/m 4. Want knop ingedrukt geeft digitalRead() == false.
  for(uint8_t i = 0; i < 4; i++){
    static uint8_t knob_list[4] = {KNOB_1, KNOB_2, KNOB_3, KNOB_4};
    knob_states[i] = !digitalRead(knob_list[i]);
    if (!knob_states[i]) program_aftertouch[i] = false;
  }
  if(!knob_states[0] && !knob_states[1] && !knob_states[2] && !knob_states[3]){
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
//    if(Poly_mode[i]) PolyNoteonCount = ;
//    if(knob_states[i] && i == 3) pushed_knobs(knob_states[0], knob_states[1], knob_states[2], knob_states[3]);
//    Serial.print(knob_states[i]);

  //Checked continu MIDI berichten.
  MIDI.read();

  //Als een MIDI bericht word ontvangen word de tijd opgeslagen in timerMIDIrecieve.
  if(MIDI.read()){
//    Serial.println("midiisgood");
    timerMIDIrecieve = millis();
  }

  //Als timerMIDIrecieve langer dan 500 milliseconden geen update ontvangt recievestop getriggerd (zet CLOCK output en GT1, GT2 output op LOW).
  if(millis() - timerMIDIrecieve > 500){
//    Serial.println("yoyoMIDInothere");
    timerMIDIrecieve = millis();
    recievestop();
  }

  for(uint8_t i = 0; i < 6; i++){
    if(Output_timers[i] > 0 && millis() - Output_timers[i] > 10){
      digitalWrite(Logic_outputs[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
    }
  }
}

void checkpolynoteon()
{
  maycheckpolynoteon = false;
  for (byte i = 0; i < 17; i++) {
    PolyNoteonType[i] = 0;
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisKey) {
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
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisVel) {
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
  }
  for (byte i = 0; i < 4; i++) {
    if (CV_states[i] == CVisAfter) {
      LastPolyAftertouchAtButton[CV_channels[i]] = i;
      PolyAftertouchType[CV_channels[i]]++;
    }
  }
}

void noteon(uint8_t channel, uint8_t note, uint8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  if(chanRange){ //Zorgt ervoor dat de notelist niet gevuld kan worden met waardes buiten de omvang van de list in geval er corrupte midi data wordt ontvangen.
    timerMIDIrecieve = millis();
    uint8_t learn_loop_count = 0;
    for(uint8_t i = 0; i < 4; i++){
      if(knob_states[i]){
        if(learn_loop_count == 0){
          if(learn_notes_count == 0){ // Monophonic keys.
            CV_channels[i] = channel;
            CV_states[i] = CVisKey;
            Gate_states[i] = GateisKey;
          }
          else if(learn_notes_count == 1){ // Drum Trigger and velocity
            Gate_channels[i] = channel;
            Gate_notes[i] = note;
            Gate_states[i] = GateisTrig;

            CV_channels[i] = channel;
            CV_states[i] = CVisVel;
          }
          else if(learn_notes_count == 2){ // Drum gate and aftertouch
            Gate_states[i] = GateisDrumGate;
            CV_states[i] = CVisAfter;
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
      
      //Als ingestelde channel overeenkomt met ontvangen channel:
      //Als noot of gate geselecteerd is:
        //Zet noot in notelist_* op de plek waar nog geen noot staat (i == -1).
        //Verschuif MIDI noot naar 5 bits naar links en verstuur noot naar DAC1.
        //Verstuur MIDI velocity 4 bits naar links en verstuur DAC1.
        //Zet Gate hoog.
      if (Gate_channels[i] == channel) {
        if (Gate_notes[i] == note) {
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
      }
      if (CV_channels[i] == channel) {
        if (PolyNoteonCount[channel] == 0 &&
        CV_states[i] == CVisKey) {
          for (uint8_t n = 0; n < LengthNotelist; n++) {
            if (notelist[i][n] == -1) {
             notelist[i][n] = note;
             break;
            }
          }
          if (i < 2) setVoltage(DAC1, i, 1, note<<5);
          else {
            setVoltage(DAC2, i-2, 1, note<<5);
          }
          if (Gate_states[i] == GateisKey) {
            digitalWrite(Logic_outputs[i+2], HIGH);
          }
          
          PolyNoteonCount[channel] = PolyNoteonType[channel];
          if (LastPolyNoteonAtButton[channel] == i) {
            PolyNoteonCount[channel] = 0;
          }

//          play_next_aftertouch[aftertouchButton[i]] = notelist[i][0];
//          play_vel_onetime[channel] = true;
          ButtonWithKey[channel] = i;
        }
        else if (CV_states[i] == CVisVel
        && (PolyVelocityCount[channel] == 0 || PlayDrumTrigVel[i])) {
          if (i < 2) {
            setVoltage(DAC1, i, 1, velocity<<5);
          }
          else {
            setVoltage(DAC2, i-2, 1, velocity<<5);
          }
          if (PolyVelocityCount[channel] == 0) {
            PolyVelocityCount[channel] = PolyVelocityType[channel];
            if (LastPolyVelocityAtButton[channel] == i) {
              PolyVelocityCount[channel] = 0;
            }
          }
        }
//        else if (CV_states[i] == CVisAfter
//        && PolyAftertouchCount[channel] == 0) {
//          Serial.println(play_next_aftertouch[i]);
//          Serial.println(i);
//          Serial.flush();
//          play_next_aftertouch[i] = notelist[ButtonWithKey[channel]][0];
//          ButtonWithKey[channel] = -1;
//          
//          PolyAftertouchCount[channel] = PolyAftertouchType[channel];
//          if (LastPolyAftertouchAtButton[channel] == i) {
//            PolyAftertouchCount[channel] = 0;
//          }
//        }
        else if (CV_states[i] == CVisKey
        && PolyNoteonCount[channel] > 0) {
          PolyNoteonCount[channel]--;
        }
        else if (CV_states[i] == CVisVel
        && PolyVelocityCount[channel] > 0) {
          PolyVelocityCount[channel]--;
        }
      }
      PlayDrumTrigVel[i] = false;
    }
    learn_notes_count++;
//    play_vel_onetime[channel] = false;
//    ButtonWithKey[channel] = -1;
  }
}

void noteoff(uint8_t channel, uint8_t note, uint8_t velocity)
{
  //Zoeken naar de MIDI off noot in de notelist.
  //Als deze gevonden wordt in de notelist dan word deze positie (i) op -1 gezet.
  //Vervolgens word de noot op de volgende positie gekopieerd naar de huidige positie
  timerMIDIrecieve = millis();
  
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  
  if (chanRange && note >= 0 && note <= 127) {
    for (uint8_t i = 0; i < 4; i++) {
      if (Gate_channels[i] == channel
      && Gate_states[i] == GateisDrumGate
      && Gate_notes[i] == note) digitalWrite(Logic_outputs[i+2], LOW);
      if (CV_channels[i] == channel
      && CV_states[i] == CVisKey) {
//        int8_t maxnote = -1;
        int8_t lastnote = -1;
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
        else if(i < 2) setVoltage(DAC1, i, 1, lastnote<<5);
        else setVoltage(DAC2, i-2, 1, lastnote<<5);
      }
    }
  }
}

void chanafter(uint8_t channel, uint8_t pressure)
{
  Serial.println("chanafter");
  Serial.flush();
  timerMIDIrecieve = millis();
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  if(chanRange && pressure >= 0 && pressure <= 127){
    for(uint8_t i = 0; i < 4; i++){
      if(knob_states[i]
      && program_aftertouch[i]){
        CV_channels[i] = channel;
        CV_states[i] = CVisAfter;
      }
      if(CV_channels[i] == channel
      && CV_states[i] == CVisAfter){
        Serial.println("playchanafter");
        Serial.flush();
        if(i < 2) setVoltage(DAC1, i, 1, pressure<<4);  //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
        else setVoltage(DAC2, i-2, 1, pressure<<4);
      }
    }
  }
}

void afterpoly(uint8_t channel, uint8_t note, uint8_t pressure)
{
  Serial.println("afterpoly");
  Serial.flush();
  timerMIDIrecieve = millis();
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  if (chanRange && pressure >= 0 && pressure <= 127) {
    for (uint8_t i = 0; i < 4; i++) {
      if (knob_states[i]
      && program_aftertouch[i]) {
        CV_channels[i] = channel;
        CV_states[i] = CVisAfter;
      }
      if (CV_channels[i] == channel
      && CV_states[i] == CVisAfter
      && ((PolyAftertouchCount[channel] == 0
      && notelist[ButtonWithKey[channel]][0] == note)
      || DrumGatePolyAfter[i] == note)) {
        Serial.println("playafterpoly");
        Serial.flush();
        if (i < 2) setVoltage(DAC1, i, 1, pressure << 4);  //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
        else setVoltage(DAC2, i - 2, 1, pressure << 4);

        if (PolyAftertouchCount[channel] == 0) {
          PolyAftertouchCount[channel] = PolyAftertouchType[channel];
          if (LastPolyAftertouchAtButton[channel] == i) {
            PolyAftertouchCount[channel] = 0;
          }
        }
      }
      else if (CV_states[i] == CVisAfter
      && PolyAftertouchCount[channel] > 0) {
        PolyAftertouchCount[channel]--;
      }
    }
  }
}

void controlchange(uint8_t channel, uint8_t number, uint8_t value)
{
  timerMIDIrecieve = millis();

  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  
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

  if (chanRange && number == 99) {  //NRPN MSB
    for (uint8_t i = 0; i < 4; i++) {
      if (knob_states[i]) {
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
  else if (chanRange && number == 98) { //NRPN LSB
    for (uint8_t i = 0; i < 4; i++) {
      if (MSB_READY[i]) {
        if (knob_states[i]) {
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
  else if (chanRange && number >= 0 && number <= 127) {
    for (uint8_t i = 0; i < 4; i++) {  //Scanned voor ingedrukte knop, en koppelt CC als knop is ingedrukt.
      if (number == 6) {  //Data Entry MSB
        if (MSB_READY[i]&& LSB_READY[i]) CCisOK[i] = true;
        else CCisOK[i] = false;
      }
      else CCisOK[i] = true;
      if (CCisOK[i]) {
        if(knob_states[i]){
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
  timerMIDIrecieve = millis();
//  Serial.println(bend);
//  Serial.flush();
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  static uint8_t CV_channels[4];
  if(chanRange && bend >= -8192 && bend <= 8191){
    for(uint8_t i = 0; i < 4; i++){
      if(knob_states[i]){
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

//setVoltage -- Set DAC voltage output
//dacpin: chip select pin for DAC.  Note and velocity on DAC1, pitch bend and CC on DAC2
//channel: 0 (A) or 1 (B).  Note and pitch bend on 0, velocity and CC on 2.
//gain: 0 = 1X, 1 = 2X.  
//mV: integer 0 to 4095.  If gain is 1X, mV is in units of half mV (i.e., 0 to 2048 mV).
//If gain is 2X, mV is in units of mV

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
  timerMIDIrecieve = millis();
//  Serial.println("clock");
  if(clock_count == 0 && (clk_state == clk_start || clk_state == clk_continue)) digitalWrite(CLOCK, HIGH); // Start clock pulse
  if(clk_state != clk_stop) clock_count++;
  if(clock_count >= 3) {  // MIDI timing clock sends 24 pulses per quarter note.  Sent pulse only once every 3 pulses (32th notes).
    digitalWrite(CLOCK, LOW);
    clock_count = 0;
  }
}

void recievestart(void)
{
  timerMIDIrecieve = millis();
  digitalWrite(RESET, HIGH);
  timerRes = millis();
  clock_count = 0;
  clk_state = clk_start;
//  if(clk_state == clk_start) Serial.println(clk_start);
}

void recievecontinue(void)
{
  timerMIDIrecieve = millis();
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
  Serial.println("clk_stop");
}

void active(void)
{
  timerMIDIrecieve = millis();
//  Serial.println("active");
}
