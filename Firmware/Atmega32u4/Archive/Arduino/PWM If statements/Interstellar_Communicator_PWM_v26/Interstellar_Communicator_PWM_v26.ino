/*
Versie:
Poly en mono gescheiden.
Eerst channel diffrentieren in cycling, trigger, split.

Logboek:
Interstellar_Communicator_PWM_v26 [4-5-22]:
- Bijna alles erin en getest.
Interstellar_Communicator_PWM_v24 [3-4-22]:
- Alle logic omgebouwd.
Interstellar_Communicator_PWM_v22 [22-2-22]:
- Gedeeltelijk Debugged.
Interstellar_Communicator_PWM_v21 [16-1-22]:
- Aangepast voor unipolaire CV.
Interstellar_Communicator_PWM_v20 [3-6-21]:
- Logboek aangemaakt.
- Midi clock ticks veranderd naar altijd output.

- BUGS:
  Met polyphonic aftertouch werkt alleen de aftertouch van de laatst gespeelde noot.
  Inleren met multitouchscherm werkt niet lekker.
  Voeg pitchbend ook nog toe in het prioriteitenstelsel van keys.
  
TODO:
- Channel aftertocuh ook nog testen.
- Dubbelklik wisfunctie.
- EEPROM.
- Split ook nog testen.
- ControlChange nog loop weghalen als het kan.
- Data besparen?:
    Verminderen aantal midi kanalen van learn. (nu 16).
- Voeg ook twee percussie kanalen toe met choke groups.
    Bij een choke wordt de velocity wordt laaggebracht.
    Alle outputs binnen 1 channel kappen elkaars velocity af.

- Test op glitches wanneer je veel midi ontvangt.
	- Glitches? Kijk of dit verdwijnt met midiThruOff.
		- Ja? Kijk of je midithru in hardware kan door-routen.
- Active sensing timeout nu niet gebruikt. Blijf testen om te kijken of er noten blijven hangen. Zo niet? Dan kan het zo blijven.
*/

#include <MidiLearn.h>
#include <UtilityObjects.h>
#include <UtilityFunc.h>
#include <MIDI.h>
#include <EEPROM.h>
#include <MidiLastNote.h>

#define CLOCK_PIN 2
#define RESET_PIN 3
#define GATE_1_PIN 4
#define GATE_2_PIN 5
#define GATE_3_PIN 7
#define GATE_4_PIN 8
#define BUTTON_1_PIN A0
#define BUTTON_2_PIN A1
#define BUTTON_3_PIN A2
#define BUTTON_4_PIN A3

MIDI_CREATE_DEFAULT_INSTANCE();

// Frequency modes for TIMER1
#define PWM62k 1   //62500 Hz

// Direct PWM change variables
#define PWM9 OCR1A
#define PWM10 OCR1B

// Frequency modes for TIMER4
#define PWM187k 1   // 187500 Hz

// Direct PWM change variables
#define PWM6 OCR4D
#define PWM13 OCR4A

// Terminal count
#define PWM6_13_MAX OCR4C

const uint8_t rowTo_32u4PINF_bit[4] = {0x80, 0x40, 0x20, 0x10};

const int8_t CV_RSHIFT[(int8_t)Type::Size] = {0, 0, 0, 0, 0, 6, 6};

TwoDimensionalLookup <(int8_t)Type::Size, 16> address;

Channels channels;

const int8_t addressAmount = 4;
const int8_t rowAmount = 4;
MidiProgrammer <addressAmount, rowAmount> learn;


int8_t learnToPolyMap[addressAmount] = {-1, -1, -1, -1};
const int8_t polyAddressAmount = 2;
Polyphony <polyAddressAmount> polyphony;


int8_t learnToLastNoteMap[4] = {-1, -1, -1, -1};
MidiLastNote lastNote[4];

int8_t rowNote[4] = {-1, -1, -1, -1};

