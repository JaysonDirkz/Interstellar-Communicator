#ifndef keys_writes_h
#define keys_writes_h

i8 keysAddresses[17]; // Has to be inited with -1. Can contain addresses 0 t/m 3.

Notes_Shared notes_shared;
i8 note_adresses[4][128]; // Has to be inited with -1. Can contain addresses 0 t/m 3.
Note_Bases note_bases[4] = {0, 0, 0, 0};
WriteFunction_t *note_writers[4] = {wrRet, wrRet, wrRet, wrRet};

void parse_key()
{
    i8 kA = keysAddresses[MIDI.getChannel()];
    if ( kA > -1 ) {
        i8 nA = note_addresses[kA][MIDI.getData1()];
        if ( nA > -1 ) {
            note_writers[nA](note_bases[nA]);
        }
    }
}

struct Notes_Shared {
    union {
        struct {
            i8 chokegroup_other_cv_pins[4];
        };

        struct {
            //Activekeys
            LastNote <10> last_note[4];
            i8 rows_g_p[4];
            i8 row_v[4];
            i8 row_a[4];
            i8 atp_note[4];
        };
    };
};

struct Note_Bases {
    union {
        struct { //Percussion
            i8 row;
            i8 chokegroup_other_pins_amount;
            i8 *chokegroup_other_cv_pins; // point to the global equivalent.
        };

        struct { //Keys
            i8 address;
            i8 voice_amount;
            i8 voice_max; // voice_amount - 1;
            i8 voice_next; // init to 0;
            LastNote <10> *last_note; //voice_amount array size. point to the global equivalent.

            i8 *rows_g_p; //voice_amount array size. point to the global equivalent.
            i8 *row_v; //voice_amount array size. point to the global equivalent.
            i8 *row_a; //voice_amount array size. point to the global equivalent.

            WriteFunction_t *write; // init to 0; For binding write function of keys.
        };
    };
};

// For both.
// void write_keys_base(Note_Bases *b)
// {
//     b->note_router[MIDI.getData1()](b);

//     i8 noteAddress = b->note_router[MIDI.getData1()];
//     noteFuncs[noteAddress](noteBases[noteAddress]);
// }

// For Percussion.
void on_trigger(Note_Bases *b)
{
    trigCounter[b->row] = PULSE_LENGTH_MS;
    gate_pins[b->row] = true;
    choke(b);
}

void on_trigger_velocity(Note_Bases *b)
{
    on_trigger(b);
    cv_pins[b->row] = MIDI.getData2;
    choke(b);
}

    // Only called by on_trigger and on_trigger_velocity.
    void choke(Note_Bases *b)
    {
        for ( i8 i = 0; i < b->chokegroup_other_pins_amount; ++i ) {
            cv_pins[b->chokegroup_other_cv_pins[i]] = 0;
        }
    }


// For keys.
void on_keys(Note_Bases *b)
{
    if ( b->last_note[b->voice_next].add(MIDI.getData1(), MIDI.getData2()) ) {
        note_writers[b->address][MIDI.getData1()] = off_keys;
        b->write(b);
        b->voice_next = b->voice_next == 0 ? b->voice_max : b->voice_next - 1;
    }
}

void off_keys(Note_Bases *b)
{
    for ( i8 voice = b->voice_max; voice >= 0; --voice ) {
        if ( b->last_note[voice].remove(MIDI.getData1(), MIDI.getData2()) ) {
            note_writers[b->address][MIDI.getData1()] = on_keys;
            b->voice_next = voice;
            return b->write(b);
        }
    }
}

void write_gate_pitch(Note_Bases *b)
{
    gate_pins[b->row_g_p[b->voice]] = b->last_note[b->voice].get_state();
    cv_pins[b->row_g_p[b->voice]] = b->last_note[b->voice].get_pitch();
}

void write_gate_pitch_velocity(Note_Bases *b)
{
    write_gate_pitch(b);
    cv_pins[b->row_v[b->voice]] = b->last_note[b->voice].get_velocity();
}

    void write_atp(Note_Bases *b)
    {
        notes_shared.atp_note[b->row_a[b->voice]] = b->last_note.get_pitch(); // shared var between keys and atp;
    }

void write_gate_pitch_atp(Note_Bases *b)
{
    write_gate_pitch(b);
    write_atp(b);
}

void write_gate_pitch_velocity_atp(Note_Bases *b)
{
    write_gate_pitch_velocity(b);
    write_atp(b);
}
#endif