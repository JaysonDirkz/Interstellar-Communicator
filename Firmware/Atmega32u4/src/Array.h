#ifndef Array_h
#define Array_h

template <class type, size_t size>
class Array {
    private:
    type data[size];

    public:
    Array(type init)
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

#endif
