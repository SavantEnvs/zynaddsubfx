// Shared setup for the zynaddsubfx fuzz harnesses (mayhem/ layer, not upstream code).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <rtosc/thread-link.h>
#include "Misc/Master.h"
#include "Misc/Config.h"
#include "globals.h"

namespace fuzzzyn {
constexpr size_t kLinkMsgLen = 4096 * 2;
// Small buffers keep each exec fast; values are within the ranges the UI allows.
inline zyn::SYNTH_T make_synth()
{
    zyn::SYNTH_T s;
    s.samplerate = 44100;
    s.buffersize = 256;
    s.oscilsize  = 512;
    s.alias();
    return s;
}

// A fresh Master per input (no state survives between inputs). The ThreadLinks are what
// MiddleWare normally installs; Master writes replies/notifications into bToU.
struct Rig {
    zyn::SYNTH_T synth = make_synth();
    zyn::Config  config;
    zyn::Master *master;
    std::vector<float> outl, outr;
    Rig()
    {
        master = new zyn::Master(synth, &config);
        master->bToU = new rtosc::ThreadLink(kLinkMsgLen, 1024);
        master->uToB = new rtosc::ThreadLink(kLinkMsgLen, 1024);
        outl.assign(synth.buffersize, 0.0f);
        outr.assign(synth.buffersize, 0.0f);
    }
    ~Rig()
    {
        auto *b = master->bToU, *u = master->uToB;
        delete master;
        delete b;
        delete u;
    }
    void drain() { while(master->bToU->hasNext()) master->bToU->read(); }
    void render(int buffers)
    {
        for(int i = 0; i < buffers; ++i) {
            master->AudioOut(outl.data(), outr.data());
            drain();
        }
    }
    void play()
    {
        master->noteOn(0, 60, 100);
        master->noteOn(0, 64, 90);
        render(6);
        // No noteOff here: on a mono-mode part every note-off reaches Part::monomemPop, which
        // reads monomemnotes[256] (src/Misc/Part.cpp:1365, off-by-one), and most saved .xmz
        // files would crash before any of their content is fuzzed. That bug is still reachable
        // through fuzz_middleware_osc ("/noteOff").
    }
};
}
