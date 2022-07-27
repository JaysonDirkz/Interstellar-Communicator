#ifndef ActiveSensing_h
#define ActiveSensing_h

static uint32_t t_last;

inline void activeSensing_update()
{
    t_last = millis();
}

inline bool activeSensing_getTimeout(uint32_t threshold = 300)
{
    uint32_t t_now = millis();
    uint32_t t_delta = t_now - t_last;
    return t_delta > threshold;
}

#endif
