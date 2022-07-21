#ifndef MidiUtility_h
#define MidiUtility_h

namespace DirkXoniC
{
//debug
const u8 debugSize = 50;
char debug[debugSize];

template <u8 keyMaxAmount = 10> class ActiveKeysFast2 {
    private:
    i8 keyPositions[128];
    const u8 keyPositionsSize = sizeof(keyPositions) / sizeof(keyPositions[0]);
    
    i8 keyAtPositions[keyMaxAmount];
    i8 queue[keyMaxAmount];

    u8 queueCount = 0;
    u8 queueEnd = 0;
    
    u8 amount = 0;

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

        for ( u8 i = 0; i < keyPositionsSize; ++i )
        {
            keyPositions[i] = -1;
        }

        for ( u8 i = 0; i < keyMaxAmount; ++i )
        {
            keyAtPositions[i] = -1;
            queue[i] = 0;
        }
    }

    bool noteOn(i8 key)
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
                    MIDI.sendSysEx(debugSize, (u8 *)debug);
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
                    MIDI.sendSysEx(debugSize, (u8 *)debug);
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
        //     MIDI.sendSysEx(debugSize, (u8 *)debug);
        // }

        return adressIsFree;
    }

    bool noteOff(i8 key)
    {
        bool adressIsFilled = keyPositions[key] > -1;
        if ( adressIsFilled ) // key was added
        {
            keyAtPositions[keyPositions[key]] = -1;

            queue[queueEnd] = keyPositions[key];

            if ( debugState )
            {
                snprintf(debug, debugSize, "ActKey Off key: %d, am: %d, qE: %d, q: %d", key, amount, queueEnd, queue[queueEnd]);
                MIDI.sendSysEx(debugSize, (u8 *)debug);
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
            // MIDI.sendSysEx(debugSize, (u8 *)debug);
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

    // i8 getQueuePosition()
    // {
    //     return queue[queueCount];
    // }

    i8 getPosition(i8 key)
    {
        return keyPositions[key];
    }

    i8 getKey(u8 position)
    {
        return keyAtPositions[position];
    }

    u8 getAmount()
    {
        return amount;
    }
};

template <u8 keyMaxAmount = 10> class ActiveKeysFast {
    private:
    i8 keyPositions[128];
    i8 keyAtPositions[keyMaxAmount];
    i8 queue[keyMaxAmount];

    u8 queueOnCount = 0;
    u8 queueOffCount = 0;
    
    u8 amount = 0;

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

        for ( u8 i = 0; i < 128; ++i )
        {
            keyPositions[i] = -1;
        }

        for ( u8 i = 0; i < keyMaxAmount; ++i )
        {
            keyAtPositions[i] = -1;
            queue[i] = i;
        }
    }

    bool noteOn(i8 key)
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
                MIDI.sendSysEx(debugSize, (u8 *)debug);
            }
            
            bool queueEqual = queueOnCount == queueOffCount;

            ++queueOnCount;
            if ( queueOnCount == keyMaxAmount ) queueOnCount = 0; // testen
            if ( queueEqual ) queueOffCount = queueOnCount;

            ++amount;
        }

        return adressIsFree;
    }

    bool noteOff(i8 key)
    {
        i8 keyPos = keyPositions[key];
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
                MIDI.sendSysEx(debugSize, (u8 *)debug);
            }

            ++queueOffCount;
            if ( queueOffCount == keyMaxAmount ) queueOffCount = 0;

            keyAtPositions[keyPos] = -1;
            keyPositions[key] = -1;
            --amount;
        }

        return adressIsFilled;
    }

    i8 getPosition(i8 key)
    {
        return keyPositions[key];
    }

    i8 getKey(u8 position)
    {
        return keyAtPositions[position];
    }

    u8 getAmount()
    {
        return amount;
    }
};

