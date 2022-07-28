#ifndef MidiLearn_h
#define MidiLearn_h

//enum class Type: int8_t {
//    None, // Te veel ram? Deze kan weg. Gebruik "Size".
//    Keys,// Te veel ram? Deze kan weg. Gebruik None of Size van KeyType class.
//    ChannelPressure,
//    ControlChange,
//    PitchBend,
//    Size
//};

enum class MessageType: int8_t {
    Keys,
    KeysVelocity,
    KeysPolyPressure,
    PercussionVelocity,
    ChannelPressure,
    ControlChange,
    PitchBend,
    Size
};

//enum class KeyType: int8_t {
//    PercussionVelocity,
//    KeysPitch,
//    KeysVelocity,
//    KeysPolyPressure,
//    Size
//};

enum class ChannelType: int8_t {
    Keys,
    Percussion,
    Size
};

// class Channels : public ChannelTypes {
class Channels {
    private:
    ChannelType channelTypes[16];
    const uint8_t channelTypesSize = sizeof(channelTypes) / sizeof(channelTypes[0]);

    public:
    Channels()
    {
        for ( int8_t c = 0; c < channelTypesSize; ++c )
        {
            if ( c == 9 ) channelTypes[c] = ChannelType::Percussion;
            else channelTypes[c] = ChannelType::Keys;
        }
    }

    ChannelType getChannelType(int8_t c)
    {
        return channelTypes[c];
    }
};

template <int8_t addressAmount, int8_t rowAmount, int8_t percChannelAmount = 4>
class Programmer {
    private:
    // Wordt enkel uitgelezen in learn polyphony fase. Met een loop.
    int8_t rows[addressAmount];
    int8_t channels[addressAmount];
    MessageType types[addressAmount];
    
    int8_t percNotes[rowAmount][percChannelAmount];

    void setType(int8_t a, MessageType t)
    {
        types[a] = t;
    }
    
    void setChannel(int8_t a, int8_t c)
    {
        channels[a] = c;
    }
    
    public:
    Programmer()
    {
        for( int8_t a = 0; a < addressAmount; ++a )
        {
            rows[a] = -1;
            channels[a] = -1;
            types[a] = MessageType::Size;
        }

        for( int8_t r = 0; r < rowAmount; ++r )
        {
            for( int8_t c = 0; c < percChannelAmount; ++c )
            {
                percNotes[r][c] = -1;
            }
        }
    }

    void program(int8_t addr, MessageType t, int8_t c, int8_t row)
    {
        // Write
        setChannel(addr, c);
        setType(addr, t);
        setRow(addr, row);
    
        if ( t == MessageType::Keys )
        {
            // Clear row of percNotes.
            setPercNote(getRow(addr), 8, -1);
        }
    }

    MessageType getType(int8_t a)
    {
        return types[a];
    }

    int8_t getChannel(int8_t a)
    {
        return channels[a];
    }

    int8_t getRow(int8_t a)
    {
        return rows[a];
    }
    
    int8_t getPercNote(int8_t r, int8_t c)
    {
        return percNotes[r][c - 8];
    }

    void setRow(int8_t a, int8_t row)
    {
        rows[a] = row;
    }

    void setPercNote(int8_t r, int8_t c, int8_t note)
    {
        // Clear current address.
        for ( int8_t j = 0; j < percChannelAmount; ++j )
        {
            percNotes[r][j] = -1;
        }

        // Write
        percNotes[r][c - 8] = note;
    }
};

template <int8_t addressAmount, int8_t outputsAmount = 4, uint8_t noteAmount = 128>
class Polyphony {
    public:
//    int8_t rows[addressAmount][noteAmount];
    int8_t counter[addressAmount];
    int8_t width[addressAmount];
    int8_t rowAtCount[addressAmount][outputsAmount];

    int8_t countAtRow[outputsAmount];
    int8_t boundary[outputsAmount];

    Polyphony()
    {
        // for ( int8_t a = 0; a < addressAmount; ++a )
        // {
        //     for ( uint8_t n = 0; n < noteAmount; ++n )
        //     {
        //        rows[a][n] = -1;
        //     }
        // }

        for ( int8_t a = 0; a < addressAmount; ++a )
        {
            counter[a] = 0;
            width[a] = 0;

            for ( int8_t o = 0; o < outputsAmount; ++o )
            {
                countAtRow[o] = 0;
                rowAtCount[a][o] = -1;
            }
        }

        for ( int8_t o = 0; o < outputsAmount; ++o )
        {
            boundary[o] = 127;
        }
    }
};

// enum Type {
//         Keys,
//         KeysVelocity,
//         KeysPolyPressure,
//         Percussion,
//         PercussionVelocity,
//         ChannelPressure,
//         ControlChange,
//         PitchBend
//     };

// union MidiConstruct {
//     struct KeyConstruct {
//         int8_t channel = 0;

//         enum {
//             keys,
//             keysVelocity,
//             keysPolyPressure,
//             channelPressure,
//             pitchBend
//         } type = keys;
        
//         union {
//             bool mono = true;
//             int8_t polyphonic = 0;
//             int8_t keysplit = 0;
//         } polyType.mono = true;
//     };

//     struct PercussionConstruct {
//         int8_t channel = 0;

//         enum {
//             percussion,
//             percussionVelocity,
//         } type = percussion;
        
//         enum {
//             none,
//             dual,
//             all
//         } choke;
//     };

//     struct ControlChangeConstruct {
//         int8_t channel = 0;

//         union Type {
//             struct {
//                 int8_t number = 0;
//             } basic;

//             struct {
//                 int8_t msb = 0;
//                 int8_t lsb = 32;
//             } hires;

//             struct {
//                 int8_t msb = 0;
//                 bool nrpn_98 = false;
//                 bool nrpn_99 = false;
//             } basicNrpn;

//             struct {
//                 int8_t msb = 0;
//                 int8_t lsb = 32;
//                 bool nrpn_98 = false;
//                 bool nrpn_99 = false;
//             } hiresNrpn;
//         };
//     };
// };

#endif