//Saved in eeprom
//------
int8_t msb_numbers[4];
// int8_t lsb_numbers[4];
int8_t nrpn_msb_controls[4];
int8_t nrpn_lsb_controls[4];
bool precision[4];
//------

// 5 als minimum want midi clock pulse lengte bij 360 bpm is 60000 / 360*24 = 6,94 ms
const uint8_t PULSE_LENGTH_MS = 5;

void setup()
{
	setinputsandoutputs();
	configuremidisethandle();
	load_learn_status();
}

void setinputsandoutputs()
{
	MIDI.begin(MIDI_CHANNEL_OMNI);
	MIDI.turnThruOff();
	
	pinMode(CLOCK_PIN, OUTPUT);
	pinMode(RESET_PIN, OUTPUT);
	pinMode(GATE_1_PIN, OUTPUT);
	pinMode(GATE_2_PIN, OUTPUT);
	pinMode(GATE_3_PIN, OUTPUT);
	pinMode(GATE_4_PIN, OUTPUT);

	pinMode(BUTTON_1_PIN, INPUT_PULLUP); //Knoppen 1 t/m 4 sluiten de input kort naar massa bij het indrukken.
	pinMode(BUTTON_2_PIN, INPUT_PULLUP);
	pinMode(BUTTON_3_PIN, INPUT_PULLUP);
	pinMode(BUTTON_4_PIN, INPUT_PULLUP);

	digitalWrite(CLOCK_PIN,LOW);
	digitalWrite(RESET_PIN,LOW);
	digitalWrite(GATE_1_PIN,LOW);
	digitalWrite(GATE_2_PIN,LOW);
	digitalWrite(GATE_3_PIN,LOW);
	digitalWrite(GATE_4_PIN,LOW);
	
	confpwm();
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
	MIDI.setHandleNoteOn(note_on);
	MIDI.setHandleNoteOff(note_off);
	MIDI.setHandleControlChange(control_change);
	MIDI.setHandlePitchBend(pitchbend);
	MIDI.setHandleAfterTouchChannel(atc);
	MIDI.setHandleAfterTouchPoly(atp);
	MIDI.setHandleClock(clock_tick);
	MIDI.setHandleStart(clock_start);
	MIDI.setHandleContinue(clock_continue);
	MIDI.setHandleStop(clock_stop);
//  MIDI.setHandleActiveSensing(RecievedActive);
}

void load_learn_status()
{
	for ( int8_t row = 0; row < 4; ++row )
	{
//		cv_type[row] = 				EEPROM.read(row);
//		cv_channel[row] = 			EEPROM.read(row+4);
//		perc_type[row] = 			EEPROM.read(row+8);
//		perc_channel[row] = 		EEPROM.read(row+12);
//		perc_num[row] = 			EEPROM.read(row+16);
//		polyphony[row + 1] = 			EEPROM.read(row+20);
//		polyphony[row + 5] = 		EEPROM.read(row+24);
//		polyphony[row + 9] = 		EEPROM.read(row+28);
//		polyphony[row + 13] = 		EEPROM.read(row+32);
//		msb_numbers[row] = 			EEPROM.read(row+36);
//		// lsb_numbers[row] = 			EEPROM.read(row+40);
//		nrpn_msb_controls[row] = 	EEPROM.read(row+44);
//		nrpn_lsb_controls[row] = 	EEPROM.read(row+48);
//		precision[row] = 			EEPROM.read(row+52);
	}
}

void loop()
{
	MIDI.read();
    controlThread();
    setupLearn();
}

int8_t clockCounter = -1;
int8_t resetCounter = -1;
int8_t trigCounter[4] = {-1, -1, -1, -1};

void controlThread()
{
    auto currentMillis = millis();
    static auto lastMillis = currentMillis;
    if ( currentMillis != lastMillis )
    {
        lastMillis = currentMillis;

        switch ( clockCounter )
        {
            case -1: break;
            case 0: digitalWrite(CLOCK_PIN, LOW);
            default: --clockCounter; break;
        }

        switch ( resetCounter )
        {
            case -1: break;
            case 0: digitalWrite(RESET_PIN, LOW);
            default: --resetCounter; break;
        }
        
        for ( int8_t row = 0; row < 4; ++row )
        {
            switch ( trigCounter[row] )
            {
                case -1: break;
                case 0: gate_out(row, LOW);
                default: --trigCounter[row]; break;
            }
        }
    }
}

