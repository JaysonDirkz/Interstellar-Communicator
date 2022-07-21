struct CvBase {
    int8_t row;

    union {
        // Others if neccesary. (atp, atc, pb).

        struct { // cc
            int8_t msb;
            int8_t lsb;
            int8_t nrpn_msb;
            int8_t nrpn_lsb;

            void (*write[128])(CvBase *);
        };
    };
};

void write_atp(CvBase *b)
{
    if ( MIDI.getData1() == keysGlobal.atp_note[b->row] )
    {
        cv_pins[b->row] = MIDI.getData2();
    }
}

void write_atc(CvBase *b)
{
    cv_pins[b->row] = MIDI.getData1();
}

void write_pb(CvBase *b)
{
    cv_pins[b->row] = (MIDI.getData2() + 8192) >> 6;
}

// Controlchange base.
void write_cc_base(CvBase *b)
{
    b->write[MIDI.getData1()](b);
}
    // Normal.
    void normal(CvBase *b)
    {
        cv_pins[b->row] = MIDI.getData2();
    }

    // Precision.
    void precision_msb(CvBase *b)
    {
        b->write[MIDI.getData1() + 32] = precision_lsb;
        b->msb = MIDI.getData2();
    }

    void precision_lsb(CvBase *b)
    {
        b->write[MIDI.getData1()] = 0;
        cv_pins[b->row] = (b->msb << 7) | MIDI.getData2();
    }

    // Nrpn and rpn normal.
    void nrpn_99_or_rpn_101_msb_number_normal(CvBase *b)
    {
        b->write[MIDI.getData1() - 1] = nrpn_98_lsb_number_normal;
    }

    void nrpn_98_or_rpn_100_number_normal(CvBase *b)
    {
        b->write[MIDI.getData1()] = 0;
        b->write[6] = nrpn_6_value_normal;
        b->write[96] = nrpn_increment;
        b->write[97] = nrpn_decrement;
    }

    void nrpn_6_value_normal(CvBase *b)
    {
        b->write[6] = 0;
        b->write[96] = 0;
        b->write[97] = 0;
        normal(b);
    }

    // Nrpn precision.
    void nrpn_99_or_rpn_101_lsb_number_precision(CvBase *b)
    {
        b->write[MIDI.getData1() - 1] = nrpn_98_lsb_number_precision;
    }

    void nrpn_98_or_rpn_100_lsb_number_precision(CvBase *b)
    {
        b->write[MIDI.getData1()] = 0;
        b->write[6] = nrpn_6_value_precision_msb;
        b->write[96] = nrpn_increment;
        b->write[97] = nrpn_decrement;
    }

    void nrpn_6_value_precision_msb(CvBase *b)
    {
        b->write[6] = 0;
        b->write[96] = 0;
        b->write[97] = 0;
        precision_msb(b);
    }

    void nrpn_increment(CvBase *b)
    {
        b->write[96] = 0;
        b->write[97] = 0;

        cv_pins[b->row] += 1;
        if ( cv_pins[b->row] > 255 ) cv_pins[b->row] = 255;
        else if ( cv_pins[b->row] < 0 ) cv_pins[b->row] = 0;
    }

    void nrpn_decrement(CvBase *b)
    {
        b->write[96] = 0;
        b->write[97] = 0;

        cv_pins[b->row] -= 1;
        if ( cv_pins[b->row] > 255 ) cv_pins[b->row] = 255;
        else if ( cv_pins[b->row] < 0 ) cv_pins[b->row] = 0;
    }