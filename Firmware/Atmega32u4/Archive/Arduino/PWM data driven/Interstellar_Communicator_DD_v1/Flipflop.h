template <class signed_t, class logic_t>
class Flipflop
{
    private:
        logic_t state;
        signed_t change = 0;

        void pollChange()
        {
            static logic_t stateHistory = state;

            if ( state > stateHistory ) change = 1;
            else if (state < stateHistory ) change = -1;
            
            stateHistory = state;
        }

    public:
        Flipflop(logic_t initial)
        {
            write(initial);
        }

        void write(logic_t in)
        {
            state = in;

            pollChange();
        }

        void trigger()
        {   
            write(! state);
        }

        logic_t read()
        {
            return state;
        }

        signed_t stealChange()
        {
            auto out = change;
            change = 0;
            return out;
        }
};