template <u8 keyMaxAmount = 10> class ActiveKeysSmall {
    private:
    i8 keysAtPosition[keyMaxAmount];
    u8 amount = 0;

    public:
    ActiveKeysSmall()
    {
        reset();
    }

    void reset()
    {
        amount = 0;
        
        for ( u8 i = 0; i < keyMaxAmount; ++i )
        {
            keysAtPosition[i] = -1;
        }
    }

    i8 getKey(i8 pos)
    {
        return keysAtPosition[pos];
    }

    bool noteOn(i8 key)
    {
        for ( i8 pos = 0; pos < keyMaxAmount; ++pos )
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
    
    bool noteOff(i8 key)
    {
        for ( i8 pos = 0; pos < keyMaxAmount; ++pos )
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
    
    i8 getPosition(i8 key)
    {
        for ( i8 pos = 0; pos < keyMaxAmount; ++pos )
        {
            if ( key == getKey(pos) )
            {
                return pos;
            }
        }
        return -1;
    }

    u8 getAmount()
    {
        return amount;
    }
};

template <u8 keyMaxAmount> class LastNoteFast {
    private:
    i8 historicNotes[keyMaxAmount];
    i8 futureNotes[keyMaxAmount];

    i8 noteHist = -1;
    i8 noteOut = 0;
    i8 velOut = 0;
    i8 stateOut = 0;

    public:
    LastNoteFast()
    {
        for ( u8 i = 0; i < keyMaxAmount; ++i )
        {
            historicNotes[i] = -2;
            futureNotes[i] = -2;
        }
    }

    bool getNoteState(i8 note)
    {
        return historicNotes[note] > -2;
    }

    i8 getState()
    {
        return stateOut;
    }

    i8 getPitch()
    {
        return noteOut;
    }

    i8 getVelocity()
    {
        return velOut;
    }

    void noteOn(i8 noteIn, i8 velIn)
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
            MIDI.sendSysEx(debugSize, (u8 *)debug);
        }
    }

    bool noteOff(i8 noteIn, i8 velIn)
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
                i8 futureNote = futureNotes[noteIn];
                i8 historicNote = historicNotes[noteIn];
                if ( futureNote > -1 ) historicNotes[futureNote] = historicNote;
                if ( historicNote > -1 ) futureNotes[historicNote] = futureNote;
            }
            
            historicNotes[noteIn] = -2;
        }

        //debug
        if ( debugState )
        {
            snprintf(debug, debugSize, "ln  i %d o %d s %d Off", noteIn, noteOut, stateOut);
            MIDI.sendSysEx(debugSize, (u8 *)debug);
        }

        return adressIsFilled;
    }
};

template <u8 amount = 10> class LastNoteSmall {
    private:
    i8 notes[amount];
    i8 endPos = amount - 1;
    i8 position = -1;

    i8 noteOut = 0;
    i8 velOut = 0;
    i8 stateOut = 0;

    public:
    LastNoteSmall()
    {
        for ( u8 pos = 0; pos < amount; ++pos )
        {
            notes[pos] = -1;
        }
    }

    i8 getState()
    {
        return stateOut;
    }

    i8 getPitch()
    {
        return noteOut;
    }

    i8 getVelocity()
    {
        return velOut;
    }

    void noteOn(i8 noteIn, i8 velIn) // Wees er van bewust dat je op deze manier noten dubbel kan toevoegen. (zo is er geen loop nodig).
    {
        position = position == endPos ? 0 : position + 1;

        notes[position] = noteIn;

        noteOut = noteIn;
        velOut = velIn;
        stateOut = 1;
    }

    bool noteOff(i8 noteIn, i8 velIn)
    {
        bool found = false;
        i8 offPos = position;

        for ( i8 i = 0; i < amount; ++i )
        {
            if ( notes[offPos] == noteIn ) found = true; // The note is found.
            
            if ( found )
            {
                i8 nextPos = offPos == 0 ? endPos : offPos - 1;
                
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
template <u8 keyMaxAmount = 10> class PressureAverage {
    private:
    #include <UtilityFunc.h>

    i8 pressures[keyMaxAmount];

    i8 average = 0;
    i8 diff = 0;
    i8 output = 0;

    Change <u8> amountChange = Change <u8> (0);

    public:
    PressureAverage()
    {
        for ( i8 i = 0; i < keyMaxAmount; ++i )
        {
            pressures[i] = 0;
        }
    }

    i8 operator()(i8 pressure, i8 position, u8 amount) {
        pressures[position] = pressure;

        int16_t sum = 0;

        for ( i8 i = 0; i < keyMaxAmount; ++i )
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