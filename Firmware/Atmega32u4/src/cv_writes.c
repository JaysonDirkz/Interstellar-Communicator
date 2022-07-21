#ifndef cv_writes_h
#define cv_writes_h

// All have to be inited with -1. Can contain addresses 0 t/m 3.
i8 atpAddresses[17];
i8 atcAddresses[17];
i8 ccAddresses[17];
i8 pbAddresses[17];

Cv_Base cv_bases[4];
WriteFunction_t *cvWrites[4] = {0, 0, 0, 0};

void parse_cv(i8 cvA)
{
    if ( cvA > -1 ) {
        cvWrites[cvAddress](&cv_bases[cvAddress]);
    }
}

// Learn:
// cvWrites[5] = a cv write function;

// Handle:
// parse_cv(cvAddresses[MIDI.getChannel()]);

struct Cv_Base {
    i8 row;

    union {
        // Others if neccesary. (atp, atc, pb).

        struct { // cc
            i8 msb;
            i8 lsb;
            i8 nrpn_msb;
            i8 nrpn_lsb;

            void (*write[128])(Cv_Base *);
        };
    };
};

void write_atp(Cv_Base *b)
{
    if ( MIDI.getData1() == keysGlobal.atp_note[b->row] )
    {
        cv_pins[b->row] = MIDI.getData2();
    }
}

void write_atc(Cv_Base *b)
{
    cv_pins[b->row] = MIDI.getData1();
}

void write_pb(Cv_Base *b)
{
    cv_pins[b->row] = (MIDI.getData2() + 8192) >> 6;
}

// Controlchange base.
void write_cc_base(Cv_Base *b)
{
    b->write[MIDI.getData1()](b);
}
    // Normal.
    void normal(Cv_Base *b)
    {
        cv_pins[b->row] = MIDI.getData2();
    }

    // Precision.
    void precision_msb(Cv_Base *b)
    {
        b->write[MIDI.getData1() + 32] = precision_lsb;
        b->msb = MIDI.getData2();
    }

    void precision_lsb(Cv_Base *b)
    {
        b->write[MIDI.getData1()] = 0;
        cv_pins[b->row] = (b->msb << 7) | MIDI.getData2();
    }

    // Nrpn and rpn normal.
    void nrpn_99_or_rpn_101_msb_number_normal(Cv_Base *b)
    {
        b->write[MIDI.getData1() - 1] = nrpn_98_lsb_number_normal;
    }

    void nrpn_98_or_rpn_100_number_normal(Cv_Base *b)
    {
        b->write[MIDI.getData1()] = 0;
        b->write[6] = nrpn_6_value_normal;
        b->write[96] = nrpn_increment;
        b->write[97] = nrpn_decrement;
    }

    void nrpn_6_value_normal(Cv_Base *b)
    {
        b->write[6] = 0;
        b->write[96] = 0;
        b->write[97] = 0;
        normal(b);
    }

    // Nrpn precision.
    void nrpn_99_or_rpn_101_lsb_number_precision(Cv_Base *b)
    {
        b->write[MIDI.getData1() - 1] = nrpn_98_lsb_number_precision;
    }

    void nrpn_98_or_rpn_100_lsb_number_precision(Cv_Base *b)
    {
        b->write[MIDI.getData1()] = 0;
        b->write[6] = nrpn_6_value_precision_msb;
        b->write[96] = nrpn_increment;
        b->write[97] = nrpn_decrement;
    }

    void nrpn_6_value_precision_msb(Cv_Base *b)
    {
        b->write[6] = 0;
        b->write[96] = 0;
        b->write[97] = 0;
        precision_msb(b);
    }

    void nrpn_increment(Cv_Base *b)
    {
        b->write[96] = 0;
        b->write[97] = 0;

        cv_pins[b->row] += 1;
        if ( cv_pins[b->row] > 255 ) cv_pins[b->row] = 255;
        else if ( cv_pins[b->row] < 0 ) cv_pins[b->row] = 0;
    }

    void nrpn_decrement(Cv_Base *b)
    {
        b->write[96] = 0;
        b->write[97] = 0;

        cv_pins[b->row] -= 1;
        if ( cv_pins[b->row] > 255 ) cv_pins[b->row] = 255;
        else if ( cv_pins[b->row] < 0 ) cv_pins[b->row] = 0;
    }
#endif