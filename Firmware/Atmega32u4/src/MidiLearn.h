#ifndef MidiLearn_h
#define MidiLearn_h

enum class MessageType: int8_t {
    Keys,
    KeysVelocity,
    KeysPolyPressure,
//    PercussionVelocity,
    ChannelPressure,
    ControlChange,
    PitchBend,
    Size
};

enum class ChannelType: int8_t {
    Keys,
    Percussion,
    Size
};

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

template <int8_t addressAmount>
class Programmer {
    private:
    // Wordt enkel uitgelezen in learn polyphony fase. Met een loop.
    int8_t rows[addressAmount];
    int8_t channels[addressAmount];
    MessageType types[addressAmount];

    int8_t percNotes[addressAmount];
    bool percVel[addressAmount];

//    void setType(int8_t a, MessageType t)
//    {
//        types[a] = t;
//    }
//    
//    void setChannel(int8_t a, int8_t c)
//    {
//        channels[a] = c;
//    }
    
    public:
    Programmer()
    {
        for( int8_t a = 0; a < addressAmount; ++a )
        {
            rows[a] = -1;
            channels[a] = -1;
            types[a] = MessageType::Size;
        }

        for( int8_t a = 0; a < addressAmount; ++a )
        {
            percNotes[a] = -1;
            percVel[a] = false;

//            DEBUG_OUT_FAST("percVellearn_1", getPercVel(r));
        }
    }

    void program(int8_t addr, MessageType t = MessageType::Size, int8_t c = -1, int8_t row = -1)
    {
        // Write
        channels[addr] = c;
        types[addr] = t;
        rows[addr] = row;

        // Clear row of perc learns.
        if ( t == MessageType::Keys || t == MessageType::Size )
            percNotes[addr] = -1;
        percVel[addr] = false;
    }

    inline MessageType getType(int8_t a)
    {
        return types[a];
    }

    inline int8_t getChannel(int8_t a)
    {
        return channels[a];
    }

    inline int8_t getRow(int8_t a)
    {
        return rows[a];
    }
    
    inline int8_t getPercNote(int8_t a)
    {
        return percNotes[a];
    }

    inline bool getPercVel(int8_t a)
    {
        return percVel[a];
    }

    void setRow(int8_t a, int8_t row)
    {
        rows[a] = row;
    }

    void setPerc(int8_t a, int8_t note, bool velState)
    {
        program(a); //clear
        percNotes[a] = note;
        percVel[a] = velState;
    }

//    void setPercVel(int8_t r, bool state)
//    {
//        percVel[r] = state;
////        DEBUG_OUT_FAST("percVellearn", getPercVel(r));
//    }
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

#endif
