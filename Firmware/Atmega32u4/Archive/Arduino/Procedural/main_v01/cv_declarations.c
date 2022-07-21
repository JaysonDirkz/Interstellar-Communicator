int8_t atpAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t atcAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t ccAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.
int8_t pbAddresses[17]; // Has to be inited with 0. Can contain addresses 1 t/m 4.

CvBase cvBases[5]; // First one is not used.
WriteFunction_t *cvWrites[5] = {0, 0, 0, 0, 0}; // First one is not used.

void parseCv(int8_t cvAddress)
{
    cvWrites[cvAddress](&cvBases[cvAddress]);
}

// Execution like this:
//parseKey(cvAddresses[MIDI.getChannel()]);