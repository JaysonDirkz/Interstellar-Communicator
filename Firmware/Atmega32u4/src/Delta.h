#ifndef Delta_h
#define Delta_h

template <class in_any_t, class out_signed_t>
class Delta {
    private:
    in_any_t history;
    
    public:
    Delta(in_any_t init = 0)
    {
        history = init;
    }
    
    out_signed_t operator()(in_any_t in) {
        out_signed_t delta = in - history;
        history = in;
        return delta;
    };
};

#endif