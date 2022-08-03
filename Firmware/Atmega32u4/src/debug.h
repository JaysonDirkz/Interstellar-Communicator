#ifdef DEBUG

#define DEBUG_OUT_FAST(text, dec) \
    char debugText[] = text; \
    char debugNum[20]; \
    itoa(dec, debugNum, 10); \
    MIDI.sendSysEx(sizeof(debugText), (uint8_t *)debugText); \
    MIDI.sendSysEx(sizeof(debugNum), (uint8_t *)debugNum);

#define DEBUG_OUT(text, arg) \
    char debug[] = text; \
    sprintf(debug, debug, arg); \
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_LOAD_EEPROM \
    char debug[] = "Load eeprom values size: %d."; \
    sprintf(debug, debug, a); \
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_SAVE_EEPROM \
    char debug[] = "Save eeprom values size: %d."; \
    sprintf(debug, debug, a); \
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_ACTIVE_TIMEOUT \
    char debug[] = "ActiveSensing timeout.";\
    sprintf(debug, debug);\
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_CV_OUT \
    char debug[] = "cv output: %d, %d.";\
    sprintf(debug, debug, row, value);\
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_GATE_OUT \
    char debug[] = "gate output: %d, %d.";\
    sprintf(debug, debug, row, state);\
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_ACTIVEKEYS_ON //\
    char debug[] = \
        "ActiveKey On key: %d, pos: %d, amnt: %d, writePtr %d, readPtr %d";\
    sprintf(debug, debug, \
        key, position, amount, waitingPositionsReadPtr, waitingPositionsReadPtr \
    ); \
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_ACTIVEKEYS_OFF //\
    char debug[] = \
        "ActiveKey Off key: %d, pos: %d, amnt: %d, writePtr %d, readPtr %d";\
    sprintf(debug, debug, \
        key, position, amount, waitingPositionsReadPtr, waitingPositionsReadPtr \
    ); \
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_LASTNOTE_ON //\
    int8_t *h = historicNotes;\
    int8_t *f = futureNotes;\
    char debug[] = \
        "LastNote On  in %d, out %d, state %d, \
        hist: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, \
        futu: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d.";\
    sprintf(debug, debug, noteIn, noteOut, stateOut,\
        h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9],\
        f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]\
    ); \
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);

#define DEBUG_LASTNOTE_OFF //\
    char debug[] = "LastNote Off  in %d, out %d, state %d.";\
    sprintf(debug, debug, noteIn, noteOut, stateOut);\
    MIDI.sendSysEx(sizeof(debug), (uint8_t *)debug);
    
#else
#define DEBUG_LOAD_EEPROM
#define DEBUG_SAVE_EEPROM
#define DEBUG_ACTIVE_TIMEOUT
#define DEBUG_CV_OUT
#define DEBUG_GATE_OUT
#define DEBUG_ACTIVEKEYS_ON
#define DEBUG_ACTIVEKEYS_OFF
#define DEBUG_LASTNOTE_ON
#define DEBUG_LASTNOTE_OFF

#endif
