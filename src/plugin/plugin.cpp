// MyKam: X-Plane 12 plugin entry points. A thin adapter: it registers commands, forwards them to
// the Controller and places the camera from the per-frame callback. No decisions live here.

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/command_names.h"
#include "core/controller.h"
#include "core/file_io.h"
#include "core/ui_model.h"
#include "core/version.h"
#include "core/view_commands.h"
#include "ui_windows.h"
#include "xplm_includes.h"
#include "xplm_sim.h"

namespace {

enum Action : std::intptr_t {
    kSaveBase = 100,  // kSaveBase + N saves slot N
    kGoBase = 200,    // kGoBase + N goes to slot N
    kRelease = 300,
    kToggleWindow = 301,
    kNext = 302,
    kPrevious = 303,
    kToggleGroup = 304
};

struct Plugin {
    mykam::XplmSim sim;
    std::unique_ptr<mykam::Controller> controller;
    std::unique_ptr<mykam::UiModel> uiModel;      // exists while the plugin is enabled
    std::unique_ptr<mykam::UiWindows> uiWindows;  // main window, quick bar and menu
    static constexpr int kMaxCommands = 48;  // room for the 23 commands
    XPLMCommandRef commands[kMaxCommands] = {};
    std::intptr_t actions[kMaxCommands] = {};
    int commandCount = 0;
    bool commandsRegistered = false;
    static constexpr int kMaxViewCommands = mykam::kViewActionCount * mykam::kViewSpeedCount;
    XPLMCommandRef viewCommands[kMaxViewCommands] = {};
    int viewCodes[kMaxViewCommands] = {};
    int viewCommandCount = 0;
    XPLMCommandRef selectCommands[mykam::kViewCommandCount] = {};  // sim/view/* commands we only watch
    // The custom keys registered with X-Plane. Each entry is separately allocated so the description
    // text X-Plane was given keeps its address.
    struct RegisteredKey {
        mykam::WantedKey wanted;
        XPLMHotKeyID id = nullptr;
    };
    std::vector<std::unique_ptr<RegisteredKey>> hotKeys;
};

// The core and X-Plane use the same modifier bits; the stored files rely on it.
static_assert(mykam::kKeyShift == xplm_ShiftFlag, "shift bit");
static_assert(mykam::kKeyAlt == xplm_OptionAltFlag, "alt bit");
static_assert(mykam::kKeyCtrl == xplm_ControlFlag, "control bit");

Plugin* g_plugin = nullptr;

void logRaw(const std::string& message) {
    const std::string line = "[MyKam] " + message + "\n";
    XPLMDebugString(line.c_str());
}

// Drop the camera callback result into X-Plane's structure. Never throws into X-Plane.
int cameraCallback(XPLMCameraPosition_t* out, int losingControl, void*) {
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller) {
        return 0;
    }
    try {
        if (losingControl != 0) {
            p->sim.markCameraLost();
            p->controller->onCameraLost();
            return 0;
        }
        if (out == nullptr) {
            return 0;
        }
        mykam::Pose world;
        if (!p->controller->tick(p->sim.frameDeltaSeconds(), world)) {
            p->sim.markCameraLost();
            return 0;  // surrender the camera
        }
        out->x = static_cast<float>(world.x);
        out->y = static_cast<float>(world.y);
        out->z = static_cast<float>(world.z);
        out->pitch = static_cast<float>(world.pitch);
        out->heading = static_cast<float>(world.heading);
        out->roll = static_cast<float>(world.roll);
        out->zoom = static_cast<float>(world.zoom);
        return 1;
    } catch (...) {
        p->sim.markCameraLost();
        return 0;
    }
}

