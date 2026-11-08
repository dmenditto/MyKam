#pragma once

// The X-Plane SDK headers, included with their own warnings silenced so our code can build clean at
// warning level 4. Only the plugin adapter includes this file (the core never sees XPLM).

#pragma warning(push, 0)
#include "XPLMDefs.h"
#include "XPLMCamera.h"
#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMMenus.h"
#include "XPLMPlanes.h"
#include "XPLMProcessing.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#pragma warning(pop)