void setupLearn()
{
    for ( int8_t row = 0; row < 4; ++row )
    {
        bool buttonStates[4];
        static bool lastButtonStates[4] = {0, 0, 0, 0};

        buttonStates[row] = ~PINF & rowTo_32u4PINF_bit[row];
        int8_t deltaButtonState = buttonStates[row] - lastButtonStates[row];
        lastButtonStates[row] = buttonStates[row];
        
        if ( deltaButtonState > 0 )
        {
            delay(200); // against hysterisis
            
            MIDI.setHandleNoteOn(learn_note);
            MIDI.disconnectCallbackFromType(midi::NoteOff);
            MIDI.setHandleControlChange(learn_control_change);
            MIDI.setHandlePitchBend(learn_pitchbend);
            MIDI.setHandleAfterTouchChannel(learn_atc);
            MIDI.disconnectCallbackFromType(midi::AfterTouchPoly);
            MIDI.disconnectCallbackFromType(midi::Clock);
            MIDI.disconnectCallbackFromType(midi::Start);
//            MIDI.disconnectCallbackFromType(midi::Continue);
            MIDI.disconnectCallbackFromType(midi::Stop);
        }
        else if ( deltaButtonState < 0 )
        {
            clearRowData();
            writeGlobalAddresses();
            writePolyAddresses();
            checkPolyphony();
            save_learn_status();

//            for ( int8_t type = 0; type < (int8_t)Type::Size; ++type )
//            {
//                for ( int8_t channel = 0; channel < 16; ++channel )
//                {
//                    if ( address.getState(type, channel) )
//                    {
//                        MIDI.sendNoteOn(address.get(type, channel), type, 5);
//                        MIDI.sendNoteOn(address.get(type, channel), channel, 5);
//                    }
//                }
//            }
            
            delay(200); // against hysterisis

            MIDI.setHandleNoteOn(note_on);
            MIDI.setHandleNoteOff(note_off);
            MIDI.setHandleControlChange(control_change);
            MIDI.setHandlePitchBend(pitchbend);
            MIDI.setHandleAfterTouchChannel(atc);
            MIDI.setHandleAfterTouchPoly(atp);
            MIDI.setHandleClock(clock_tick);
            MIDI.setHandleStart(clock_start);
//            MIDI.setHandleContinue(clock_continue);
            MIDI.setHandleStop(clock_stop);
        }
    }
}

void clearRowData()
{
    for ( int8_t row = 0; row < rowAmount; ++row )
    {
        rowNote[row] = -1;
    }
}

void writeGlobalAddresses()
{
    // Reset all addresses.
    address.fill();
    
    // Write addresses for learned types.
//    int8_t globalAddrCount = 0;
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        if ( learn.getRow(addr) > -1 )
        {
            // If poly, only last address is set.
//            address.setUnique((int8_t)learn.getType(addr), learn.getChannel(addr), globalAddrCount);
            address.setUnique((int8_t)learn.getType(addr), learn.getChannel(addr), addr);
//            MIDI.sendNoteOn(address.get((int8_t)learn.getType(addr), learn.getChannel(addr)), 61, 5);
//            ++globalAddrCount;
        }
    }
}