int commandHandler(XPLMCommandRef, XPLMCommandPhase phase, void* refcon) {
    if (phase != xplm_CommandBegin) {
        return 1;
    }
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller) {
        return 1;
    }
    try {
        const std::intptr_t action = reinterpret_cast<std::intptr_t>(refcon);
        if (action >= kSaveBase + 1 && action <= kSaveBase + 9) {
            p->controller->saveSlot(static_cast<int>(action - kSaveBase));
        } else if (action >= kGoBase + 1 && action <= kGoBase + 9) {
            p->controller->goToSlot(static_cast<int>(action - kGoBase));
        } else if (action == kRelease) {
            p->controller->release();
        } else if (action == kNext) {
            p->controller->next();
        } else if (action == kPrevious) {
            p->controller->previous();
        } else if (action == kToggleGroup) {
            if (p->uiModel) {
                p->uiModel->toggleBarGroup();
            }
        } else if (action == kToggleWindow) {
            if (p->uiWindows) {
                p->uiWindows->toggleMainWindow();
            }
        }
    } catch (...) {
        logRaw("warning: a command failed with an unexpected error");
    }
    return 1;
}

// Before-handler for X-Plane's own view commands. While mykam holds an external camera it moves
// that camera instead and swallows the command (return 0); otherwise X-Plane handles it (return 1).
int viewCommandHandler(XPLMCommandRef, XPLMCommandPhase phase, void* refcon) {
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller) {
        return 1;
    }
    try {
        const int code = static_cast<int>(reinterpret_cast<std::intptr_t>(refcon));
        const mykam::ViewAction action = static_cast<mykam::ViewAction>(code / mykam::kViewSpeedCount);
        const mykam::ViewSpeed speed = static_cast<mykam::ViewSpeed>(code % mykam::kViewSpeedCount);
        mykam::CommandPhase cp = mykam::CommandPhase::Continue;
        if (phase == xplm_CommandBegin) {
            cp = mykam::CommandPhase::Begin;
        } else if (phase == xplm_CommandEnd) {
            cp = mykam::CommandPhase::End;
        }
        return p->controller->onViewCommand(action, speed, cp) ? 0 : 1;
    } catch (...) {
        return 1;
    }
}

// Before-handler for the X-Plane view-selecting commands (sim/view/chase and so on). It only notes
// which view the user chose and always returns 1, so X-Plane still acts on the command.
int viewSelectHandler(XPLMCommandRef, XPLMCommandPhase phase, void* refcon) {
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller || phase != xplm_CommandBegin) {
        return 1;
    }
    try {
        const std::intptr_t index = reinterpret_cast<std::intptr_t>(refcon);
        if (index >= 0 && static_cast<std::size_t>(index) < mykam::kViewCommandCount) {
            p->controller->onViewSelected(mykam::kViewCommands[index].name);
        }
    } catch (...) {
    }
    return 1;
}

// Flight loop for the cockpit head animation: every frame while animating, then 0 (not called again).
float headFlightLoop(float, float, int, void*) {
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller) {
        return 0.0f;
    }
    try {
        return p->controller->tickCockpit(p->sim.frameDeltaSeconds()) ? -1.0f : 0.0f;
    } catch (...) {
        return 0.0f;
    }
}

// Flight loop for the hand-over to X-Plane's free camera: runs once on the frame after it is
// requested, then 0 (idle again). Never runs inside the camera callback.
float handoffFlightLoop(float, float, int, void*) {
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller) {
        return 0.0f;
    }
    try {
        p->controller->performHandoff();
    } catch (...) {
        logRaw("warning: the hand-over to the free camera failed with an unexpected error");
    }
    return 0.0f;
}

const char* const kViewActionNames[mykam::kViewActionCount] = {
    "left", "right", "up", "down", "forward", "backward", "rot_left", "rot_right", "rot_up", "rot_down",
    "zoom_in", "zoom_out"};
const char* const kViewSpeedSuffix[mykam::kViewSpeedCount] = {"", "_fast", "_slow"};

