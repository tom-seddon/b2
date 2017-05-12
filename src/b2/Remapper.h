#ifndef HEADER_78536F7980464D9F92CE741243E9D123
#define HEADER_78536F7980464D9F92CE741243E9D123

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Remaps one rate (the "step" rate - the number of iterations in the
// loop) to another (the "items" rate - probably a number of items of
// data being produced or consumed, but whatever).

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Remapper {
public:
    // Default initialization gives you a 1:1 remapper.
    Remapper();

    // The remapper will produce num_items items for every num_steps
    // steps.
    Remapper(uint64_t num_steps,uint64_t num_items);

    // Do one step. The return value is the number of items required
    // since the last step.
    uint64_t Step();

    // Determine how many units will be produced over the next
    // num_steps steps.
    uint64_t GetNumUnits(uint64_t steps) const;
protected:
private:
    uint64_t m_num_steps;
    uint64_t m_num_items;
    uint64_t m_error;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
