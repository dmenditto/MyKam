#include "doctest.h"

#include <string>

#include "companions.h"
#include "controller.h"
#include "fake_sim.h"
#include "ui_model.h"

using mykam::CameraState;
using mykam::Controller;
using mykam::Pose;
using mykam::ViewKind;
using mykam::XprMode;

namespace {

const char* const kIniPath = "plug/mykam.ini";
const char* const kXCamera = "SRS.X-Camera";

// A controller with an instant transition (outside views) and the Cessna loaded.
struct Rig {
    FakeSim sim;
    Controller controller;
    Rig() : controller(sim, "plug") {
        sim.files[kIniPath] = "transition_seconds = 0\n";
        controller.start();
        controller.onAircraftLoaded("Cessna_172SP.acf");
        sim.logs.clear();
        // XPRealistic sees its own commands: the fake plays the before-handler call.
        sim.onPluginCommand = [this](const std::string& name) {
            controller.onXprCommand(name == mykam::kXprMasterCommand);
        };
    }
    void withXpr() {
        sim.enabledPlugins.insert(mykam::kXprSignature);
        controller.checkCompanions();
    }
    int newOutside(XprMode mode) {
        sim.viewExternal = true;
        sim.view = Pose();
        sim.view.x = 10.0;
        sim.view.zoom = 1.0;
        const int id = controller.saveCurrentAsNew();
        mykam::ViewEdit e;
        e.name = controller.slots().find(id)->name;
        e.xpr = mode;
        REQUIRE(controller.editView(id, e) == mykam::EditResult::Saved);
        return id;
    }
    int freezeRuns() const {
        int n = 0;
        for (const std::string& c : sim.pluginCommandsRun) {
            n += c == mykam::kXprFreezeCommand ? 1 : 0;
        }
        return n;
    }
};

}  // namespace

// ---------------------------------------------------------------- issue #1: other camera plugins

TEST_CASE("companions: X-Camera running is logged once, shown in the window and cleared when it stops") {
    Rig rig;
    rig.controller.checkCompanions();
    CHECK(rig.controller.otherCameraPlugins().empty());
    CHECK(rig.sim.logs.empty());

    rig.sim.enabledPlugins.insert(kXCamera);
    const unsigned before = rig.controller.revision();
    rig.controller.checkCompanions();
    rig.controller.checkCompanions();  // no change: no second line
    CHECK(rig.controller.otherCameraPlugins() == "X-Camera");
    CHECK(rig.controller.revision() > before);
    CHECK(rig.sim.logCount("warning: X-Camera is running next to MyKam") == 1);
    mykam::UiModel model(rig.controller);
    CHECK(model.cameraConflictNote().find("X-Camera is running too") == 0);
    CHECK(model.cameraConflictNote().find("Plugin Admin") != std::string::npos);

    rig.sim.enabledPlugins.clear();
    rig.controller.checkCompanions();
    CHECK(rig.controller.otherCameraPlugins().empty());
    CHECK(model.cameraConflictNote().empty());
    CHECK(rig.sim.logCount("no other camera plugin is running any more") == 1);
}

TEST_CASE("companions: X-Camera switched off from its own controls (still enabled in Plugin Admin) clears it") {
    Rig rig;
    const char* const active = "SRS/X-Camera/integration/X-Camera_enabled";
    rig.sim.enabledPlugins.insert(kXCamera);
    rig.sim.pluginDatarefs[active] = 1;
    rig.controller.checkCompanions();
    CHECK(rig.controller.otherCameraPlugins() == "X-Camera");

    rig.sim.pluginDatarefs[active] = 0;  // X-Camera's own off switch
    rig.controller.checkCompanions();
    CHECK(rig.controller.otherCameraPlugins().empty());
    CHECK(rig.sim.logCount("no other camera plugin is running any more") == 1);

    rig.sim.pluginDatarefs[active] = 1;  // and on again
    rig.controller.checkCompanions();
    CHECK(rig.controller.otherCameraPlugins() == "X-Camera");
    CHECK(rig.sim.logCount("warning: X-Camera is running next to MyKam") == 2);

    rig.sim.pluginDatarefs.clear();  // an X-Camera without that dataref counts as on
    rig.controller.checkCompanions();
    CHECK(rig.controller.otherCameraPlugins() == "X-Camera");
}

// ---------------------------------------------------------------- issue #7: XPRealistic per view

