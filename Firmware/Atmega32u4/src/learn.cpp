#ifndef learn_parse_h
#define learn_parse_h

void learn_parse_keys(u8 c, u8 n, u8 v, u8 r, u8 parse_cntr)
{
    i8 r_base;
    if ( parse_cntr == 0 ) r_base = r;

    if ( c >= 9 && c <= 12 ) // Percussion
    {
        if ( keyAddresses[c] == -1 ) {
            keyAddresses[c] = r; // Link base address to midi channel.
        }
        i8 kA = keyAddresses[c];

        // Clear note with the same row on this address.
        for ( i8 i = 0; i < 128; ++i ) {
            if ( note_addresses[kA][i] == r ) {
                note_addresses[kA][i] = -1;
            }
        }

        note_adresses[kA][n] = r;
        notes_bases[r].row = r;

        // Count amount of notes programmed on this address
        u8 n_cnt = 0;
        for ( i8 i = 0; i < 128; ++i ) {
            if ( note_addresses[kA][i] > -1 ) n_cnt += 1;
        }

        // Attach function:
        if ( c == 9 || n_cnt == 1 ) { // Attach normal function.
            notes_writes[r] = on_trigger_velocity;
        }
        else if ( c == 10 ) { // Attach choke function to 2 rows.
            note_bases[r].chokegroup_other_cv_pins = &note_shared.chokegroup_other_cv_pins[0];

            u8 n_cnt = 0;
            for ( i8 i = 0; i < 128; ++i ) {
                i8 r_any = note_addresses[kA][i];
                if ( r_any > -1 ) {
                    notes_writes[r_any] = on_trigger_velocity_choke; // Attach base function.
                    note_bases[r_any].chokegroup_other_pins_amount = 1;
                    if ( r_any != r ) note_bases[r].chokegroup_other_cv_pins[0] = r;
                    if ( ++n_cnt == 2 ) break;
                }
            }
        }
        else { // Attach choke function to 2 or more rows.
            for ( i8 i = 0; i < 128; ++i ) {
                i8 r = note_addresses[kA][i];
                if ( r > -1 ) {
                    notes_writes[r] = on_trigger_velocity_choke; // Attach base function.
                    //.....
                }
            }
        }
    }
    else
    {
        switch ( parse_cntr )
        {
            case 0: // Program pitch.
            {
                u8 voice = 0;

                keyAddresses[c] = r; // Link base address to midi channel.

                // Unique for keys.
                for ( i8 i = 0; i < 4; ++i ) { // Attach on_keys function to all notes.
                    notes_writes[r] = on_keys;
                }

                notes_bases[r].address = r;
                notes_bases[r].voice_amount = 1;
                notes_bases[r].voice_max = voice_amount - 1;
                notes_bases[r].voice_next = 0;
                notes_bases[r].row_g_p = &keysGlobal.row_g_p[voice];
                notes_bases[r].row_g_p[voice] = r;
                notes_bases[r].last_note = &keysGlobal.last_note[voice];
                notes_bases[r].write = write_gate_pitch; // Attach function.
            }
            break;
            
            case 1: // Program velocity
            {
                u8 voice = 0;

                notes_bases[r_base].row_v = &keysGlobal.row_v[voice];
                notes_bases[r_base].row_v[voice] = r;
            }
            break;
            
            case 2: // Program aftertouch (eerst AfterTouchChannel)
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

void learn_parse_atp(u8 channel, auto note, auto aftertouch)
{
    --channel;

    for ( i8 r = 0; r < 4; ++r )
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

void learn_parse_atc(u8 channel, u8 aftertouch)
{
//    --channel;
//
//    for ( i8 r = 0; r < 4; ++r )
//    {
//        if (
//            learn.getChannel(r) == channel
//            and
//            learn.getType(r) == MessageType::ChannelPressure
//            and
//            (~PINF & rowTo_32u4PINF_bit[r]) > 0
//        ) {
////            address.set((i8)MessageType::ChannelPressure, channel, r);
//        }
//    }
}

void learn_parse_control_change(u8 channel, u8 number, u8 control)
{
    --channel;
    
    static bool msb_ready = false;
    static bool nrpn_ready = false;

    if ( parse_cntr == 0 )
    {
    for ( i8 r = 0; r < 4; ++r )
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

void learn_parse_pitchbend(u8 channel, int pitch)
{
    --channel;

    if ( parse_cntr == 0 )
    {
    for ( i8 r = 0; r < 4; ++r )
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
//    i8 globalAddrCount = 0;
    for ( i8 addr = 0; addr < addressAmount; ++addr )
    {
        if ( learn.getRow(addr) > -1 )
        {
            // If poly, only last address is set.
//            address.setUnique((i8)learn.getType(addr), learn.getChannel(addr), globalAddrCount);
            address.set((i8)learn.getType(addr), learn.getChannel(addr), addr);
//            MIDI.sendNoteOn(address.get((i8)learn.getType(addr), learn.getChannel(addr)), 61, 5); // debug
//            ++globalAddrCount;
        }
    }
}

void writeKeyAftertouchMapping()
{
    // Reset
    for ( i8 addr = 0; addr < addressAmount; ++addr )
    {
        learnToPressureMap[addr] = -1;
    }

    i8 count = 0;
    
    for ( i8 channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            for ( i8 addr = 0; addr < addressAmount; ++addr )
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
    for ( i8 addr = 0; addr < addressAmount; ++addr )
    {
        learnToPolyMap[addr] = -1;
    }
    
    // Include in polyphony if type and channel match more than 1 time found.
    i8 polyAddrCount = 0;
    for ( i8 channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            for ( i8 type = (i8)MessageType::Keys; type <= (i8)MessageType::KeysPolyPressure; ++type )
            {
                i8 polyWidth = 0;
                
                for ( i8 addr = 0; addr < addressAmount; ++addr )
                {
                    if ( learn.getType(addr) == (MessageType)type and learn.getChannel(addr) == channel )
                    {
                        learnToPolyMap[addr] = polyAddrCount;
                        ++polyWidth;

//                        MIDI.sendNoteOn(address.get((i8)type, channel), 62, 5); // debug

                        // Reset het poly adres als het mono blijkt te zijn.
                        // Niet mono? Dan poly adres counter incrementen.
                        if ( addr == address.get((i8)type, channel) )
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
    for ( i8 a = 0; a < polyAddressAmount; ++a )
    {
        // Reset width.
        polyphony.width[a] = 0;

        // Reset counter
        polyphony.counter[a] = 0;

        // Reset count and r info.
        for ( i8 count = 0; count < 4; ++count )
        {
            polyphony.rowAtCount[a][count] = -1;

            i8 r = count;
            polyphony.countAtRow[r] = 0;
        }
    }

    // Write all width
    for ( i8 a = 0; a < addressAmount; ++a )
    {
        i8 polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            ++polyphony.width[polyAddr];
        }
    }

    // Write width for KeysVelocity and KeysPolyPressure (not higher than Key width).
    for ( i8 channel = 0; channel < 16; ++channel )
    {
        if ( channels.getChannelType(channel) != ChannelType::Percussion )
        {
            i8 keyAddr = address.get((i8)MessageType::Keys, channel);
            i8 velAddr = address.get((i8)MessageType::KeysVelocity, channel);
            i8 pressAddr = address.get((i8)MessageType::KeysPolyPressure, channel);

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
    for ( i8 a = 0; a < addressAmount; ++a )
    {
        i8 polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            i8 count = polyphony.counter[polyAddr]++;
            
            if ( count < polyphony.width[polyAddr] ) polyphony.rowAtCount[polyAddr][count] = learn.getRow(a);
        }
    }

    // Write boundary.
    for ( i8 a = 0; a < addressAmount; ++a )
    {
        i8 polyAddr = learnToPolyMap[a];
        
        if ( polyAddr > -1 )
        {
            if ( channels.getChannelType(learn.getChannel(a)) == ChannelType::KeysSplit )
            {
                i8 r = learn.getRow(a);
                i8 width = polyphony.width[polyAddr];
                
                if ( width > 1 )
                {
                    // [width][r]
                    const i8 boundariesHigh[3][4] = {
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
    for ( i8 a = 0; a < polyAddressAmount; ++a )
    {
        polyphony.counter[a] = 0;
    }
}

void clearRowData()
{
    for ( i8 r = 0; r < rowAmount; ++r )
    {
        rowNote[r] = -1;
    }
}

void clearKeysActive()
{
    for ( i8 addr = 0; addr < keysActiveSize; ++addr )
    {
        keysActive[addr].reset();
    }
}

void learn_parse_type(u8 t, c, d1, d2)
{
    // address.fill(); // Reset all global addresses.
    // clearRowData(); // Reset global rowNote data.
    // clearKeysActive();

    // Program the note type, velocity or aftertouch for each r.
    u8 button_cntr = 0;
	for ( u8 r = 0; r < 4; ++r )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[r]) > 0 )
        {
            // Be sure that something is going to be written in this r before the code comes here!
            // Clear.
            for_each(atpAddresses, if r then 0);
            for_each(atcAddresses, if r then 0);
            for_each(ccAddresses, if r then 0);
            for_each(pbAddresses, if r then 0);
            for_each(keyAddresses, if r then 0);

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

u8 learn_parse_midi()
{
    u8 b_now = get_bitfield_buttons();
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

    static u8 b_saved = 0;
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