void registerViewCommands(Plugin& p) {
    for (int a = 0; a < mykam::kViewActionCount; ++a) {
        for (int s = 0; s < mykam::kViewSpeedCount; ++s) {
            const std::string name = std::string("sim/general/") + kViewActionNames[a] + kViewSpeedSuffix[s];
            XPLMCommandRef ref = XPLMFindCommand(name.c_str());
            if (ref == nullptr || p.viewCommandCount >= Plugin::kMaxViewCommands) {
                continue;  // this X-Plane has no such command: nothing to intercept
            }
            const int code = a * mykam::kViewSpeedCount + s;
            p.viewCommands[p.viewCommandCount] = ref;
            p.viewCodes[p.viewCommandCount] = code;
            ++p.viewCommandCount;
            XPLMRegisterCommandHandler(ref, viewCommandHandler, 1, reinterpret_cast<void*>(static_cast<std::intptr_t>(code)));
        }
    }
}

void registerViewSelectCommands(Plugin& p) {
    for (std::size_t i = 0; i < mykam::kViewCommandCount; ++i) {
        XPLMCommandRef ref = XPLMFindCommand(mykam::kViewCommands[i].name);
        if (ref == nullptr) {
            continue;  // this X-Plane has no such command: nothing to watch
        }
        p.selectCommands[i] = ref;
        XPLMRegisterCommandHandler(ref, viewSelectHandler, 1, reinterpret_cast<void*>(static_cast<std::intptr_t>(i)));
    }
}

void unregisterViewSelectCommands(Plugin& p) {
    for (std::size_t i = 0; i < mykam::kViewCommandCount; ++i) {
        if (p.selectCommands[i] != nullptr) {
            XPLMUnregisterCommandHandler(p.selectCommands[i], viewSelectHandler, 1,
                                         reinterpret_cast<void*>(static_cast<std::intptr_t>(i)));
            p.selectCommands[i] = nullptr;
        }
    }
}

void unregisterViewCommands(Plugin& p) {
    for (int i = 0; i < p.viewCommandCount; ++i) {
        XPLMUnregisterCommandHandler(p.viewCommands[i], viewCommandHandler, 1,
                                     reinterpret_cast<void*>(static_cast<std::intptr_t>(p.viewCodes[i])));
    }
    p.viewCommandCount = 0;
}

void registerOneCommand(Plugin& p, const std::string& name, const std::string& description, std::intptr_t action) {
    XPLMCommandRef ref = XPLMCreateCommand(name.c_str(), description.c_str());
    if (ref == nullptr) {
        return;
    }
    if (p.commandCount >= Plugin::kMaxCommands) {
        return;
    }
    const int i = p.commandCount++;
    p.commands[i] = ref;
    p.actions[i] = action;
    XPLMRegisterCommandHandler(ref, commandHandler, 1, reinterpret_cast<void*>(action));
}

// Registers mykam/<baseName>.
void registerCommand(Plugin& p, const std::string& baseName, const std::string& description, std::intptr_t action) {
    registerOneCommand(p, mykam::commandName(baseName), description, action);
}

void unregisterCommands(Plugin& p) {
    for (int i = 0; i < p.commandCount; ++i) {
        XPLMUnregisterCommandHandler(p.commands[i], commandHandler, 1, reinterpret_cast<void*>(p.actions[i]));
    }
    p.commandCount = 0;
}

// A custom key was pressed: go to the view it belongs to.
void hotKeyCallback(void* refcon) {
    Plugin* p = g_plugin;
    if (p == nullptr || !p->controller) {
        return;
    }
    try {
        p->controller->goToView(static_cast<int>(reinterpret_cast<std::intptr_t>(refcon)));
    } catch (...) {
        logRaw("warning: a custom key failed with an unexpected error");
    }
}

void unregisterHotKeys(Plugin& p) {
    for (const auto& key : p.hotKeys) {
        XPLMUnregisterHotKey(key->id);
    }
    p.hotKeys.clear();
}