void writePolyAddresses()
{
    // Reset
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        learnToPolyMap[addr] = -1;
    }
    
    // Include in polyphony if type and channel match more than 1 time found.
    int8_t polyAddrCount = 0;
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            for ( int8_t type = (int8_t)Type::Keys; type <= (int8_t)Type::KeysPolyPressure; ++type )
            {
                int8_t polyWidth = 0;
                
                for ( int8_t addr = 0; addr < addressAmount; ++addr )
                {
                    if ( learn.getType(addr) == (Type)type and learn.getChannel(addr) == channel )
                    {
                        learnToPolyMap[addr] = polyAddrCount;
                        ++polyWidth;

//                        MIDI.sendNoteOn(address.get((int8_t)type, channel), 62, 5);

                        // Reset het poly adres als het mono blijkt te zijn.
                        // Niet mono? Dan poly adres counter incrementen.
                        if ( addr == address.get((int8_t)type, channel) )
                        {
                            if ( polyWidth == 1  ) learnToPolyMap[addr] = -1;
                            else ++polyAddrCount;
//                            MIDI.sendNoteOn(addr, 63, 5);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void checkPolyphony()
{
    for ( int8_t a = 0; a < polyAddressAmount; ++a )
    {
        // Reset width.
        polyphony.width[a] = 0;

        // Reset counter
        polyphony.counter[a] = 0;

        // Reset count and row info.
        for ( int8_t count = 0; count < 4; ++count )
        {
            polyphony.rowAtCount[a][count] = -1;

            int8_t row = count;
            polyphony.countAtRow[row] = 0;
        }
    }

    // Write all width
    for ( int8_t a = 0; a < addressAmount; ++a )
    {
        int8_t polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            ++polyphony.width[polyAddr];
        }
    }

    // Write width for KeysVelocity and KeysPolyPressure (not higher than Key width).
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            int8_t keyAddr = address.get((int8_t)Type::Keys, channel);
            int8_t velAddr = address.get((int8_t)Type::KeysVelocity, channel);
            int8_t pressAddr = address.get((int8_t)Type::KeysPolyPressure, channel);

            if ( keyAddr > -1 )
            {
                if ( velAddr > -1 ) polyphony.width[velAddr] = min(polyphony.width[keyAddr], polyphony.width[velAddr]);
                if ( pressAddr > -1 ) polyphony.width[pressAddr] = min(polyphony.width[keyAddr], polyphony.width[pressAddr]);
            }
        }
    }

    // Write rowAtCount
    for ( int8_t a = 0; a < addressAmount; ++a )
    {
        int8_t polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            int8_t count = polyphony.counter[polyAddr]++;
            
            if ( count < polyphony.width[polyAddr] ) polyphony.rowAtCount[polyAddr][count] = learn.getRow(a);
        }
    }

    // Write boundary.
    for ( int8_t a = 0; a < addressAmount; ++a )
    {
        int8_t polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            if ( channels.getChannelType(learn.getChannel(a)) == ChannelType::KeysSplit )
            {
                int8_t row = learn.getRow(a);
                int8_t width = polyphony.width[polyAddr];
                
                if ( width > 1 )
                {
                    // [width][row]
                    const int8_t boundariesHigh[3][4] = {
                        {63, 127, 127, 127},
                        {42, 85, 127, 127},
                        {31, 63, 95, 127}
                    };
                    
                    polyphony.boundary[row] = boundariesHigh[width - 2][row];
                }
                else polyphony.boundary[row] = 127;
            }
        }
    }
    
    // Clear all counters.
    for ( int8_t a = 0; a < polyAddressAmount; ++a )
    {
        polyphony.counter[a] = 0;
    }
}

void save_learn_status()
{
	for ( int8_t row = 0; row < 4; ++row )
	{
//		EEPROM.update(row, 	  cv_type[row]);
//		EEPROM.update(row+4,  cv_channel[row]);
//		EEPROM.update(row+8,  perc_type[row]);
//		EEPROM.update(row+12, perc_channel[row]);
//		EEPROM.update(row+16, perc_num[row]);
//		EEPROM.update(row+20, polyphony[row + 1]);
//		EEPROM.update(row+24, polyphony[row + 5]);
//		EEPROM.update(row+28, polyphony[row + 9]);
//		EEPROM.update(row+32, polyphony[row + 13]);
//		EEPROM.update(row+36, msb_numbers[row]);
//		// EEPROM.update(row+40, lsb_numbers[row]);
//		EEPROM.update(row+44, nrpn_msb_controls[row]);
//		EEPROM.update(row+48, nrpn_lsb_controls[row]);
//		EEPROM.update(row+52, precision[row]);
	}
}