TEST_CASE("xprealistic: the per-view choice is stored as viewN.xprealistic and read back") {
    mykam::ViewStore s;
    Pose p;
    p.zoom = 1.0;
    const int a = s.add(p, ViewKind::External);
    const int b = s.add(p, ViewKind::Cockpit);
    const int c = s.add(p, ViewKind::External);
    REQUIRE(s.setXpr(a, XprMode::Off));
    REQUIRE(s.setXpr(b, XprMode::On));
    CHECK_FALSE(s.setXpr(99, XprMode::On));
    const std::string text = s.serialize();
    CHECK(text.find("view" + std::to_string(a) + ".xprealistic=off\n") != std::string::npos);
    CHECK(text.find("view" + std::to_string(b) + ".xprealistic=on\n") != std::string::npos);
    CHECK(text.find("view" + std::to_string(c) + ".xprealistic") == std::string::npos);  // Keep writes nothing

    mykam::ViewStore back;
    const mykam::ViewParseResult r = back.parse(text + "view" + std::to_string(c) + ".xprealistic=maybe\n");
    CHECK(back.find(a)->xpr == XprMode::Off);
    CHECK(back.find(b)->xpr == XprMode::On);
    CHECK(back.find(c)->xpr == XprMode::Keep);
    REQUIRE(r.problems.size() == 1);
    CHECK(r.problems[0].find("xprealistic 'maybe' ignored") != std::string::npos);

    XprMode m = XprMode::Keep;
    CHECK(mykam::parseXprMode("OFF", m));
    CHECK(m == XprMode::Off);
    CHECK_FALSE(mykam::parseXprMode("1", m));
    CHECK(m == XprMode::Off);
}

TEST_CASE("xprealistic: Off pauses it on arrival, On resumes it, Leave as is changes nothing") {
    Rig rig;
    rig.withXpr();
    CHECK(rig.controller.xprAvailable());
    CHECK(rig.sim.logCount("XPRealistic found") == 1);
    CHECK_FALSE(rig.controller.xprFrozen());
    const int off = rig.newOutside(XprMode::Off);
    const int on = rig.newOutside(XprMode::On);
    const int keep = rig.newOutside(XprMode::Keep);

    rig.controller.goToView(off);
    CHECK(rig.controller.xprFrozen());
    CHECK(rig.freezeRuns() == 1);
    CHECK(rig.sim.logCount("XPRealistic effects paused for") == 1);
    rig.controller.goToView(off);  // already paused: no second toggle
    CHECK(rig.freezeRuns() == 1);
    rig.controller.goToView(keep);
    CHECK(rig.controller.xprFrozen());
    CHECK(rig.freezeRuns() == 1);
    rig.controller.goToView(on);
    CHECK_FALSE(rig.controller.xprFrozen());
    CHECK(rig.freezeRuns() == 2);
    CHECK(rig.sim.logCount("XPRealistic effects resumed for") == 1);
}

TEST_CASE("xprealistic: toggles from keys or its menu are counted, and the master switch is respected") {
    Rig rig;
    rig.withXpr();
    const int off = rig.newOutside(XprMode::Off);

    rig.controller.onXprCommand(false);  // the user froze it with a key
    CHECK(rig.controller.xprFrozen());
    rig.controller.goToView(off);
    CHECK(rig.freezeRuns() == 0);  // already frozen

    rig.controller.onXprCommand(false);  // unfrozen again
    rig.controller.onXprCommand(true);   // master switch off
    CHECK_FALSE(rig.controller.xprMasterOn());
    rig.controller.goToView(off);
    CHECK(rig.freezeRuns() == 0);
    CHECK(rig.sim.logCount("XPRealistic is switched off (master switch)") == 1);
}

TEST_CASE("xprealistic: without XPRealistic nothing is run; when it comes back it starts on and not frozen") {
    Rig rig;
    const int off = rig.newOutside(XprMode::Off);
    rig.controller.goToView(off);
    CHECK(rig.sim.pluginCommandsRun.empty());
    rig.controller.onXprCommand(false);  // a stray call while it is not there changes nothing
    CHECK_FALSE(rig.controller.xprFrozen());

    rig.withXpr();
    rig.controller.release();
    rig.controller.goToView(off);
    CHECK(rig.controller.xprFrozen());
    rig.sim.enabledPlugins.clear();
    rig.controller.checkCompanions();
    CHECK_FALSE(rig.controller.xprAvailable());
    CHECK(rig.sim.logCount("XPRealistic is no longer running") == 1);
    rig.withXpr();
    CHECK_FALSE(rig.controller.xprFrozen());  // re-enabled: XPRealistic starts unfrozen
    CHECK(rig.controller.xprMasterOn());
}

