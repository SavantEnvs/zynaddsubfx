// Fuzzes rtosc's text OSC format (the savefile / pretty-format scanner used by
// Master::loadOSC and rtosc::load_from_file): count + syntax-check, scan, print, re-scan.
#include <cstdint>
#include <cstring>
#include <vector>
#include <rtosc/rtosc.h>
#include <rtosc/pretty-format.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::vector<char> src(data, data + size);
    src.push_back('\0');

    // A message line: "/address args..."
    int n = rtosc_count_printed_arg_vals_of_msg(src.data());
    if(n > 0 && n < 4096) {
        std::vector<rtosc_arg_val_t> args(n);
        std::vector<char> strbuf(size + 16);
        std::vector<char> addr(size + 16);
        rtosc_scan_message(src.data(), addr.data(), addr.size(), args.data(), n,
                           strbuf.data(), strbuf.size());
        // pretty-format.h: "buffer[-1] must be accessible and be whitespace
        // (since it can be converted to a newline)" -- give the printer one
        // leading space byte before the buffer it writes into.
        std::vector<char> printed(8193);
        printed[0] = ' ';
        rtosc_print_message(addr.data(), args.data(), n, printed.data() + 1,
                            printed.size() - 1, nullptr, 0);
    }

    // Bare argument values: "1 2.0 \"s\" [1 2 3] ..."
    n = rtosc_count_printed_arg_vals(src.data());
    if(n > 0 && n < 4096) {
        std::vector<rtosc_arg_val_t> args(n);
        std::vector<char> strbuf(size + 16);
        rtosc_scan_arg_vals(src.data(), args.data(), n, strbuf.data(), strbuf.size());
        std::vector<char> printed(8193);  // same buffer[-1] precondition as above
        printed[0] = ' ';
        rtosc_print_arg_vals(args.data(), n, printed.data() + 1, printed.size() - 1,
                             nullptr, 0);
    }
    return 0;
}