void learn_note(int8_t channel, int8_t note, int8_t velocity)
{
    --channel;

    // Program the note type, velocity or aftertouch for each row.
    int8_t addrCnt = 0;
	for ( int8_t row = 0; row < 4; ++row )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
//            for ( int8_t a = 0; a < addressAmount; ++a )
//            {
//                if ( learn.getRow(a) > -1 ) addrCnt = a + 1;
//                
//                if ( learn.getRow(a) == row )
//                {
//                    addrCnt = a;
//                    break;
//                }
//            }
            
            switch ( channels.getChannelType(channel) )
            {
                case ChannelType::KeysCycling:
                case ChannelType::KeysSplit:
                    switch ( addrCnt )
                    {
                        case 0:
//                        learn.program(addrCnt, Type::Keys, channel, row);
                        learn.program(row, Type::Keys, channel, row);
                        break;
                        
                        case 1:
//                        learn.program(addrCnt, Type::KeysVelocity, channel, row);
                        learn.program(row, Type::KeysVelocity, channel, row);
                        break;
                        
                        case 2:
//                        learn.program(addrCnt, Type::KeysPolyPressure, channel, row);
                        learn.program(row, Type::KeysPolyPressure, channel, row);
                        break;
                        
                        default:
                        break;
                    }
                break;
                
                case ChannelType::Percussion:
//                    learn.program(addrCnt, Type::PercussionVelocity, channel, row);
                    learn.program(row, Type::PercussionVelocity, channel, row);
//                    learn.setPercNote(addrCnt, channel, note);
                    learn.setPercNote(row, channel, note);
                break;

                default:
                break;
            }

            ++addrCnt;
        }
	}
}

void note_on(int8_t channel, int8_t note, int8_t velocity) //Moeten bytes zijn, geen int. (met int wordt de data corrupt).
{
    --channel;

    if ( channels.getChannelType(channel) == ChannelType::Percussion )
    {
        Type type = Type::PercussionVelocity;

        if ( address.getState((int8_t)type, channel) )
        {
            int8_t globalAddr = address.get((int8_t)type, channel);
            int8_t row = learn.getRow(globalAddr);
            cv_out(row, velocity, type);
        }
    
        for ( int8_t row = 0; row < 4; ++row )
        {
            if ( note == learn.getPercNote(row, channel) )
            {
                trigCounter[row] = PULSE_LENGTH_MS;
                gate_out(row, true);
                return;
            }
        }
    }
    else
    {
        for ( int8_t t = (int8_t)Type::Keys; t <= (int8_t)Type::KeysPolyPressure; ++t )
        {
            Type type = (Type)t;
            
            if ( address.getState((int8_t)type, channel) )
            {
                int8_t globalAddr = address.get((int8_t)type, channel);
                int8_t polyAddr = learnToPolyMap[globalAddr];
//                MIDI.sendNoteOn(polyAddr, 1, 5);
                
                // Schrijf row.
                int8_t row;

                if ( polyAddr > -1 )
                {
                    if ( channels.getChannelType(channel) == ChannelType::KeysCycling )
                    {
                        if ( polyphony.counter[polyAddr] >= polyphony.width[polyAddr] ) polyphony.counter[polyAddr] = 0;
                        
                        row = polyphony.rowAtCount[polyAddr][ polyphony.counter[polyAddr]++ ];
                    }
                    else row = getRowFromSplit(note, polyAddr);
                }
                else row = learn.getRow(globalAddr);


                // Output.
                static int8_t velocitySave = 0;                
                
                switch ( type )
                {   
                    case Type::Keys:
                        lastNote[row].noteOn(note, velocity);
            
                        velocitySave = lastNote[row].getVelocity();
                        
                        cv_out(row, lastNote[row].getPitch(), type);
                        gate_out(row, true);
                    break;
                
                    case Type::KeysVelocity:
                        cv_out(row, velocitySave, type);
                    break;
                
                    case Type::KeysPolyPressure:
                        learn.setRow(globalAddr, row);
                        rowNote[row] = note;
                    default: break;
                }
            }
        }
    }
}

