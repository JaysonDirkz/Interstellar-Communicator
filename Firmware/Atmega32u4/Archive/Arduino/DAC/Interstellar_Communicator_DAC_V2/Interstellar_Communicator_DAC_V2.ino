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

//const byte DAC[2] = {8,9};

bool knob_states[4];

byte Logic_outputs[6] = {CLOCK, RESET, GT1, GT2, GT3,GT4};

int8_t learn_notes_count = -1;
int8_t fir_but_push = -1;
int8_t sec_but_push = -1;
int8_t thi_but_push = -1;

byte CV_channels[4];
byte CV_states[4];
byte CVisKey = 1;

byte Gate_channels[4];
byte Gate_states[4];
byte GateisKey = 2;
byte GateisDrumGate = 3;
byte Gate_notes[4];

byte Poly_count = 0;

bool program_aftertouch[4];
bool play_aftertouch[4];
//byte note_for_after_bot[4];
//byte note_for_after_top[4];

unsigned long timerMIDIrecieve; //Opslag van millis() voor ontvangen MIDI bericht.

int8_t notelist[4][20]; //Alle noten hoger dan -1 in deze list zijn ingedrukt.

int clkstate = "stop"; //Zet clock status op clock stopgezet bij reset.
byte clock_count; //Telt de ontvangen midi clock messages.

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
  setVoltage(DAC1, 0, 1, 0);
  setVoltage(DAC1, 1, 1, 0);
  setVoltage(DAC2, 0, 1, 0);
  setVoltage(DAC2, 1, 1, 0);

  //Zet de alle waardes in de notelist op -1. Dit betekent geen noot ingedrukt.
  for(byte i = 0; i < 4; i++){
    for(byte n = 0; n < 20; n++){
      notelist[i][n] = -1;
    }
  }
}

void loop()
{
//  Schrijft continu de inverse status van de knoppen 1 t/m 4. Want knop ingedrukt geeft digitalRead() == false.
  for(byte i = 0; i < 4; i++){
    static byte knob_list[4] = {knob_1, knob_2, knob_3, knob_4};
    knob_states[i] = !digitalRead(knob_list[i]);
    if(!knob_states[0] && !knob_states[1] && !knob_states[2] && !knob_states[3]){
      learn_notes_count = 0;
    }
//    if(knob_states[i] && i == 3) pushed_knobs(knob_states[0], knob_states[1], knob_states[2], knob_states[3]);
//    Serial.print(knob_states[i]);
  }

  //Checked continu MIDI berichten.
  MIDI.read();

  //Als een MIDI bericht word ontvangen word de tijd opgeslagen in timerMIDIrecieve.
  if(MIDI.read()){
//    Serial.println("midiisgood");
    timerMIDIrecieve = millis();
  }

  //Als timerMIDIrecieve langer dan 500 milliseconden geen update ontvangt recieveStop getriggerd (zet CLOCK output en GT1, GT2 output op LOW).
  if(millis() - timerMIDIrecieve > 500){
//    Serial.println("yoyoMIDInothere");
    timerMIDIrecieve = millis();
    recieveStop();
  }

  for(byte i = 0; i < 6; i++){
    if(Output_timers[i] > 0 && millis() - Output_timers[i] > 10){
      digitalWrite(Logic_outputs[i], LOW);  // Set triggers low 10 msec after HIGH.
      Output_timers[i] = 0;
    }
  }
}

