#ifndef MidiUtility_h
#define MidiUtility_h

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
    ActiveKeysFast2()
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

                // if ( debugState )
                // {
                //     snprintf(debug, debugSize, "ActKey On  key: %d, am: %d", key, amount);
                //     MIDI.sendSysEx(debugSize, (uint8_t *)debug);
                // }

                queueCount = 0;
                queueEnd = 0;
            }
            else
            {
                keyAtPositions[queue[queueCount]] = key;

                keyPositions[key] = queue[queueCount];

                // if ( debugState )
                // {
                //     snprintf(debug, debugSize, "ActKey On  key: %d, am: %d, qC: %d, q: %d", key, amount, queueCount, queue[queueCount]);
                //     MIDI.sendSysEx(debugSize, (uint8_t *)debug);
                // }
                
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

            // if ( debugState )
            // {
            //     snprintf(debug, debugSize, "ActKey Off key: %d, am: %d, qE: %d, q: %d", key, amount, queueEnd, queue[queueEnd]);
            //     MIDI.sendSysEx(debugSize, (uint8_t *)debug);
            // }

            ++queueEnd;
            if ( queueEnd == keyMaxAmount ) queueEnd = 0; // testen

            keyPositions[key] = -1;
            --amount;
        }

        //debug
        // if ( debugState )
        // {
            // snprintf(debug, debugSize, "ak  k %d a %d  Off", key, amount);
            // MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        // }

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

// TODO: var outputKey not working in "noteOff" method.
// template <uint8_t keyMaxAmount = 10>
// class ActiveKeysFast {
// private:
//     int8_t outputKey = -1;
//     int8_t keyPositions[128];
//     int8_t keyAtPositions[keyMaxAmount];

//     int8_t waitingPositionsReadPtr = 0;
//     int8_t waitingPositionsWritePtr = 0;
//     int8_t waitingPositions[keyMaxAmount];

//     int8_t lastPosition = keyMaxAmount - 1;
//     uint8_t amount = 0;

// public:
//     ActiveKeysFast()
//     {
//         reset();
//     }

//     void reset()
//     {
//         outputKey = -1;
//         waitingPositionsReadPtr = 0;
//         waitingPositionsWritePtr = 0;
//         amount = 0;

//         for ( uint8_t i = 0; i < 128; ++i )
//         {
//             keyPositions[i] = -1;
//         }

//         for ( uint8_t i = 0; i < keyMaxAmount; ++i )
//         {
//             keyAtPositions[i] = -1;
//         }
//     }

//     bool noteOn(int8_t key)
//     {
//         bool adressIsFree = keyPositions[key] == -1 && amount < keyMaxAmount;
//         if ( adressIsFree ) // key not yet added and amount limit not reached
//         {
//             int8_t position;

//             bool positionsAreWaiting = waitingPositionsReadPtr != waitingPositionsWritePtr;
//             if ( positionsAreWaiting ) {
//                 position = waitingPositions[waitingPositionsReadPtr];
//                 waitingPositionsReadPtr = waitingPositionsReadPtr == lastPosition ? 0 : waitingPositionsReadPtr + 1;
//             } else {
//                 position = amount;
//             }

//             keyAtPositions[position] = key;
//             keyPositions[key] = position;
//             outputKey = key;

//             ++amount;

//             DEBUG_ACTIVEKEYS_ON
//         }

//         return adressIsFree;
//     }

//     bool noteOff(int8_t key)
//     {
//         int8_t position = keyPositions[key];
//         bool adressIsFilled = position > -1;
//         if ( adressIsFilled ) // key was added
//         {
//             if ( key != outputKey ) {
//                 waitingPositions[waitingPositionsWritePtr] = position;
//                 waitingPositionsWritePtr = waitingPositionsWritePtr == lastPosition ? 0 : waitingPositionsWritePtr + 1;
//             }

//             keyAtPositions[position] = -1;
//             keyPositions[key] = -1;

//             --amount;

//             DEBUG_ACTIVEKEYS_OFF
//         }

//         return adressIsFilled;
//     }

//     int8_t getPosition(int8_t key)
//     {
//         return keyPositions[key];
//     }

//     int8_t getKey(uint8_t position)
//     {
//         return keyAtPositions[position];
//     }

//     uint8_t getAmount()
//     {
//         return amount;
//     }
// };

template <uint8_t keyMaxAmount = 10>
class ActiveKeysSmall {
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

    int8_t getKey(int8_t pos)
    {
        return keysAtPosition[pos];
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

template <uint8_t keyMaxAmount>
class LastNoteFast {
private:
    int8_t historicNotes[keyMaxAmount];
    int8_t futureNotes[keyMaxAmount];

    int8_t noteHist = -1;
    int8_t noteOut = 0;
    int8_t velOut = 0;
    int8_t stateOut = 0;

public:
    bool noteOffVelocity = false;

    LastNoteFast()
    {
        reset();
    }

    void reset()
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

        DEBUG_LASTNOTE_ON
    }

    bool noteOff(int8_t noteIn, int8_t velIn)
    {
        bool adressIsFilled = historicNotes[noteIn] > -2;
        if ( adressIsFilled )
        {
            if ( noteIn == noteHist )// Note OFF which was ON as the previous note.
            {
                if ( noteOffVelocity ) velOut = velIn;

                noteHist = historicNotes[noteIn];
                if ( noteHist > -1 ) // There is a next note.
                {
                    noteOut = noteHist;
                }
                else // No next note.
                {
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

        DEBUG_LASTNOTE_OFF

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

#endif