TEST_CASE("xprealistic: a missing command is reported and the state is not changed") {
    Rig rig;
    rig.withXpr();
    rig.sim.pluginCommandsWork = false;
    const int off = rig.newOutside(XprMode::Off);
    rig.controller.goToView(off);
    CHECK_FALSE(rig.controller.xprFrozen());
    CHECK(rig.sim.logCount("warning: command XPRealistic/FreezeMode not found") == 1);
}

TEST_CASE("xprealistic: a cockpit view set to On resumes it only when the glide has ended") {
    FakeSim sim;
    Controller controller(sim, "plug");
    sim.files[kIniPath] = "transition_seconds = 1\n";
    controller.start();
    controller.onAircraftLoaded("Cessna_172SP.acf");
    sim.onPluginCommand = [&controller](const std::string& name) {
        controller.onXprCommand(name == mykam::kXprMasterCommand);
    };
    sim.enabledPlugins.insert(mykam::kXprSignature);
    controller.checkCompanions();

    sim.viewExternal = false;
    sim.head = Pose();
    sim.head.heading = 20.0;
    const int cockpit = controller.saveCurrentAsNew();
    REQUIRE(cockpit != 0);
    mykam::ViewEdit e;
    e.name = controller.slots().find(cockpit)->name;
    e.xpr = XprMode::On;
    REQUIRE(controller.editView(cockpit, e) == mykam::EditResult::Saved);

    controller.onXprCommand(false);  // the user had paused it
    REQUIRE(controller.xprFrozen());
    sim.head.heading = -40.0;  // looking elsewhere, so there is a glide back
    controller.goToView(cockpit);
    REQUIRE(controller.cockpitAnimating());
    CHECK(controller.xprFrozen());  // still paused during the glide
    CHECK(sim.pluginCommandsRun.empty());
    controller.tickCockpit(0.5);
    CHECK(controller.xprFrozen());
    controller.tickCockpit(1.0);  // glide ends
    CHECK_FALSE(controller.cockpitAnimating());
    CHECK_FALSE(controller.xprFrozen());
    CHECK(sim.pluginCommandsRun.size() == 1);
}

TEST_CASE("xprealistic: the edit dialog offers the choice while XPRealistic runs, and saves it") {
    Rig rig;
    const int id = rig.newOutside(XprMode::Keep);
    mykam::UiModel model(rig.controller);
    mykam::EditDraft d = model.beginEdit(id);
    CHECK_FALSE(model.showXprChoice(d));
    rig.withXpr();
    CHECK(model.showXprChoice(d));
    d.xpr = XprMode::Off;
    CHECK(model.saveEdit(d) == mykam::EditResult::Saved);
    CHECK(rig.controller.slots().find(id)->xpr == XprMode::Off);
    rig.sim.enabledPlugins.clear();
    rig.controller.checkCompanions();
    CHECK(model.showXprChoice(model.beginEdit(id)));  // a view that has a choice still shows it
}

TEST_CASE("xprealistic: the quick bar button pauses and resumes it and the rows show each view's choice") {
    Rig rig;
    mykam::UiModel model(rig.controller);
    CHECK_FALSE(model.toggleXpr());  // not loaded: nothing to do
    CHECK(rig.sim.pluginCommandsRun.empty());

    rig.withXpr();
    CHECK(model.xprRunning());
    CHECK(model.xprButtonTooltip() == "XPRealistic effects on - click to pause");
    CHECK(model.toggleXpr());
    CHECK_FALSE(model.xprRunning());
    CHECK(rig.controller.xprFrozen());
    CHECK(rig.freezeRuns() == 1);
    CHECK(rig.sim.logCount("XPRealistic effects paused from the quick bar") == 1);
    CHECK(model.xprButtonTooltip() == "XPRealistic effects paused - click to resume");
    CHECK(model.toggleXpr());
    CHECK(model.xprRunning());

    rig.controller.onXprCommand(true);  // master switch off from XPRealistic's menu
    CHECK_FALSE(model.xprRunning());
    CHECK_FALSE(model.toggleXpr());
    CHECK(model.xprButtonTooltip() == "XPRealistic is switched off (its master switch)");

    const int off = rig.newOutside(XprMode::Off);
    const int on = rig.newOutside(XprMode::On);
    const int keep = rig.newOutside(XprMode::Keep);
    model.refresh();
    for (const mykam::ViewRow& row : model.rows(mykam::ViewGroup::External)) {
        if (row.id == off) CHECK(std::string(mykam::UiModel::xprRowText(row.xpr)) == "XPR off");
        if (row.id == on) CHECK(std::string(mykam::UiModel::xprRowText(row.xpr)) == "XPR on");
        if (row.id == keep) CHECK(std::string(mykam::UiModel::xprRowText(row.xpr)).empty());
    }
}
