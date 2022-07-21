#ifndef SysExProgrammer_h
#define SysExProgrammer_h

class SysExProgrammer {
    private:
    char** identifiers = nullptr;
    u8 identifierAmount = 0;

    public:
    SysExProgrammer(char** arrayOfIdentifiers, u8 identifierAmountIn)
    {
        identifiers = arrayOfIdentifiers;
        identifierAmount = identifierAmountIn;
    }

    process(u8* messages, u8 messageLengthBytes)
    {
        for ( u8 i = 0; i < identifierAmount; ++i )
        {
        }
    }
};

#endif