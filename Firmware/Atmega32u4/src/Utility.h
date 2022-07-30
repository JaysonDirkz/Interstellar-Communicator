#ifndef Utility_h
#define Utility_h

template <typename type_t>
bool sampleChange(type_t *samples) 
{
    bool notEqual = samples[0] != samples[-1];
    samples[-1] = samples[0];
    return notEqual;
}

template <typename type_t>
long sampleDelta(type_t *samples)
{
    long delta = samples[0] - samples[-1];
    samples[-1] = samples[0];
    return delta;
}

template <class signed_t>
class Control {
    private:
    signed_t v_out;
    signed_t v_in;
    signed_t v_deltaHist = 0;
    bool v_changeHist = false;

    unsigned long t_0 = 0;
    unsigned long t_debounce = 0;

    bool trigger = false;

    void process()
    {
        if ( trigger )
        {
            unsigned long t_now = millis();
            unsigned long t_d = t_now - t_0;
            if ( t_d >= t_debounce )
            {
                trigger = false;
                v_out = v_in;
            }
        }
    }

    public:
    Control(signed_t init)
    {
        v_in = init;
        v_out = init;
    }
    
    // operator signed_t() const {
    //     return v_out;
    // }
    
    // void operator=(signed_t in) {
    //     v_in = in;
    // };

    void debounce(unsigned long t_in)
    {
        t_debounce = t_in;
    }

    void set(signed_t in = 0)
    {
        t_0 = millis();
        v_in = in;
        trigger = true;
    }

    signed_t get()
    {
        process();
        return v_out;
    }

    signed_t delta()
    {
        signed_t out = get() - v_deltaHist;
        v_deltaHist = v_out;
        return out;
    }

    bool change()
    {
        bool out = get() != v_changeHist;
        v_changeHist = v_out;
        return out;
    }
};

template <class signed_t>
class Delta {
    private:
    signed_t history;
    
    public:
    Delta(signed_t init)
    {
        history = init;
    }
    
    signed_t operator()(signed_t in) {
        signed_t delta = in - history;
        history = in;
        return delta;
    };
};

template <class type_t>
class Change {
    private:
    type_t history;
    
    public:
    Change(type_t init)
    {
        history = init;
    }
    
    bool operator()(type_t in) {
        bool out = in != history;
        history = in;
        return out;
    };
};

// Arrays
template <class array_t, class length_t>
array_t arraySum (array_t* array, length_t length)
{
    array_t sum = array[0];

    for (length_t i = 1; i < length; ++i)
    {
        sum += array[i];
    }

    return sum;
}

template <class array_t, class length_t>
array_t arrayMax (array_t* array, length_t length)
{
    array_t max = array[0];

    for (length_t i = 1; i < length; ++i)
    {
        if ( array[i] > max ) max = array[i];
    }

    return max;
}

template <class type, size_t size> class Array {
    private:
    type data[size];

    public:
    Array(type init = 0)
    {
        for ( size_t i = 0; i < size; ++i )
        {
            data[i] = init;
        }
    }

    type get(size_t index)
    {
        return data[index];
    }

    void set(size_t index, type in)
    {
        data[index] = in;
    }

    type* address()
    {
        return data;
    }

    size_t length()
    {
        return size;
    }
};

template <int8_t size_1, int8_t size_2, int8_t resetVal = -1> class TwoDimensionalLookup {
    private:
    int8_t data[size_1][size_2];

    public:
    TwoDimensionalLookup()
    {
        fill();
    }

    void fill(int8_t in = resetVal)
    {
        for ( int8_t i = 0; i < size_1; ++i )
        {
            for ( int8_t j = 0; j < size_2; ++j )
            {
                data[i][j] = in;
            }
        }
    }

    void replace(int8_t in, int8_t replacement)
    {
        for ( int8_t i = 0; i < size_1; ++i )
        {
            for ( int8_t j = 0; j < size_2; ++j )
            {
                if ( data[i][j] == in ) data[i][j] = replacement;
            }
        }
    }

    void remove(int8_t in)
    {
        replace(in, resetVal);
    }

    void set(int8_t dim_1, int8_t dim_2, int8_t in)
    {
        data[dim_1][dim_2] = in;
    }

    void setUnique(int8_t dim_1, int8_t dim_2, int8_t in)
    {
        remove(in);
        set(dim_1, dim_2, in);
    }

    int8_t get(int8_t dim_1, int8_t dim_2)
    {
        return data[dim_1][dim_2];
    }

    bool getState(int8_t dim_1, int8_t dim_2)
    {
        return get(dim_1, dim_2) != resetVal;
    }
};

#endif