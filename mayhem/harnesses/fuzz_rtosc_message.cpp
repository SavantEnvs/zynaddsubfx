// Fuzzes rtosc's binary OSC message/bundle API, the first code any OSC packet reaches:
// rtosc_valid_message_p / rtosc_message_length validation, argument access
// (rtosc_narguments, rtosc_argument, rtosc_itr_*), bundle walking (rtosc_bundle_p,
// rtosc_bundle_elements/fetch/size) and rtosc_match_path against Zyn-style port patterns.
// The accessors are only called on input the validators accepted, as their contract requires.
#include <cstdint>
#include <cstring>
#include <vector>
#include <rtosc/rtosc.h>

static void walk_message(const char *msg, size_t len)
{
    if(!rtosc_valid_message_p(msg, len) || rtosc_message_length(msg, len) != len)
        return;
    volatile uint64_t acc = 0;
    unsigned n = rtosc_narguments(msg);
    for(unsigned i = 0; i < n; ++i) {
        acc += rtosc_type(msg, i);
        rtosc_arg_t a = rtosc_argument(msg, i);
        acc += a.i;
    }
    rtosc_arg_itr_t itr = rtosc_itr_begin(msg);
    while(!rtosc_itr_end(itr)) {
        rtosc_arg_val_t v = rtosc_itr_next(&itr);
        acc += v.type;
    }
    static const char *patterns[] = {
        "part#16/kit#16/Padenabled::T:F", "sysefx#4/efftype::i", "microtonal/tunings::s",
        "noteOn:iii", "*", "part#16/", "Pvolume::i",
    };
    for(const char *p : patterns)
        acc += rtosc_match_path(p, msg, nullptr) != nullptr;
    (void)acc;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Exact-size heap copy (+4 zero bytes of slack, as OSC packets are 4-byte padded).
    std::vector<char> buf(data, data + size);
    buf.resize(size + 4, 0);
    const char *msg = buf.data();
    if(size >= 16 && rtosc_bundle_p(msg)) {
        size_t elms = rtosc_bundle_elements(msg, size);
        for(unsigned i = 0; i < elms; ++i) {
            const char *e = rtosc_bundle_fetch(msg, i);
            size_t esz = rtosc_bundle_size(msg, i);
            if(e < msg || e + esz > msg + size)
                break;
            walk_message(e, esz);
        }
        return 0;
    }
    if(size)
        walk_message(msg, size);
    return 0;
}
