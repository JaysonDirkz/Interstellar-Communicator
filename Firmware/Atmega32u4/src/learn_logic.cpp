int8_t atpAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t atcAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t ccAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t pbAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.

CvBase cvBases[5]; // First one is not used.
WriteFunction_t *cvWrites[5] = {0, 0, 0, 0, 0}; // First one is not used.

int8_t globalAddrCnt = 0;

void learn()
{
    globalAddrCnt = 0; // Reset global address counter.
    // address.fill(); // Reset all global addresses.
    // clearRowData(); // Reset global rowNote data.
    // clearKeysActive();

    int8_t channel = MIDI.getChannel() - 1;



    
    // Program the note type, velocity or aftertouch for each row.
    globalAddrCnt = 0;
	for ( int8_t row = 0; row < 4; ++row )
	{
        if ( (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            // Be sure that something is going to be written in this row before the code comes here!
            // Clear.
            for_each(atpAddresses, if row + 1 then 0);
            for_each(atcAddresses, if row + 1 then 0);
            for_each(ccAddresses, if row + 1 then 0);
            for_each(pbAddresses, if row + 1 then 0);
            for_each(keyAddresses, if row + 1 then 0);

            switch ( MIDI.getType() ) {
                case NoteOn:
                case NoteOff:
                case AfterTouchPoly:
                case AfterTouchChannel:
                case PitchBend:
                case ControlChange:
                    learn(Midi.getType(), MIDI.getChannel());
                break;
            }

            int8_t last_voice;

            switch ( channels.getChannelType(channel) )
            {
                case ChannelType::KeysCycling:
                case ChannelType::KeysSplit:
                    switch ( globalAddrCnt )
                    {
                        case 0:
                            //Clear more stuff maybe?

                            last_voice = row;

                            // Set.
                            keyAddresses[MIDI.getChannel()] = row + 1;
                            keysBases[row + 1].row_g_p[last_voice] = row;
                            keysBases[row + 1].voice_next = 0;
                            //.....etc
                        break;
                        
                        case 1:

                            keysBases[row + 1].row_v[last_voice] = row;
                            last_voice = row;
                            keysBases[row + 1].row_v[last_voice] = row;
                        break;
                        
                        case 2:
                        // Bij ontvangst van polyaftertouch wordt channelaftertouch hier naar toe veranderd.
                        // Maar kan niet terug veranderd worden naar channlpressure. Dus polypressure heeft prio.
                            learn.program(row, MessageType::ChannelPressure, channel, row);

                        // Voor note herkenning bij polypressure.
                        // Zorgt ervoor dat polypressure alleen tijdens de huidige leerfase ingeleerd kan worden. (Want rowNote wordt weer gereset.
                            rowNote[row] = note;
//                            keysActive[learnToPressureMap[row]].noteOn(note); // Niet hier gebruiken want learnToPressureMap is nog niet gemapped.
                        break;

                        case 3:
                            learn.program(row, MessageType::PitchBend, channel, row);
                        break;
                        
                        default:
                        break;
                    }
                break;
                
                case ChannelType::Percussion:
                    keyAddresses[MIDI.getChannel()] = row + 1;
                    keysBases[row + 1].row_g_p[last_voice] = row;
                break;

                default:
                break;
            }

            ++globalAddrCnt;
        }
	}


    // writeGlobalAddresses();
    // writeKeyAftertouchMapping();
    // writePolyAddresses();
    // checkPolyphony();
}


void learn_atc(uint8_t channel, uint8_t aftertouch)
{
//    --channel;
//
//    for ( int8_t row = 0; row < 4; ++row )
//    {
//        if (
//            learn.getChannel(row) == channel
//            and
//            learn.getType(row) == MessageType::ChannelPressure
//            and
//            (~PINF & rowTo_32u4PINF_bit[row]) > 0
//        ) {
////            address.set((int8_t)MessageType::ChannelPressure, channel, row);
//        }
//    }
}

void learn_atp(uint8_t channel, uint8_t note, uint8_t aftertouch)
{
    --channel;

    for ( int8_t row = 0; row < 4; ++row )
    {
        if (
            learn.getChannel(row) == channel
            and
            learn.getType(row) == MessageType::ChannelPressure
            and
            (~PINF & rowTo_32u4PINF_bit[row]) > 0
            and rowNote[row] == note
//            and keysActive[learnToPressureMap[row]].getPosition(note) > -1 // Niet gebruiken hier want learnToPressureMap is nog niet gemapped.
        ) {
            learn.program(row, MessageType::KeysPolyPressure, channel, row);
        }
    }
}

void learn_control_change(uint8_t channel, uint8_t number, uint8_t control)
{
    --channel;
    
    static bool msb_ready = false;
    static bool nrpn_ready = false;

    if ( globalAddrCnt == 0 )
    {
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
                    learn.program(row, MessageType::ControlChange, channel, row);

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
                learn.program(row, MessageType::ControlChange, channel, row);

                if ( msb_numbers[row] + 32 == number && msb_ready )
                {
                    precision[row] = true;
                }
                
                if ( msb_ready || (msb_numbers[row] == number && learn.getChannel(row) == channel) )
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
        }
    }
    }
}

void learn_pitchbend(uint8_t channel, int pitch)
{
    --channel;

    if ( globalAddrCnt == 0 )
    {
    for ( int8_t row = 0; row < 4; ++row )
    {
        if (  (~PINF & rowTo_32u4PINF_bit[row]) > 0 )
        {
            learn.program(row, MessageType::PitchBend, channel, row);
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