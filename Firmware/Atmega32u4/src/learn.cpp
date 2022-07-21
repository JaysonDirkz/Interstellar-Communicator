#ifndef learn_parse_h
#define learn_parse_h

int8_t atpAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t atcAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t ccAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t pbAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.

CvBase cvBases[5]; // First one is not used.
WriteFunction_t *cvWrites[5] = {0, 0, 0, 0, 0}; // First one is not used.

void learn_parse_keys(uint_t c, n, v, r, voice)
{
    uint8_t a = r + 1; // address.

    if ( parse_cntr > 0 ) last_voice = parse_cntr - 1;

    if ( c >= 9 && c <= 12 ) // Percussion
    {
        keyAddresses[c] = a;
        keysBases[a].row = r;
        
        keysBases[a].noteRouter = keysGlobal[r].noteRouters;
        keysBases[a].noteRouter[n] = 
    }
    else
    {
        switch ( parse_cntr )
        {
            case 0:
                //Clear more stuff maybe?

                voice = r;

                // Set.
                keyAddresses[MIDI.getChannel()] = r + 1;
                keysBases[r + 1].row_g_p[parse_cntr] = r;
                keysBases[r + 1].voice_next = 0;
                //.....etc
            break;
            
            case 1:

                keysBases[r + 1].row_v[voice] = r;
                keysBases[r + 1].row_v[voice] = r;
            break;
            
            case 2:
            // Bij ontvangst van polyaftertouch wordt channelaftertouch hier naar toe veranderd.
            // Maar kan niet terug veranderd worden naar channlpressure. Dus polypressure heeft prio.
                learn.program(r, MessageType::ChannelPressure, channel, r);

            // Voor note herkenning bij polypressure.
            // Zorgt ervoor dat polypressure alleen tijdens de huidige leerfase ingeleerd kan worden. (Want rowNote wordt weer gereset.
                rowNote[r] = note;
//                            keysActive[learnToPressureMap[r]].noteOn(note); // Niet hier gebruiken want learnToPressureMap is nog niet gemapped.
            break;

            case 3:
                learn.program(r, MessageType::PitchBend, channel, r);
            break;
            
            default:
            break;
        }
    }
}

void learn_parse_atp(uint8_t channel, uint8_t note, uint8_t aftertouch)
{
    --channel;

    for ( int8_t r = 0; r < 4; ++r )
    {
        if (
            learn.getChannel(r) == channel
            and
            learn.getType(r) == MessageType::ChannelPressure
            and
            (~PINF & rowTo_32u4PINF_bit[r]) > 0
            and rowNote[r] == note
//            and keysActive[learnToPressureMap[r]].getPosition(note) > -1 // Niet gebruiken hier want learnToPressureMap is nog niet gemapped.
        ) {
            learn.program(r, MessageType::KeysPolyPressure, channel, r);
        }
    }
}

void learn_parse_atc(uint8_t channel, uint8_t aftertouch)
{
//    --channel;
//
//    for ( int8_t r = 0; r < 4; ++r )
//    {
//        if (
//            learn.getChannel(r) == channel
//            and
//            learn.getType(r) == MessageType::ChannelPressure
//            and
//            (~PINF & rowTo_32u4PINF_bit[r]) > 0
//        ) {
////            address.set((int8_t)MessageType::ChannelPressure, channel, r);
//        }
//    }
}

void learn_parse_control_change(uint8_t channel, uint8_t number, uint8_t control)
{
    --channel;
    
    static bool msb_ready = false;
    static bool nrpn_ready = false;

    if ( parse_cntr == 0 )
    {
    for ( int8_t r = 0; r < 4; ++r )
    {
        precision[r] = false;
        
        if (  (~PINF & rowTo_32u4PINF_bit[r]) > 0 )
        {
            switch ( number )
            {
            case 38:
                if ( nrpn_ready && msb_ready )
                {
                    precision[r] = true;

                    nrpn_ready = false;
                    msb_ready = false;
                }
                break;
            case 6:
                if ( nrpn_ready )
                {
                    learn.program(r, MessageType::ControlChange, channel, r);

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
                    nrpn_lsb_controls[r] = control;

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
                    nrpn_msb_controls[r] = control;
                }
                break;
            default:
                learn.program(r, MessageType::ControlChange, channel, r);

                if ( msb_numbers[r] + 32 == number && msb_ready )
                {
                    precision[r] = true;
                }
                
                if ( msb_ready || (msb_numbers[r] == number && learn.getChannel(r) == channel) )
                {
                    msb_ready = false;
                }
                else
                {
                    msb_numbers[r] = number;

                    msb_ready = true;
                }

                break;
            }
        }
    }
    }
}

void learn_parse_pitchbend(uint8_t channel, int pitch)
{
    --channel;

    if ( parse_cntr == 0 )
    {
    for ( int8_t r = 0; r < 4; ++r )
    {
        if (  (~PINF & rowTo_32u4PINF_bit[r]) > 0 )
        {
            learn.program(r, MessageType::PitchBend, channel, r);
        }
    }
    }
}


void writeGlobalAddresses()
{
    // Write addresses for learned types.
//    int8_t globalAddrCount = 0;
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        if ( learn.getRow(addr) > -1 )
        {
            // If poly, only last address is set.
//            address.setUnique((int8_t)learn.getType(addr), learn.getChannel(addr), globalAddrCount);
            address.set((int8_t)learn.getType(addr), learn.getChannel(addr), addr);
//            MIDI.sendNoteOn(address.get((int8_t)learn.getType(addr), learn.getChannel(addr)), 61, 5); // debug
//            ++globalAddrCount;
        }
    }
}