void note_off(int8_t channel, int8_t note, int8_t velocity)
{
    --channel;

    if ( channels.getChannelType(channel) == ChannelType::Percussion )
    {
                    // Misschien niet nodig.
//            for ( int8_t row = 0; row < 4; ++row )
//            {
        //        if ( note == learn.percNote[channel] )
        //        {
        //            trigCounter[row] = 0;
        //            gate_out(row, false);
//                }
//            }
    }
    else
    {
        for ( int8_t t = (int8_t)Type::Keys; t <= (int8_t)Type::KeysPolyPressure; t = t + 1 )
        {
            Type type = (Type)t;
            
            if ( address.getState((int8_t)type, channel) )
            {
                int8_t globalAddr = address.get((int8_t)type, channel);
                int8_t polyAddr = learnToPolyMap[globalAddr];
                
                // Schrijf row.
                if ( polyAddr > -1 ) //polyphony
                {
                    if ( channels.getChannelType(channel) == ChannelType::KeysCycling )
                    {
//                        static int8_t countHist = 0;
//                        
//                        if ( type == Type::Keys )
//                        {
//                            for ( int8_t c = 0; c < polyphony.width[polyAddr]; ++c )
//                            {
//                                int8_t r = polyphony.rowAtCount[polyAddr][c];
//                                if ( lastNote[r].noteOff(note, velocity) )
//                                {
//                                    keyRow = r;
//                                    countHist = c;
//
//                                    cv_out(r, lastNote[r].getPitch(), type);
//                                    gate_out(r, lastNote[r].getState());
//                                }
//                            }
//                        }
//                        
//                        row = polyphony.rowAtCount[polyAddr][countHist];
//
//                        polyphony.counter[polyAddr] = lastNote[keyRow].getState() ? countHist + 1 : countHist;

                        //............
                        for ( int8_t c = 0; c < polyphony.width[polyAddr]; ++c )
                        {
                            static int8_t keyRowHist[addressAmount] = {0, 0};
                            // Output.
                            note_off_lastStage(globalAddr, type, note, velocity, polyphony.rowAtCount[polyAddr][c], &keyRowHist[c]);

                            polyphony.counter[polyAddr] = lastNote[keyRowHist[c]].getState() ? c + 1 : c;
                        }
                        //........
                    }
                    else
                    {
                        static int8_t keyRow = 0;
                        // Output.
                        note_off_lastStage(globalAddr, type, note, velocity, getRowFromSplit(note, polyAddr), &keyRow);
                    }
                }
                else
                {
                    static int8_t keyRow = 0;
                    // Output.
                    note_off_lastStage(globalAddr, type, note, velocity, learn.getRow(globalAddr), &keyRow);
                }
            }
        }
    }
}

void note_off_lastStage(int8_t globalAddr, Type type, int8_t note, int8_t velocity, int8_t row, int8_t* keyRowPntr)
{
    switch ( type )
    {
        case Type::Keys:
        {
            if ( lastNote[row].noteOff(note, velocity) )
            {
                *keyRowPntr = row;
                cv_out(row, lastNote[row].getPitch(), type);
                gate_out(row, lastNote[row].getState());
            }
        }
        break;

        case Type::KeysVelocity:
            cv_out(row, lastNote[*keyRowPntr].getVelocity(), type);
        break;

        case Type::KeysPolyPressure:
            if ( lastNote[*keyRowPntr].getState() )
            {
                learn.setRow(globalAddr, row);
                rowNote[row] = lastNote[*keyRowPntr].getPitch();
            }
            else rowNote[row] = -1;
        default: break;
    }
}

