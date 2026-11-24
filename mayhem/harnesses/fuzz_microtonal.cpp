// Fuzzes the Scala tuning parsers: Microtonal::loadscl (.scl) and Microtonal::loadkbm (.kbm),
// which read files, plus the text tuning/mapping parsers the UI feeds
// (Microtonal::texttotunings / texttomapping), then evaluates note frequencies.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>
#include "Misc/Microtonal.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *tmpdir = getenv("TMPDIR");
    std::string path = std::string(tmpdir && *tmpdir ? tmpdir : "/tmp") + "/zynscl-XXXXXX";
    std::vector<char> tmpl(path.begin(), path.end());
    tmpl.push_back('\0');
    int fd = mkstemp(tmpl.data());
    if(fd < 0)
        return 0;
    if(size)
        (void)!write(fd, data, size);
    close(fd);

    zyn::SclInfo scl;
    zyn::KbmInfo kbm;
    memset(&scl, 0, sizeof(scl));
    memset(&kbm, 0, sizeof(kbm));
    zyn::Microtonal::loadscl(scl, tmpl.data());
    zyn::Microtonal::loadkbm(kbm, tmpl.data());
    unlink(tmpl.data());

    std::vector<char> text(data, data + size);
    text.push_back('\0');
    int gzip = 0;
    zyn::Microtonal m(gzip);
    m.defaults();
    m.Penabled = true;
    if(m.texttotunings(text.data()) > 0) {
        m.texttomapping(text.data());
        m.Pmappingenabled = true;
    }
    volatile float acc = 0;
    for(int note = 0; note < 128; note += 7)
        acc += m.getnotefreq(note / 12.0f, 0);
    (void)acc;
    return 0;
}