// Bring the registered keys in line with what the core wants: the core decides the set, this only
// applies the difference. Runs when the set changes (aircraft load, key edit, rename, delete).
void applyHotKeys(Plugin& p) {
    if (!p.controller) {
        return;
    }
    std::vector<mykam::WantedKey> registered;
    registered.reserve(p.hotKeys.size());
    for (const auto& key : p.hotKeys) {
        registered.push_back(key->wanted);
    }
    const mykam::KeyDiff diff = mykam::diffKeys(registered, p.controller->wantedKeys());
    for (const mykam::WantedKey& gone : diff.remove) {
        for (std::size_t i = 0; i < p.hotKeys.size(); ++i) {
            if (p.hotKeys[i]->wanted == gone) {
                XPLMUnregisterHotKey(p.hotKeys[i]->id);
                p.hotKeys.erase(p.hotKeys.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
    }
    for (const mykam::WantedKey& added : diff.add) {
        auto entry = std::make_unique<Plugin::RegisteredKey>();
        entry->wanted = added;
        entry->id = XPLMRegisterHotKey(static_cast<char>(added.key.vkey), static_cast<XPLMKeyFlags>(added.key.flags),
                                       entry->wanted.description.c_str(), hotKeyCallback,
                                       reinterpret_cast<void*>(static_cast<std::intptr_t>(added.viewId)));
        if (entry->id == nullptr) {
            logRaw("warning: X-Plane did not accept the custom key for " + added.description);
            continue;
        }
        p.hotKeys.push_back(std::move(entry));
    }
}

// File name of the user's aircraft, e.g. "Cessna_172SP.acf", and the folder that holds it.
struct AircraftModel {
    std::string fileName;
    std::string folder;
};

AircraftModel currentAircraft() {
    char fileName[512] = {};
    char path[1024] = {};
    XPLMGetNthAircraftModel(0, fileName, path);
    fileName[sizeof(fileName) - 1] = '\0';
    path[sizeof(path) - 1] = '\0';
    AircraftModel m;
    m.fileName = fileName;
    m.folder = mykam::folderOfPath(path);
    return m;
}

void loadCurrentAircraft(mykam::Controller& controller) {
    const AircraftModel m = currentAircraft();
    controller.onAircraftLoaded(m.fileName, m.folder);
}

std::string pluginFolder() {
    char path[2048] = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, path, nullptr, nullptr);
    path[sizeof(path) - 1] = '\0';
    return mykam::pluginFolderFromBinaryPath(path);
}

}  // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    // The SDK guarantees 256-byte buffers for these three strings.
    strcpy_s(outName, 256, "MyKam");
    strcpy_s(outSig, 256, "mykam.camera");
    strcpy_s(outDesc, 256, "Saved cameras with smooth transitions for X-Plane 12");
    try {
        g_plugin = new Plugin();
        if (!g_plugin->sim.init()) {
            logRaw("error: required X-Plane datarefs were not found, plugin not started");
            delete g_plugin;
            g_plugin = nullptr;
            return 0;
        }
        g_plugin->sim.setCameraCallback(cameraCallback, nullptr);
        g_plugin->controller = std::make_unique<mykam::Controller>(g_plugin->sim, pluginFolder());
        g_plugin->sim.setMouseTarget(g_plugin->controller.get());
        g_plugin->controller->start();
        return 1;
    } catch (...) {
        delete g_plugin;
        g_plugin = nullptr;
        return 0;
    }
}

PLUGIN_API void XPluginStop(void) {
    if (g_plugin == nullptr) {
        return;
    }
    try {
        g_plugin->uiWindows.reset();  // windows first: they read the model, which reads the controller
        g_plugin->uiModel.reset();
        if (g_plugin->controller) {
            g_plugin->controller->setOnKeysChanged(nullptr);
        }
        unregisterHotKeys(*g_plugin);
        if (g_plugin->controller) {
            g_plugin->controller->stop();
        }
        g_plugin->sim.destroyMouseLayer();
    } catch (...) {
    }
    delete g_plugin;
    g_plugin = nullptr;
}

