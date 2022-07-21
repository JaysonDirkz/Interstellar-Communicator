struct KeysBaseGlobal {
    WriteFunction_t *noteRouters[128]; // init to 0;

    union {
        struct {
            int8_t chokegroup_other_cv_pins;
        };

        struct {
            //Activekeys
            LastNote <10> last_note;
            int8_t rows_g_p;
            int8_t row_v;
            int8_t row_a;
            int8_t atp_note;
        };
    };
};

// enum KeysBaseType { // ????
//     OnTrigger = 1,
//     ...
// };

struct KeysBase {
    // KeysBaseType type; //???

    WriteFunction_t **noteRouter; // init to 0; For attaching the 128 func array after learning.

    union {
        struct { //Percussion
            int8_t row;
            int8_t chokegroup_other_pins_amount;
            int8_t *chokegroup_other_cv_pins; // point to the global equivalent.
        };

        struct { //Keys
            int8_t voice_amount;
            int8_t voice_max; // voice_amount - 1;
            int8_t voice_next; // init to 0;
            LastNote <10> *last_note; //voice_amount array size. point to the global equivalent.

            int8_t *rows_g_p; //voice_amount array size. point to the global equivalent.
            int8_t *row_v; //voice_amount array size. point to the global equivalent.
            int8_t *row_a; //voice_amount array size. point to the global equivalent.

            WriteFunction_t *write; // init to 0; For binding write function of keys.
        };
    };
};

// For both.
void write_keys_base(KeysBase *b)
{
    b->noteRouter[MIDI.getData1()](b);
}

// For Percussion.
void on_trigger(KeysBase *b)
{
    gate_pins[b->row] = true;
}

void on_trigger_velocity(KeysBase *b)
{
    on_trigger(b);
    cv_pins[b->row] = MIDI.getData2;
}

    // Only called by on_trigger_choke and on_trigger_velocity_choke.
    void choke(KeysBase *b)
    {
        for ( int8_t i = 0; i < b->chokegroup_other_pins_amount; ++i ) {
            cv_pins[b->chokegroup_other_cv_pins[i]] = 0;
        }
    }

void on_trigger_choke(KeysBase *b)
{
    on_trigger(b);
    choke(b);
}

void on_trigger_velocity_choke(KeysBase *b)
{
    on_trigger_velocity(b);
    choke(b);
}


// For keys.
void on_keys(KeysBase *b)
{
    if ( b->last_note[b->voice_next].add(MIDI.getData1(), MIDI.getData2()) ) {
        b->noteRouter[MIDI.getData1()] = off_keys;
        b->write(b);
        b->voice_next = b->voice_next == 0 ? b->voice_max : b->voice_next - 1;
    }
}

void off_keys(KeysBase *b)
{
    for ( int8_t voice = b->voice_max; voice >= 0; --voice ) {
        if ( b->last_note[voice].remove(MIDI.getData1(), MIDI.getData2()) ) {
            b->noteRouter[MIDI.getData1()] = on_keys;
            b->voice_next = voice;
            return b->write(b);
        }
    }
}

void write_gate_pitch(KeysBase *b)
{
    gate_pins[b->row_g_p[b->voice]] = b->last_note[b->voice].get_state();
    cv_pins[b->row_g_p[b->voice]] = b->last_note[b->voice].get_pitch();
}

void write_gate_pitch_velocity(KeysBase *b)
{
    write_gate_pitch(b);
    cv_pins[b->row_v[b->voice]] = b->last_note[b->voice].get_velocity();
}

    void write_atp(KeysBase *b)
    {
        keysGlobal.atp_note[b->row_a[b->voice]] = b->last_note.get_pitch(); // shared var between keys and atp;
    }

void write_gate_pitch_atp(KeysBase *b)
{
    write_gate_pitch(b);
    write_atp(b);
}

void write_gate_pitch_velocity_atp(KeysBase *b)
{
    write_gate_pitch_velocity(b);
    write_atp(b);
}