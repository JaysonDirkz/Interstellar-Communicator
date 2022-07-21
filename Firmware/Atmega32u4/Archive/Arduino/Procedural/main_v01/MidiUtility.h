#ifndef MidiUtility_h
#define MidiUtility_h

namespace DirkXoniC
{
//debug
const uint8_t debugSize = 50;
char debug[debugSize];

template <uint8_t keyMaxAmount = 10> class ActiveKeysFast2 {
    private:
    int8_t keyPositions[128];
    const uint8_t keyPositionsSize = sizeof(keyPositions) / sizeof(keyPositions[0]);
    
    int8_t keyAtPositions[keyMaxAmount];
    int8_t queue[keyMaxAmount];

    uint8_t queueCount = 0;
    uint8_t queueEnd = 0;
    
    uint8_t amount = 0;

    public:
    ActiveKeysFast()
    {
        reset();
    }

    void reset()
    {
        queueCount = 0;
        queueEnd = 0;
        amount = 0;

        for ( uint8_t i = 0; i < keyPositionsSize; ++i )
        {
            keyPositions[i] = -1;
        }

        for ( uint8_t i = 0; i < keyMaxAmount; ++i )
        {
            keyAtPositions[i] = -1;
            queue[i] = 0;
        }
    }

    bool noteOn(int8_t key)
    {
        bool adressIsFree = keyPositions[key] == -1 && amount < keyMaxAmount;
        if ( adressIsFree ) // key not yet added and amount limit not reached
        {
            if ( queueCount == queueEnd )
            {
                keyAtPositions[amount] = key;

                keyPositions[key] = amount;

                if ( debugState )
                {
                    snprintf(debug, debugSize, "ActKey On  key: %d, am: %d", key, amount);
                    MIDI.sendSysEx(debugSize, (uint8_t *)debug);
                }

                queueCount = 0;
                queueEnd = 0;
            }
            else
            {
                keyAtPositions[queue[queueCount]] = key;

                keyPositions[key] = queue[queueCount];

                if ( debugState )
                {
                    snprintf(debug, debugSize, "ActKey On  key: %d, am: %d, qC: %d, q: %d", key, amount, queueCount, queue[queueCount]);
                    MIDI.sendSysEx(debugSize, (uint8_t *)debug);
                }
                
                ++queueCount;
                if ( queueCount == keyMaxAmount ) queueCount = 0; // testen
            }

            ++amount;
        }

        //debug
        // if ( debugState )
        // {
        //     snprintf(debug, debugSize, "ak  k %d a %d  On", key, amount);
        //     MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        // }

        return adressIsFree;
    }

    bool noteOff(int8_t key)
    {
        bool adressIsFilled = keyPositions[key] > -1;
        if ( adressIsFilled ) // key was added
        {
            keyAtPositions[keyPositions[key]] = -1;

            queue[queueEnd] = keyPositions[key];

            if ( debugState )
            {
                snprintf(debug, debugSize, "ActKey Off key: %d, am: %d, qE: %d, q: %d", key, amount, queueEnd, queue[queueEnd]);
                MIDI.sendSysEx(debugSize, (uint8_t *)debug);
            }

            ++queueEnd;
            if ( queueEnd == keyMaxAmount ) queueEnd = 0; // testen

            keyPositions[key] = -1;
            --amount;
        }

        //debug
        if ( debugState )
        {
            // snprintf(debug, debugSize, "ak  k %d a %d  Off", key, amount);
            // MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        }

        return adressIsFilled;
    }

    // bool clearLastPosition()
    // {
    //     if ( keyAtPositions[amount] > -1 )
    //     {
    //         keyPositions[keyAtPositions[amount]] = -1;

    //         // keyAtPositions[amount] = -1;

    //         return true;
    //     }
    //     else return false;
    // }

    // int8_t getQueuePosition()
    // {
    //     return queue[queueCount];
    // }

    int8_t getPosition(int8_t key)
    {
        return keyPositions[key];
    }

    int8_t getKey(uint8_t position)
    {
        return keyAtPositions[position];
    }

    uint8_t getAmount()
    {
        return amount;
    }
};

template <uint8_t keyMaxAmount = 10> class ActiveKeysFast {
    private:
    int8_t keyPositions[128];
    int8_t keyAtPositions[keyMaxAmount];
    int8_t queue[keyMaxAmount];

    uint8_t queueOnCount = 0;
    uint8_t queueOffCount = 0;
    
    uint8_t amount = 0;

    public:
    ActiveKeysFast()
    {
        reset();
    }

    void reset()
    {
        queueOnCount = 0;
        queueOffCount = 0;
        amount = 0;

        for ( uint8_t i = 0; i < 128; ++i )
        {
            keyPositions[i] = -1;
        }

        for ( uint8_t i = 0; i < keyMaxAmount; ++i )
        {
            keyAtPositions[i] = -1;
            queue[i] = i;
        }
    }

    bool noteOn(int8_t key)
    {
        bool adressIsFree = keyPositions[key] == -1 && amount < keyMaxAmount;
        if ( adressIsFree ) // key not yet added and amount limit not reached
        {
            keyAtPositions[queue[queueOnCount]] = key;
            keyPositions[key] = queue[queueOnCount];

            if ( debugState )
            {
                snprintf(debug, debugSize,
                    "ActKeyOn key: %d, am: %d, qOn: %d, qOf: %d, qC: %d",
                    key, amount, queueOnCount, queueOffCount, queue[queueOnCount]);
                MIDI.sendSysEx(debugSize, (uint8_t *)debug);
            }
            
            bool queueEqual = queueOnCount == queueOffCount;

            ++queueOnCount;
            if ( queueOnCount == keyMaxAmount ) queueOnCount = 0; // testen
            if ( queueEqual ) queueOffCount = queueOnCount;

            ++amount;
        }

        return adressIsFree;
    }

    bool noteOff(int8_t key)
    {
        int8_t keyPos = keyPositions[key];
        bool adressIsFilled = keyPos > -1;
        if ( adressIsFilled ) // key was added
        {
            queue[keyPos] = queue[queueOffCount];
            queue[queueOffCount] = keyPos;

            if ( debugState )
            {
                snprintf(debug, debugSize,
                    "ActKeyOff key: %d, am: %d, q1: %d, q2: %d",
                    key, amount, queue[keyPos], queue[queueOffCount]);
                MIDI.sendSysEx(debugSize, (uint8_t *)debug);
            }

            ++queueOffCount;
            if ( queueOffCount == keyMaxAmount ) queueOffCount = 0;

            keyAtPositions[keyPos] = -1;
            keyPositions[key] = -1;
            --amount;
        }

        return adressIsFilled;
    }

    int8_t getPosition(int8_t key)
    {
        return keyPositions[key];
    }

    int8_t getKey(uint8_t position)
    {
        return keyAtPositions[position];
    }

    uint8_t getAmount()
    {
        return amount;
    }
};

template <uint8_t keyMaxAmount = 10> class ActiveKeysSmall {
    private:
    int8_t keysAtPosition[keyMaxAmount];
    uint8_t amount = 0;

    public:
    ActiveKeysSmall()
    {
        reset();
    }

    void reset()
    {
        amount = 0;
        
        for ( uint8_t i = 0; i < keyMaxAmount; ++i )
        {
            keysAtPosition[i] = -1;
        }
    }

    int8_t getKey(int8_t pos)
    {
        return keysAtPosition[pos];
    }

    bool noteOn(int8_t key)
    {
        for ( int8_t pos = 0; pos < keyMaxAmount; ++pos )
        {
            if ( getKey(pos) == -1 ) // place empty
            {
                // give current counter val to the key.
                keysAtPosition[pos] = key;

                // increment the amount of keys pushed down.
                ++amount;
                
                return true;
            }
        }

        return false;
    }
    
    bool noteOff(int8_t key)
    {
        for ( int8_t pos = 0; pos < keyMaxAmount; ++pos )
        {
            if ( key == getKey(pos) )
            {
                keysAtPosition[pos] = -1;
                --amount;
                
                return true;
            }
        }

        return false;
    }
    
    int8_t getPosition(int8_t key)
    {
        for ( int8_t pos = 0; pos < keyMaxAmount; ++pos )
        {
            if ( key == getKey(pos) )
            {
                return pos;
            }
        }
        return -1;
    }

    uint8_t getAmount()
    {
        return amount;
    }
};

template <uint8_t keyMaxAmount> class LastNoteFast {
    private:
    int8_t historicNotes[keyMaxAmount];
    int8_t futureNotes[keyMaxAmount];

    int8_t noteHist = -1;
    int8_t noteOut = 0;
    int8_t velOut = 0;
    int8_t stateOut = 0;

    public:
    LastNoteFast()
    {
        for ( uint8_t i = 0; i < keyMaxAmount; ++i )
        {
            historicNotes[i] = -2;
            futureNotes[i] = -2;
        }
    }

    bool getNoteState(int8_t note)
    {
        return historicNotes[note] > -2;
    }

    int8_t getState()
    {
        return stateOut;
    }

    int8_t getPitch()
    {
        return noteOut;
    }

    int8_t getVelocity()
    {
        return velOut;
    }

    void noteOn(int8_t noteIn, int8_t velIn)
    {
        bool adressIsFree = historicNotes[noteIn] == -2;
        if ( adressIsFree )
        {
            historicNotes[noteIn] = noteHist;
            if ( noteHist > -1 ) futureNotes[noteHist] = noteIn;

            noteHist = noteIn;
            noteOut = noteIn;
            velOut = velIn;
            stateOut = 1;
        }

        if ( debugState )
        {
            snprintf(debug, debugSize, "ln  i %d o %d s %d On", noteIn, noteOut, stateOut);
            MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        }
    }

    bool noteOff(int8_t noteIn, int8_t velIn)
    {
        bool adressIsFilled = historicNotes[noteIn] > -2;
        if ( adressIsFilled )
        {
            if ( noteIn == noteHist )// Note OFF which was ON as the previous note.
            {
                noteHist = historicNotes[noteIn];
                if ( noteHist > -1 ) // There is a next note.
                {
                    noteOut = noteHist;
                    
                    if ( velIn > 0 )
                    {
                        velOut = velIn;
                    }
                    else if ( velOut > 63 ) 
                    {
                        velOut = 63;
                    }
                }
                else
                {
                    velOut = velIn; // No next note.
                    stateOut = 0;
                }
            }
            else // Note OFF which was ON but not the previous note.
            {
                int8_t futureNote = futureNotes[noteIn];
                int8_t historicNote = historicNotes[noteIn];
                if ( futureNote > -1 ) historicNotes[futureNote] = historicNote;
                if ( historicNote > -1 ) futureNotes[historicNote] = futureNote;
            }
            
            historicNotes[noteIn] = -2;
        }

        //debug
        if ( debugState )
        {
            snprintf(debug, debugSize, "ln  i %d o %d s %d Off", noteIn, noteOut, stateOut);
            MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        }

        return adressIsFilled;
    }
};

template <uint8_t amount = 10> class LastNoteSmall {
    private:
    int8_t notes[amount];
    int8_t endPos = amount - 1;
    int8_t position = -1;

    int8_t noteOut = 0;
    int8_t velOut = 0;
    int8_t stateOut = 0;

    public:
    LastNoteSmall()
    {
        for ( uint8_t pos = 0; pos < amount; ++pos )
        {
            notes[pos] = -1;
        }
    }

    int8_t getState()
    {
        return stateOut;
    }

    int8_t getPitch()
    {
        return noteOut;
    }

    int8_t getVelocity()
    {
        return velOut;
    }

    void noteOn(int8_t noteIn, int8_t velIn) // Wees er van bewust dat je op deze manier noten dubbel kan toevoegen. (zo is er geen loop nodig).
    {
        position = position == endPos ? 0 : position + 1;

        notes[position] = noteIn;

        noteOut = noteIn;
        velOut = velIn;
        stateOut = 1;
    }

    bool noteOff(int8_t noteIn, int8_t velIn)
    {
        bool found = false;
        int8_t offPos = position;

        for ( int8_t i = 0; i < amount; ++i )
        {
            if ( notes[offPos] == noteIn ) found = true; // The note is found.
            
            if ( found )
            {
                int8_t nextPos = offPos == 0 ? endPos : offPos - 1;
                
                notes[offPos] = notes[nextPos];

                if ( offPos == position ) // Note OFF is currently playing.
                {
                    if ( notes[offPos] > -1 ) // There is a next note.
                    {
                        noteOut = notes[offPos];
                        
                        if ( velIn > 0 )
                        {
                            velOut = velIn;
                        }
                        else if ( velOut > 63 ) 
                        {
                            velOut = 63;
                        }
                    }
                    else
                    {
                        velOut = velIn; // No next note.
                        stateOut = 0;
                    }
                }
                else {}; // Note OFF is not currently playing.
            }

            if ( notes[offPos] == -1 ) break; // No leftover notes from this point.

            offPos = offPos == 0 ? endPos : offPos - 1; // increment.
        }

        return found;
    }
};

/* Obsoliet. Het is beter om de max waarde te nemen. Gebruik std::max_element uit <algorithms> te gebruiken.
template <uint8_t keyMaxAmount = 10> class PressureAverage {
    private:
    #include <UtilityFunc.h>

    int8_t pressures[keyMaxAmount];

    int8_t average = 0;
    int8_t diff = 0;
    int8_t output = 0;

    Change <uint8_t> amountChange = Change <uint8_t> (0);

    public:
    PressureAverage()
    {
        for ( int8_t i = 0; i < keyMaxAmount; ++i )
        {
            pressures[i] = 0;
        }
    }

    int8_t operator()(int8_t pressure, int8_t position, uint8_t amount) {
        pressures[position] = pressure;

        int16_t sum = 0;

        for ( int8_t i = 0; i < keyMaxAmount; ++i )
        {
            sum = sum + pressures[i];
        }

        bool amountChanged = amountChange(amount);
        average = amount > 0 ? sum / amount : 0;

        if ( amount == 0 ) diff = 0;
        else if ( amountChanged ) diff = average - output;

        output = average - diff;
        return output;
    };
};
*/
}
#endif