int8_t getRowFromSplit(int8_t note, int8_t polyAddr)
{
    for ( int8_t c = 0; polyphony.width[polyAddr]; ++c )
    {
        int8_t rowTemp = polyphony.rowAtCount[polyAddr][c];
        
        if ( note <= polyphony.boundary[rowTemp] )
        {
            return rowTemp;
        }
    }
}

void learn_atc(int8_t channel, int8_t aftertouch)
{
    --channel;

    int8_t addrCnt = 0;
    for ( int8_t row = 0; row < 4; ++row )
    {
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn.program(addrCnt++, Type::ChannelPressure, channel, row);
        }
    }
}

void atc(int8_t channel, int8_t aftertouch)
{
    --channel;
    
    Type type = Type::ChannelPressure;

    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        cv_out(learn.getRow(globalAddr), aftertouch, type);
    }
}

void atp(int8_t channel, int8_t note, int8_t aftertouch)
{
    --channel;

    Type type = Type::KeysPolyPressure;

//    static Change <int8_t> ac(-1);
//    if ( ac(address.get((int8_t)type, channel)) ) MIDI.sendNoteOn(address.get((int8_t)type, channel), 0, 5);
    
    if ( address.getState((int8_t)type, channel) )
    {
        int8_t globalAddr = address.get((int8_t)type, channel);
        int8_t row = learn.getRow(globalAddr);

//        static Change <int8_t> rc(-1);
//        if ( rc(row) ) MIDI.sendNoteOn(row, 1, 5);

//        static Change <int8_t> nc(-1);
//        if ( nc(rowNote[row]) ) MIDI.sendNoteOn(rowNote[row], 2, 5);
    
        if (  note == rowNote[row] )
        {
            cv_out(row, aftertouch, type);
        }
    }
}

void learn_control_change(int8_t channel, int8_t number, int8_t control)
{
    --channel;
    
	static bool msb_ready = false;
	static bool nrpn_ready = false;

    int8_t addrCnt = 0;
	for ( int8_t row = 0; row < 4; ++row )
	{
		precision[row] = false;
		
		if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
		{
			switch ( number )
			{
			case 38:
				if ( nrpn_ready && msb_ready )
				{
					precision[row] = true;

					nrpn_ready = false;
					msb_ready = false;
				}
				break;
			case 6:
				if ( nrpn_ready )
				{
                    learn.program(addrCnt, Type::ControlChange, channel, row);

					msb_ready = true;
				}
				break;
			case 98:
				if ( control == 127 )
				{
					break;
				}
				else if ( msb_ready )
				{
					nrpn_lsb_controls[row] = control;

					nrpn_ready = true;
				}
				break;
			case 99:
				if ( control == 127 )
				{
					break;
				}
				else
				{
					nrpn_msb_controls[row] = control;
				}
				break;
			default:
                learn.program(addrCnt, Type::ControlChange, channel, row);

				if ( msb_numbers[row] + 32 == number && msb_ready )
				{
					precision[row] = true;
				}
				
				if ( msb_ready || msb_numbers[row] == number && learn.getChannel(row) == channel )
				{
					msb_ready = false;
				}
				else
				{
					msb_numbers[row] = number;

					msb_ready = true;
				}

				break;
			}
            ++addrCnt;
		}
	}
}

void learn_pitchbend(int8_t channel, int pitch)
{
    --channel;

    int8_t addrCnt = 0;
	for ( int8_t row = 0; row < 4; ++row )
	{
		if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
		{
            learn.program(addrCnt++, Type::PitchBend, channel, row);
		}
	}
}

//test deze

