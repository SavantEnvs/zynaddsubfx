// Globals the zynaddsubfx program normally defines in src/main.cpp (as upstream's
// src/Tests/MessageTest.cpp does); the harnesses run without MiddleWare/NSM.
#include "Misc/MiddleWare.h"
class NSM_Client *nsm = 0;
zyn::MiddleWare *middleware = 0;
char *instance_name = (char *)"";
