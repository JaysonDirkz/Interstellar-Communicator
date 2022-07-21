int8_t keysAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.

KeysBaseGlobal keysGlobal[4];
KeysBase keysBases[5] = {0, 0, 0, 0, 0}; // First one is not used.
WriteFunction_t *keysWrite[5] = {0, 0, 0, 0, 0}; // First one is not used

void parseKey(int8_t keysNoteAddress)
{
    keysWrite[keysNoteAddress](keysBases[keysNoteAddress]);
}

// Execution like this:
//parseKey(keysAddresses[MIDI.getChannel()]);