void control_change(int8_t channel, int8_t number, int8_t control)
{
    --channel;
    
    Type type = Type::ControlChange;
    
    static int8_t nrpn_iter[4] = {0, 0, 0,};
    static int8_t msb_controller[4] = {-1, -1, -1, -1};
    
	bool msb_6 = 6 == number;
	bool lsb_38 = 38 == number;
	
	bool nrpn_99 = 99 == number;
	bool nrpn_98 = 98 == number;
    
	const int NRPN_99_CTRL = B00 << 7 | B0100001;
	const int NRPN_98_CTRL = B01 << 7 | B1000001;
	const int MSB_6 = B10 << 7 | B0001001;
	const int LSB_38 = B10 << 7 | B0010001;
	const int MSB = B0000011;
	const int LSB = B0000101;
    
	for ( int8_t row = 0; row < 4; ++row )
	{
		bool cv_main = channel == learn.getChannel(row);
        
		bool msb_num = msb_numbers[row] == number;
		bool lsb_num = msb_numbers[row] + 32 == number;
        
		bool nrpn_msb_ctrl = nrpn_msb_controls[row] == control;
		bool nrpn_lsb_ctrl = nrpn_lsb_controls[row] == control;
        
		switch ( nrpn_iter[row] << 7 | (nrpn_lsb_ctrl & nrpn_98) << 6 | (nrpn_msb_ctrl & nrpn_99) << 5 | lsb_38 << 4 | msb_6 << 3 | lsb_num << 2 | msb_num << 1 | cv_main )
		{
			case NRPN_99_CTRL:
				nrpn_iter[row] = 1;
				return;
            break;
            
			case NRPN_98_CTRL:
				nrpn_iter[row] = 2;
				return;
            break;
             
			case MSB_6:
				if ( precision[row] )
				{
					msb_controller[row] = control;
                    return;
				}
				else
				{
					cv_out(row, (control << 6) + 64, type);
					nrpn_iter[row] = 0;
				}
//				return;//ongebruikt
            break;
            
			case LSB_38:
				cv_out(row, msb_controller[row] << 7 | control, type);
				nrpn_iter[row] = 0;
//				return;//ongebruikt
            break;
            
			case MSB:
				if ( precision[row] )
				{
					msb_controller[row] = control;
                  return;
				} else cv_out(row, (control << 6) + 64, type);
//				return; //ongebruikt
            break;
            
			case LSB:
				cv_out(row, msb_controller[row] << 7 | control, type);
//				return; //ongebruikt
            break;
            
			default:
			break;
		}
	}
}

void pitchbend(int8_t channel, int pitch)
{
    --channel;
    
	Type type = Type::PitchBend;

//    cv_out(learn.getRow(type, channel), pitch, type);
}


void cv_out(int8_t row, int number, Type type)
{
	uint8_t value = number >> CV_RSHIFT[(int8_t)type];
	switch (row)
	{
		case 0: PWM6 = value; break;
		case 1: PWM9 = value; break;
		case 2: PWM10 = value; break;
		case 3: PWM13 = value; break;
        default: break;
	}

	 if ( !MIDI.getThruState() ) {
	 	MIDI.sendControlChange(0, value >> 7, row + 1);
	 	MIDI.sendControlChange(32, value & 127, row + 1);
	 }
}

void gate_out(int8_t row, bool state)
{
	const int8_t GATES[4] = {GATE_1_PIN, GATE_2_PIN, GATE_3_PIN, GATE_4_PIN};

	digitalWrite(GATES[row], state);

	 if ( !MIDI.getThruState() ) {
	 	MIDI.sendNoteOn(0, state, row + 1);
	 }
}

void clock_tick()
{
    digitalWrite(CLOCK_PIN, HIGH);
    clockCounter = PULSE_LENGTH_MS;
//    MIDI.sendRealTime(midi::Clock);
}

void clock_start()
{
	digitalWrite(RESET_PIN, HIGH);	
    resetCounter = PULSE_LENGTH_MS;
}

void clock_continue()
{
}

void clock_stop()
{
	digitalWrite(CLOCK_PIN, LOW);
	digitalWrite(RESET_PIN, LOW);
	gate_out(0, LOW);
	gate_out(1, LOW);
	gate_out(2, LOW);
	gate_out(3, LOW);
}

// void sense_timeout()
// {
// 	clock_stop();
// }
