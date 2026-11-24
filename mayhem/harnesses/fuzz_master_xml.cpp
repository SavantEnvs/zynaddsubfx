// Fuzzes the .xmz master-file loader: Master::putalldata (XMLwrapper/mxml + every
// Part/ADnote/SUBnote/PADnote/Effect/Microtonal parameter loader), then renders audio
// from the loaded state so the synthesis and effect DSP run on the loaded parameters.
#include "common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // putalldata takes a NUL-terminated XML string.
    std::vector<char> buf(data, data + size);
    buf.push_back('\0');

    fuzzzyn::Rig rig;
    rig.master->putalldata(buf.data());
    rig.master->applyparameters();   // what the loaders call before handing the Master to RT
    rig.master->initialize_rt();
    rig.play();
    return 0;
}
