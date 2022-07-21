void learn()
{
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
//                    learn.program(globalAddrCnt, MessageType::PercussionVelocity, channel, row);
                    learn.program(row, MessageType::PercussionVelocity, channel, row);
//                    learn.setPercNote(globalAddrCnt, channel, note);
                    learn.setPercNote(row, channel, note);
                break;

                default:
                break;
            }

            ++globalAddrCnt;
        }
	}
}