PLUGIN_API int XPluginEnable(void) {
    if (g_plugin == nullptr) {
        return 0;
    }
    try {
        Plugin& p = *g_plugin;
        for (int n = 1; n <= mykam::kHotkeyCount; ++n) {
            registerCommand(p, "save_slot_" + std::to_string(n),
                            "Save the current view into the MyKam view that has key " + std::to_string(n) +
                                " (creates it when none has)",
                            kSaveBase + n);
            registerCommand(p, "go_slot_" + std::to_string(n),
                            "Switch to the MyKam view that has key " + std::to_string(n), kGoBase + n);
        }
        registerCommand(p, "release", "Return camera control to X-Plane", kRelease);
        registerCommand(p, "toggle_window", "Show or hide the MyKam window", kToggleWindow);
        registerCommand(p, "next", "Switch to the next saved MyKam view (Internal, then External)", kNext);
        registerCommand(p, "previous", "Switch to the previous saved MyKam view (Internal, then External)", kPrevious);
        registerCommand(p, "toggle_group", "Switch the quick bar between the Internal and External view lists",
                        kToggleGroup);
        registerViewCommands(p);
        registerViewSelectCommands(p);
        p.sim.setHeadFlightLoop(headFlightLoop);
        XPLMRegisterFlightLoopCallback(headFlightLoop, 0.0f, nullptr);  // interval 0: idle until asked
        p.sim.setHandoffFlightLoop(handoffFlightLoop);
        XPLMRegisterFlightLoopCallback(handoffFlightLoop, 0.0f, nullptr);  // idle until a hand-over is requested
        p.controller->setOnKeysChanged([]() {
            try {
                if (g_plugin != nullptr) {
                    applyHotKeys(*g_plugin);
                }
            } catch (...) {
                logRaw("warning: registering the custom keys failed with an unexpected error");
            }
        });
        loadCurrentAircraft(*p.controller);
        p.uiModel = std::make_unique<mykam::UiModel>(*p.controller);
        p.uiWindows = std::make_unique<mykam::UiWindows>();
        if (!p.uiWindows->create(*p.uiModel)) {
            logRaw("warning: could not create the MyKam window, the commands still work");
            p.uiWindows.reset();
        }
        return 1;
    } catch (...) {
        return 0;
    }
}

PLUGIN_API void XPluginDisable(void) {
    if (g_plugin == nullptr) {
        return;
    }
    try {
        g_plugin->uiWindows.reset();
        g_plugin->uiModel.reset();
        if (g_plugin->controller) {
            g_plugin->controller->setOnKeysChanged(nullptr);
        }
        unregisterHotKeys(*g_plugin);
        unregisterCommands(*g_plugin);
        unregisterViewCommands(*g_plugin);
        unregisterViewSelectCommands(*g_plugin);
        XPLMUnregisterFlightLoopCallback(headFlightLoop, nullptr);
        XPLMUnregisterFlightLoopCallback(handoffFlightLoop, nullptr);
        if (g_plugin->controller) {
            g_plugin->controller->release();
        }
        g_plugin->sim.destroyMouseLayer();
    } catch (...) {
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void* param) {
    if (from != XPLM_PLUGIN_XPLANE || g_plugin == nullptr || !g_plugin->controller) {
        return;
    }
    try {
        const std::intptr_t index = reinterpret_cast<std::intptr_t>(param);
        if (message == XPLM_MSG_PLANE_LOADED && index == 0) {
            loadCurrentAircraft(*g_plugin->controller);
        } else if (message == XPLM_MSG_PLANE_UNLOADED && index == 0) {
            g_plugin->controller->release();  // NFR2.3: never hold the camera across an aircraft change
        }
    } catch (...) {
        logRaw("warning: handling an X-Plane message failed with an unexpected error");
    }
}