void noteon(byte channel, byte note, byte velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  if(chanRange){ //Zorgt ervoor dat de notelist niet gevuld kan worden met waardes buiten de omvang van de list in geval er corrupte midi data wordt ontvangen.
    timerMIDIrecieve = millis();
    byte learn_loop_count = 0;
    static byte CV_note_bot[4];
    static byte CV_note_top[4];
    static byte GateisTrig = 1;
    static byte Poly_mode[4];
    byte Monophonic = 1;
    byte Duophonic = 3;
    byte Triophonic = 4;
    byte Quadraphonic = 5;
    byte CVisVel = 2;
    for(byte i = 0; i < 4; i++){
      if(knob_states[i]){
        if(learn_loop_count == 0){
          fir_but_push = i;
          if(learn_notes_count == 0){ // Drum/pad/key Trigger.
            Gate_channels[i] = channel;
            Gate_notes[i] = note;
            Gate_states[i] = GateisTrig;
          }
          else if(learn_notes_count == 1){  // Range of Monophonic keys.
            Gate_states[i] = GateisKey;
            if(Gate_notes[i] > note){
              CV_note_top[i] = Gate_notes[i];
              CV_note_bot[i] = note;
              CV_states[i] = CVisKey;
              CV_channels[i] = channel;
              Poly_mode[i] = Monophonic;
            }
            else if(Gate_notes[i] < note){
              CV_note_bot[i] = Gate_notes[i];
              CV_note_top[i] = note;
              CV_states[i] = CVisKey;
              CV_channels[i] = channel;
              Poly_mode[i] = Monophonic;
            }
            else Gate_states[i] = GateisDrumGate; // if Gate_notes[i] == note, then CV_states[i], CV_channels[i], CV_note_bot[i] and CV_note_top[i] will not be written, so it works as Drum gate and doesn't interfere with what's on the CV output.
          }
        }
        else if(learn_loop_count == 1){
          sec_but_push = i;
          if(learn_notes_count == 0 || learn_notes_count == 1){ // Sets first the first channel to monophonic key mode.
            CV_channels[fir_but_push] = channel;
            CV_note_bot[fir_but_push] = 0;
            CV_note_top[fir_but_push] = 127;
            CV_states[fir_but_push] = CVisKey;
            Gate_states[fir_but_push] = GateisKey;

            Poly_mode[fir_but_push] = Monophonic;
            
            if(learn_notes_count == 0) { // Monophonic keys + [velocity]
              CV_channels[i] = channel;
              CV_states[i] = CVisVel;
            }
            else if(learn_notes_count == 1){ // Duophonic keys
              CV_channels[i] = channel;
              CV_note_bot[i] = 0;
              CV_note_top[i] = 127;
              CV_states[i] = CVisKey;
              Gate_states[i] = GateisKey;

              Poly_mode[fir_but_push] = Duophonic;
              Poly_mode[i] = Duophonic;
            }
          }
        }
        else if(learn_loop_count == 2){
          thi_but_push = i;
          if(learn_notes_count == 0){ // Monophonic keys + velocity + [poly/channel aftertouch]
            program_aftertouch[i] = true;
          }
          else if(learn_notes_count == 1){ // Duophonic keys + [velocity from first key)]
            CV_channels[i] = channel;
            CV_states[i] = CVisVel;
          }
          else if(learn_notes_count == 2){  // Triophonic keys
            CV_channels[i] = channel;
            CV_note_bot[i] = 0;
            CV_note_top[i] = 127;
            CV_states[i] = CVisKey;
            Gate_states[i] = GateisKey;

            Poly_mode[fir_but_push] = Triophonic;
            Poly_mode[sec_but_push] = Triophonic;
            Poly_mode[i] = Triophonic;
          }
        }
        else if(learn_loop_count == 3){
          if(learn_notes_count == 1){ // Duophonic keys + velocity + [poly/channel aftertouch]
            program_aftertouch[i] = true;
          }
          else if(learn_notes_count == 2){ // Triophonic keys + [velocity from first key]
            CV_channels[i] = channel;
            CV_states[i] = CVisVel;
          }
          else if(learn_notes_count == 3){ // Quadraphonic keys
            CV_channels[i] = channel;
            CV_note_bot[i] = 0;
            CV_note_top[i] = 127;
            CV_states[i] = CVisKey;
            Gate_states[i] = GateisKey;

            Poly_mode[fir_but_push] = Quadraphonic;
            Poly_mode[sec_but_push] = Quadraphonic;
            Poly_mode[thi_but_push] = Quadraphonic;
            Poly_mode[i] = Quadraphonic;
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
      if(Gate_channels[i] == channel){
        if(Gate_notes[i] == note){
          if(Gate_states[i] == GateisDrumGate){
            digitalWrite(Logic_outputs[i+2], HIGH);
          }
          else if(Gate_states[i] == GateisTrig){
            digitalWrite(Logic_outputs[i+2], HIGH);
            Output_timers[i+2] = millis();
          }
        }
      }
      if(CV_channels[i] == channel){
        if(CV_note_bot[i] <= note && CV_note_top[i] >= note && CV_states[i] == CVisKey){
          if(Poly_count == 0){
            for(byte n = 0; n < 20; n++){
              if(notelist[i][n] == -1){
                notelist[i][n] = note;
                break;
              }
            }
            if(Poly_mode[i] == Duophonic && sec_but_push == i) Poly_count = 1;
            else if(Poly_mode[i] == Triophonic && thi_but_push == i) Poly_count = 1;
            else if(Poly_mode[i] == Quadraphonic && 3 == i) Poly_count = 1;
            else Poly_count = Poly_mode[i];
            if(i < 2) setVoltage(DAC1, i, 1, note<<5);
            else setVoltage(DAC2, i-2, 1, note<<5);
            if(Gate_states[i] == GateisKey) digitalWrite(Logic_outputs[i+2], HIGH);
          }
          if(Poly_count > 0) Poly_count--;
          Serial.println(Poly_count);
          Serial.println(Poly_mode[i]);
        }
        else if(CV_states[i] == CVisVel){
          if(i < 2) setVoltage(DAC1, i, 1, velocity<<4);
          else setVoltage(DAC2, i-2, 1, velocity<<4);
        }
        
//        else if(CV_states[i] == CVisAT){
//          play_aftertouch[i] = true;
//        }
      }
    }
    learn_notes_count++;
  }
}

void noteoff(byte channel, byte note, byte velocity)
{
  //Zoeken naar de MIDI off noot in de notelist.
  //Als deze gevonden wordt in de notelist dan word deze positie (i) op -1 gezet.
  //Vervolgens word de noot op de volgende positie gekopieerd naar de huidige positie
  timerMIDIrecieve = millis();
  
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  
  if(chanRange && note >= 0 && note <= 127){
    for(byte i = 0; i < 4; i++){
      if(Gate_channels[i] == channel && Gate_states[i] == GateisDrumGate && Gate_notes[i] == note) digitalWrite(Logic_outputs[i+2], LOW);
      if(CV_channels[i] == channel && CV_states[i] == CVisKey){
//        int8_t maxnote = -1;
        int8_t lastnote = -1;
        for(byte n = 0; n < 20; n++){
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
}

void controlchange(byte channel, byte number, byte value)
{
  timerMIDIrecieve = millis();

  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  
  static byte Nrpn_Msb_chans[4];
  static byte Nrpn_Msb_nums[4];
  static byte Nrpn_Msb_vals[4];
  static bool MSB_READY[4];
  
  static byte Nrpn_Lsb_chans[4];
  static byte Nrpn_Lsb_nums[4];
  static byte Nrpn_Lsb_vals[4];
  static bool LSB_READY[4];

  static bool CCisOK[4];
  
  static byte CV_numbers[4];
  byte CVisCC = 3;

  if(chanRange && number == 99){  //NRPN MSB
    for(byte i = 0; i < 4; i++){
      if(knob_states[i]){
        Nrpn_Msb_chans[i] = channel;
        Nrpn_Msb_nums[i] = number;
        Nrpn_Msb_vals[i] = value;
      }
      if(Nrpn_Msb_chans[i] == channel && Nrpn_Msb_nums[i] == number && Nrpn_Msb_vals[i] == value) MSB_READY[i] = true;
      else MSB_READY[i] = false;
    }
  }
  else if(chanRange && number == 98){ //NRPN LSB
    for(byte i = 0; i < 4; i++){
      if(MSB_READY[i]){
        if(knob_states[i]){
          Nrpn_Lsb_chans[i] = channel;
          Nrpn_Lsb_nums[i] = number;
          Nrpn_Lsb_vals[i] = value;
        }
        if(Nrpn_Lsb_chans[i] == channel && Nrpn_Lsb_nums[i] == number && Nrpn_Lsb_vals[i] == value) LSB_READY[i] = true;
        else LSB_READY[i] = false;
      }
    }
  }
  else if(chanRange && number >= 0 && number <= 127){
    for(byte i = 0; i < 4; i++){  //Scanned voor ingedrukte knop, en koppelt CC als knop is ingedrukt.
      if(number == 6){  //Data Entry MSB
        if(MSB_READY[i] && LSB_READY[i]) CCisOK[i] = true;
        else CCisOK[i] = false;
      }
      else CCisOK[i] = true;
      if(CCisOK[i]){
          if(knob_states[i]){
          CV_channels[i] = channel;
          CV_numbers[i] = number;
          CV_states[i] = CVisCC;
        }
        if(CV_channels[i] == channel && CV_numbers[i] == number && CV_states[i] == CVisCC){ //Stuur CC naar DAC. CC data is 7-bits. DAC is 12-bits. Bitshift van 4 naar links is 11 bits dus dat is de helft van de DAC output.
          if(i < 2) setVoltage(DAC1, i, 1, value<<4);
          else setVoltage(DAC2, i-2, 1, value<<4);
        }
      }
    }
  }
}

void pitchbend(byte channel, int bend)
{
  timerMIDIrecieve = millis();
//  Serial.println("PB recieved");
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
  
  static byte CV_channels[4];
  static byte CVisPB = 4;
  if(chanRange && bend >= 0 && bend <= 16383){
    for(byte i = 0; i < 4; i++){
      if(knob_states[i]){
        CV_channels[i] = channel;
        CV_states[i] = CVisPB;
      }
      if(CV_channels[i] == channel && CV_states == CVisPB){
        if(i < 2) setVoltage(DAC1, i, 1, bend>>2);  //Bend data is 14-bits. DAC is 12-bit. Dus bitshift van 2 naar rechts.
        else setVoltage(DAC2, i-2, 1, bend>>2);
      }
    }
  }
}

void afterchan(byte channel, byte pressure)
{
  timerMIDIrecieve = millis();
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
}

void afterpoly(byte channel, byte note, byte pressure)
{
  timerMIDIrecieve = millis();
  bool chanRange = (channel >= 0 && channel <= 127); // Kan niet als global geinitieerd worden want channel bestaat dan nog niet.
}

//setVoltage -- Set DAC voltage output
//dacpin: chip select pin for DAC.  Note and velocity on DAC1, pitch bend and CC on DAC2
//channel: 0 (A) or 1 (B).  Note and pitch bend on 0, velocity and CC on 2.
//gain: 0 = 1X, 1 = 2X.  
//mV: integer 0 to 4095.  If gain is 1X, mV is in units of half mV (i.e., 0 to 2048 mV).
//If gain is 2X, mV is in units of mV

void setVoltage(byte dacpin, bool channel, bool gain, unsigned int mV)
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

void recieveClock(void)
{
  timerMIDIrecieve = millis();
//  Serial.println("clock");
  if(clock_count == 0 && (clkstate == "start" || clkstate == "con")) digitalWrite(CLOCK, HIGH); // Start clock pulse
  if(clkstate != "stop") clock_count++;
  if(clock_count >= 3) {  // MIDI timing clock sends 24 pulses per quarter note.  Sent pulse only once every 3 pulses (32th notes).
    digitalWrite(CLOCK, LOW);
    clock_count = 0;
  }
}

void recieveStart(void)
{
  timerMIDIrecieve = millis();
  digitalWrite(RESET, HIGH);
  timerRes = millis();
  clock_count = 0;
  clkstate = "start";
//  if(clkstate == "start") Serial.println("start");
}

void recieveContinue(void)
{
  timerMIDIrecieve = millis();
  clkstate = "con";
}

void recieveStop(void)
{
  clkstate = "stop";
  digitalWrite(CLOCK, LOW);
  for(byte i = 0; i < 4; i++){
    for(byte n = 0; n < 20; n++){ //scans for holded note.
      notelist[i][n] = -1;
    }
    digitalWrite(Logic_outputs[i+2], LOW);
  }
  Serial.println("Stop");
}

void active(void)
{
  timerMIDIrecieve = millis();
  Serial.println("active");
}
