// Fuzzes OSC message dispatch the way OSC clients (the GUI, liblo/UDP peers, plugin hosts)
// reach the synth: MiddleWare::transmitMsg -> MiddleWareImpl::handleMsg (non-realtime ports:
// kit/pad parameter object creation, presets, automation, midi-learn, undo) -> forwarded to the
// realtime Master port tree (Part/effect/filter/oscil parameter ports, rtosc dispatch), rendering
// audio between messages so the DSP runs with the changed parameters.
//
// Input: records [u16 little-endian length][length bytes of one binary OSC message].
// liblo only delivers complete, well-formed messages, so a record is used only if rtosc's own
// validators accept it (rtosc_valid_message_p and rtosc_message_length == record length).
// Skipped on purpose:
//  - messages with blob ('b') arguments: in this tree blobs on these ports carry raw in-process
//    object pointers that MiddleWare itself creates (e.g. "*-data:b", "paste_scl:b");
//  - addresses containing "save", "/bank/" or "/config/": they write files (instrument/master
//    saves, bank renames/swaps, the user's config), which would make one input's result depend
//    on files a previous input wrote.
//  - addresses containing "presets/" (the preset store, PresetExtractor.cpp): copy/paste wrap
//    their work in MiddleWareImpl::doReadOnlyOp, which waits for the realtime thread to answer
//    /freeze_state. This single-threaded harness only runs Master::AudioOut between messages,
//    so the wait always ends in its own assert(tries < 10000) after ~5 s, a harness artifact.
//    copy also writes <name>.<type>.xpz into presetsDirList[0], and delete/scan-for-presets
//    remove or read those files.
// As a second guard, every zyn::Config::cfg.presetsDirList entry is cleared after the Config is
// built (its default [0] is "./"): PresetsStore::copypreset then returns early and a scan finds
// nothing. Nothing else is kept across inputs: each one builds a fresh Config and MiddleWare.
#include "common.h"
#include <rtosc/rtosc.h>
#include "Misc/MiddleWare.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    zyn::Config config;
    for(auto &dir : config.cfg.presetsDirList)
        dir.clear();
    zyn::MiddleWare mw(fuzzzyn::make_synth(), &config);
    zyn::Master *master = mw.spawnMaster();
    const int bs = master->synth.buffersize;
    std::vector<float> outl(bs), outr(bs);
    auto render = [&](int n) {
        for(int i = 0; i < n; ++i) {
            mw.tick();
            master = mw.spawnMaster();
            master->AudioOut(outl.data(), outr.data());
        }
        mw.tick();
    };

    std::vector<char> msg;
    size_t pos = 0;
    int n = 0;
    while(pos + 2 <= size && n < 64) {
        size_t len = data[pos] | (data[pos + 1] << 8);
        pos += 2;
        if(len > size - pos)
            break;
        msg.assign(data + pos, data + pos + len);
        pos += len;
        msg.resize(len + 4, 0);
        if(len == 0 || !rtosc_valid_message_p(msg.data(), len) ||
           rtosc_message_length(msg.data(), len) != len)
            continue;
        if(strchr(rtosc_argument_string(msg.data()), 'b'))
            continue;
        if(strstr(msg.data(), "save") || strstr(msg.data(), "/bank/") ||
           strstr(msg.data(), "/config/") || strstr(msg.data(), "presets/"))
            continue;
        mw.transmitMsg(msg.data());
        render(1);
        if(++n % 4 == 0) {
            master->noteOn(0, 60 + n % 12, 100);
            render(2);
        }
    }
    master->noteOn(0, 60, 100);
    render(4);
    master->noteOff(0, 60);
    render(2);
    return 0;
}
