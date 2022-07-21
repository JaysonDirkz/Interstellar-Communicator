struct KeysBase {
    union {
        struct { //Percussion
            int8_t row;
            int8_t chokegroup_other_pins_amount;
            int8_t *chokegroup_other_cv_pins_ptrs;
        };

        struct { //Keys
            int8_t voice_amount;
            int8_t voice_max = voice_amount - 1;
            int8_t voice_current = 0;
            LastNote <10> *last_note; //voice_amount array size.

            int8_t *rows_g_p; //voice_amount array size.
            int8_t *row_v; //voice_amount array size.
            int8_t *row_a; //voice_amount array size.
        };
    };

    KeysBase()
    {
        //debug
        if ( debugState )
        {
            snprintf(debug, debugSize, "KeysBase created at: %d", this);
            MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        }
    }

    virtual ~KeysBase()
    {
        //debug
        if ( debugState )
        {
            snprintf(debug, debugSize, "KeysBase deleted at: %d", this);
            MIDI.sendSysEx(debugSize, (uint8_t *)debug);
        }
    }

    virtual void on(){}
    virtual void off(){}
};

KeysBase *keysBases[128];

struct Trigger: KeysBase {
    void on() {
        gate_pins[row] = true;
    }
};

struct Trigger_Velocity: Trigger {
    void on() {
        Trigger::on();
        cv_pins[row] = MIDI.getData2;
    }
};

template <typename TrigSubBase>
struct Choke: TrigSubBase {
    Choke(int8_t amount)
    {
        TrigSubBase::chokegroup_other_pins_amount = amount;
        TrigSubBase::chokegroup_other_cv_pins_ptrs = new int8_t[amount];
    }

    void on() {
        TrigSubBase::on();

        for ( int8_t i = 0; i < TrigSubBase::chokegroup_other_pins_amount; ++i ) {
            TrigSubBase::chokegroup_other_cv_pins_ptrs[i] = 0;
        }
    }
};

struct Keys: KeysBase {
    Keys(int8_t amount)
    {
        voice_amount = amount;
        last_note = new LastNote <10> [amount];
    }

    virtual write(int8_t voice) {}

    void on()
    {
        voice_current = voice_current == 0 ? voice_max : voice_current - 1;
        last_note[voice_current].add(MIDI.getData1());
        write(voice_current);
    }

    void off()
    {
        voice_current = 0;
        for ( int8_t voice = voice_max; voice >= 0; --voice ) {
            if ( last_note[voice].remove(MIDI.getData1()) ) {
                return write(voice);;
            }
            voice_current = voice;
        }
    }
};

struct GP: Keys {
    GP(int8_t amount)
    {
        Keys::Keys(amount);
        rows_g_p = new int8_t[amount];
    }

    void write(int8_t voice)
    {
        gate_pins[row_g_p[voice]] = last_note[voice].get_state();
        cv_pins[row_g_p[voice]] = last_note[voice].get_pitch();
    }
};

struct GPV: GP {
    GPV(int8_t amount)
    {
        GP::GP(amount);
        rows_v = new int8_t[amount];
    }

    void write(int8_t voice)
    {
        GP::write(voice);
        cv_pins[row_v[voice]] = last_note[voice].get_velocity();
    }
};

template <typename GP_V>
struct GPA_V: GP_V {
    GPA_V(int8_t amount)
    {
        GP_V::GP_V(amount);
        GP_V::rows_a = new int8_t[amount];
    }

    void write(int8_t voice)
    {
        GP_V::write(voice);
        GP_V::atp_note[row_a[voice]] = GP_V::last_note.get_pitch(); // shared var between keys and atp;
    }
};