void writeKeyAftertouchMapping()
{
    // Reset
    for ( int8_t addr = 0; addr < addressAmount; ++addr )
    {
        learnToPressureMap[addr] = -1;
    }

    int8_t count = 0;
    
    for ( int8_t channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            for ( int8_t addr = 0; addr < addressAmount; ++addr )
            {
                if ( learn.getType(addr) == MessageType::KeysPolyPressure and learn.getChannel(addr) == channel )
                {
                    MIDI.sendNoteOn(addr, 10, 5); //debug
                    learnToPressureMap[addr] = count;
                    ++count;
                }
            }
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
            for ( int8_t type = (int8_t)MessageType::Keys; type <= (int8_t)MessageType::KeysPolyPressure; ++type )
            {
                int8_t polyWidth = 0;
                
                for ( int8_t addr = 0; addr < addressAmount; ++addr )
                {
                    if ( learn.getType(addr) == (MessageType)type and learn.getChannel(addr) == channel )
                    {
                        learnToPolyMap[addr] = polyAddrCount;
                        ++polyWidth;

//                        MIDI.sendNoteOn(address.get((int8_t)type, channel), 62, 5); // debug

                        // Reset het poly adres als het mono blijkt te zijn.
                        // Niet mono? Dan poly adres counter incrementen.
                        if ( addr == address.get((int8_t)type, channel) )
                        {
                            if ( polyWidth == 1  ) learnToPolyMap[addr] = -1;
                            else ++polyAddrCount;
//                            MIDI.sendNoteOn(addr, 63, 5); // debug
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

        // Reset count and r info.
        for ( int8_t count = 0; count < 4; ++count )
        {
            polyphony.rowAtCount[a][count] = -1;

            int8_t r = count;
            polyphony.countAtRow[r] = 0;
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
            int8_t keyAddr = address.get((int8_t)MessageType::Keys, channel);
            int8_t velAddr = address.get((int8_t)MessageType::KeysVelocity, channel);
            int8_t pressAddr = address.get((int8_t)MessageType::KeysPolyPressure, channel);

            if ( keyAddr > -1 )
            {
//                if ( velAddr > -1 ) polyphony.width[velAddr] = std::min(polyphony.width[keyAddr], polyphony.width[velAddr]);
//                if ( pressAddr > -1 ) polyphony.width[pressAddr] = std::min(polyphony.width[keyAddr], polyphony.width[pressAddr]);
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
                int8_t r = learn.getRow(a);
                int8_t width = polyphony.width[polyAddr];
                
                if ( width > 1 )
                {
                    // [width][r]
                    const int8_t boundariesHigh[3][4] = {
                        {63, 127, 127, 127},
                        {42, 85, 127, 127},
                        {31, 63, 95, 127}
                    };
                    
                    polyphony.boundary[r] = boundariesHigh[width - 2][r];
                }
                else polyphony.boundary[r] = 127;
            }
        }
    }
    
    // Clear all counters.
    for ( int8_t a = 0; a < polyAddressAmount; ++a )
    {
        polyphony.counter[a] = 0;
    }
}

void clearRowData()
{
    for ( int8_t r = 0; r < rowAmount; ++r )
    {
        rowNote[r] = -1;
    }
}

void clearKeysActive()
{
    for ( int8_t addr = 0; addr < keysActiveSize; ++addr )
    {
        keysActive[addr].reset();
    }
}

void learn_parse_type(uint8_t t, c, d1, d2)
{
    // address.fill(); // Reset all global addresses.
    // clearRowData(); // Reset global rowNote data.
    // clearKeysActive();

    // Program the note type, velocity or aftertouch for each r.
    uint8_t button_cntr = 0;
	for ( uint8_t r = 0; r < 4; ++r )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[r]) > 0 )
        {
            // Be sure that something is going to be written in this r before the code comes here!
            // Clear.
            for_each(atpAddresses, if r + 1 then 0);
            for_each(atcAddresses, if r + 1 then 0);
            for_each(ccAddresses, if r + 1 then 0);
            for_each(pbAddresses, if r + 1 then 0);
            for_each(keyAddresses, if r + 1 then 0);

            switch ( t ) {
                case Midi::NoteOn:
                    learn_parse_keys(c, d1, d2, r, button_cntr)
                break;

                case Midi::AfterTouchPoly:
                    learn_parse_atp(c, d1, d2, r)
                break;

                case Midi::AfterTouchChannel: // Only programmed if poly not yet programmed.
                    learn_parse_atc(c, d1, r)
                break;

                case PitchBend:
                    learn_parse_pb(c, d1, d2, r)
                break;

                case ControlChange:
                    learn_parse_cc(c, d1, d2, r)
                break;
            }

            ++button_cntr;
        }
	}


    // writeGlobalAddresses();
    // writeKeyAftertouchMapping();
    // writePolyAddresses();
    // checkPolyphony();
}

uint8_t learn_parse_midi()
{
    uint8_t b_now = get_bitfield_buttons();
    if ( MIDI.read() ) {
        auto type = MIDI.getType();
        switch ( type ) {
            case NoteOn:
            case NoteOff:
            case AfterTouchPoly:
            case AfterTouchChannel:
            case PitchBend:
            case ControlChange:
            {
                learn_parse_type(type, MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
            }
            break;
        }
    }

    static uint8_t b_saved = 0;
    bool b_changed = b_now != b_saved; b_saved = b_now;
    if ( b_changed && b_now == 0 ) {
        // Save to EEPROM.
        delay(200); // against too much writes.
        // cvBases.
        // keysBases.
        // keysGlobal.
        // all addresses.
    }

    return b_now;
}

#endif