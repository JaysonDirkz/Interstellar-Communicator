#ifndef MidiLearn_h
#define MidiLearn_h

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

template <typename type_t, int8_t addressAmount>
struct Programmer {
    // Wordt enkel uitgelezen in learn polyphony fase. Met een loop.
    int8_t rows[addressAmount];
    int8_t channels[addressAmount];
    type_t types[addressAmount];
    int8_t notes[addressAmount];
    
    Programmer()
    {
        for( int8_t a = 0; a < addressAmount; ++a )
        {
            rows[a] = -1;
            channels[a] = -1;
            types[a] = (type_t)-1;
            notes[a] = -1;
        }
    }

    void program(int8_t a, type_t t, int8_t c, int8_t r, int8_t n = -1)
    {
        // Write
        rows[a] = r;
        channels[a] = c;
        types[a] = t;
        notes[a] = n;
    }
};

template <int8_t addressAmount, int8_t outputsAmount = 4>
class Polyphony {
    public:
//    int8_t rows[addressAmount][noteAmount];
//    int8_t counter[addressAmount];
    int8_t width[addressAmount];
    int8_t rowAtCount[addressAmount][outputsAmount];

//    int8_t countAtRow[outputsAmount];
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
//            counter[a] = 0;
            width[a] = 0;

            for ( int8_t o = 0; o < outputsAmount; ++o )
            {
//                countAtRow[o] = 0;
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
