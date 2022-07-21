//new callback n_on_callbacks[?];
//new callback n_off_callback[?];
//new callback atp_callbacks[?];
//new callback atc_callbacks[?];
//new callback pb_callbacks[?];
//new callback cc_callbacks[?];

struct Pre_percussion {
    type callback = fn();
    let mut percussion_callbacks: [callback; 128];

    fn handler() {
        percussion_callbacks[MIDI.getData1()];
    }
};

struct Percussion <chokegroup_other_pins_amount: i8> {
    let mut row: i8;
    let mut chokegroup_other_cv_pins_ptrs: [i8; chokegroup_other_pins_amount];

    fn handle_trigger() {
        gate_pins[row] = true;
    }

    fn handle_trigger_and_velocity() {
        handle_trigger();
        cv_pins[row] = MIDI.getData2;
    }

    fn handle_trigger_and_choke() {
        handle_trigger();

        // Choke
        for i in 0..chokegroup_other_pins_amount {
            chokegroup_other_cv_pins_ptrs[i] = 0;
        }
    }

    fn handle_trigger_velocity_and_choke() {
        handle_trigger_and_choke()
        cv_pins[row] = MIDI.getData2;
    }
};

struct Keys <voice_width: i8> {
    let voice_max: i8 = voice_width - 1;
    let mut voice_current: i8 = 0;
    last_note: [LastNote <10>; voice_width];

    let mut rows_g_p: [i8; voice_width];
    let mut row_v: [i8; voice_width];
    let mut row_a: [i8; oice_width];

    type mut write_callback = fn(i8);

    fn write_g_p(voice: i8) {
        gate_pins[row_g_p[voice]] = last_note[voice].get_state();
        cv_pins[row_g_p[voice]] = last_note[voice].get_pitch();
    }

    fn write_g_p_v(voice: i8) {
        write_g_p(voice);
        cv_pins[row_v[voice]] = last_note[voice].get_velocity();
    }

    fn write_g_p_a(voice: i8) {
        write_g_p(voice);
        atp_note[row_a[voice]] = last_note[voice].get_pitch(); // shared var between keys and atp;
    }

    fn write_g_p_v_a(voice: i8) {
        write_g_p_v(voice);
        atp_note[row_a[voice]] = last_note[voice].get_pitch(); // shared var between keys and atp;
    }

    fn on() {
        voice_current = if voice_current == 0 {voice_max} else {voice_current} - 1;
        last_note[voice_current].add(MIDI.getData1());
        write_callback(voice_current);
    }

    fn off() {
        voice_current = 0;
        for voice in (0..voice_max).rev() {
            if last_note[voice].remove(MIDI.getData1()) {
                write_callback(voice)
            }
            voice_current = voice;
        }
    }
};