// Fuzzes the .xiz instrument loader: Part::loadXMLinstrument (XMLwrapper file load, then the kit
// loader with ADnote/SUBnote/PADnote parameters, OscilGen, filters, envelopes, LFOs, part effects),
// then Part::applyparameters (PADsynth wavetables) and note synthesis (ComputePartSmps).
// Setup follows upstream's src/Tests/InstrumentStats.cpp; everything is created per input.
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>
#include "Misc/Time.h"
#include "Misc/Sync.h"
#include "Misc/Part.h"
#include "Misc/Allocator.h"
#include "Misc/Microtonal.h"
#include "DSP/FFTwrapper.h"
#include "globals.h"
#include "common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *tmpdir = getenv("TMPDIR");
    std::string path = std::string(tmpdir && *tmpdir ? tmpdir : "/tmp") + "/zynxiz-XXXXXX";
    std::vector<char> tmpl(path.begin(), path.end());
    tmpl.push_back('\0');
    int fd = mkstemp(tmpl.data());
    if(fd < 0)
        return 0;
    if(size)
        (void)!write(fd, data, size);
    close(fd);

    zyn::SYNTH_T synth = fuzzzyn::make_synth();
    zyn::AbsTime time(synth);
    zyn::Sync sync;
    zyn::Alloc alloc;
    int compress = 0, interp = 1;
    zyn::Microtonal microtonal(compress);
    zyn::FFTwrapper fft(synth.oscilsize);
    {
        zyn::Part part(alloc, synth, time, &sync, compress, interp, &microtonal, &fft);
        int r = part.loadXMLinstrument(tmpl.data());
        unlink(tmpl.data());
        if(r == 0) {
            part.applyparameters();
            part.initialize_rt();
            part.Penabled = true;
            part.NoteOn(60, 100, 0);
            part.NoteOn(67, 90, 0);
            for(int i = 0; i < 6; ++i)
                part.ComputePartSmps();
        }
    }
    return 0;
}
