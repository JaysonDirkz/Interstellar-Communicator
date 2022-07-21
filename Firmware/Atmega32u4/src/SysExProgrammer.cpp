class SysExProgrammer {
    private:
    char** identifiers = nullptr;
    uitn8_t identifierAmount = 0;

    public:
    SysExProgrammer(char** arrayOfIdentifiers, uint8_t identifierAmountIn)
    {
        identifiers = arrayOfIdentifiers;
        identifierAmount = identifierAmountIn;
    }

    process(uint8_t* messages, uint8_t messageLengthBytes)
    {
        for ( uint8_t i = 0; i < identifierAmount; ++i )
        {
        }
    }
};