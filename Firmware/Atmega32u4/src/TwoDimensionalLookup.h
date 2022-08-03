#ifndef TwoDimensionalLookup_h
#define TwoDimensionalLookup_h

template <int8_t size_1, int8_t size_2, int8_t resetVal = -1>
class TwoDimensionalLookup {
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
