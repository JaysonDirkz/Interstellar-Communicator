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

template <int8_t addressAmount, int8_t rowAmount>
class Programmer {
    private:
    // Wordt enkel uitgelezen in learn polyphony fase. Met een loop.
    int8_t rows[addressAmount];
    int8_t channels[addressAmount];
    MessageType types[addressAmount];

    int8_t percNotes[rowAmount];
    bool percVel[rowAmount];

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
            percNotes[r] = -1;
        }
    }

    void program(int8_t addr, MessageType t, int8_t c, int8_t row)
    {
        // Write
        setChannel(addr, c);
        setType(addr, t);
        setRow(addr, row);

        // Clear row of perc learns.
        if ( t == MessageType::Keys )
            setPercNote(getRow(addr), -1);
        setPercVel(getRow(addr), false);
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
    
    int8_t getPercNote(int8_t r)
    {
        return percNotes[r];
    }

    bool getPercVel(int8_t r)
    {
        return percVel[r];
    }

    void setRow(int8_t a, int8_t row)
    {
        rows[a] = row;
    }

    void setPercNote(int8_t r, int8_t note)
    {
        percNotes[r] = note;
    }

    void setPercVel(int8_t r, bool state)
    {
        percVel[r] = state;
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

#endif
