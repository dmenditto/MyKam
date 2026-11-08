#include "doctest.h"

#include <string>
#include <vector>

#include "config.h"

TEST_CASE("config: a valid value is used with no warning (FR7)") {
    const mykam::ConfigResult r = mykam::parseConfig("transition_seconds = 2.5\n");
    CHECK(r.config.transitionSeconds == doctest::Approx(2.5));
    CHECK(r.warning.empty());
}

TEST_CASE("config: comments, blank lines, other keys and CRLF are fine") {
    const mykam::ConfigResult r =
        mykam::parseConfig("# comment\r\n; another\r\n\r\nother = 5\r\n  transition_seconds=  3  \r\n");
    CHECK(r.config.transitionSeconds == doctest::Approx(3.0));
    CHECK(r.warning.empty());
}

TEST_CASE("config: a missing file falls back to 1.5 with one warning line") {
    const mykam::ConfigResult r = mykam::defaultConfig("file not found");
    CHECK(r.config.transitionSeconds == doctest::Approx(1.5));
    CHECK(r.warning.find("file not found") != std::string::npos);
    CHECK(r.warning.find('\n') == std::string::npos);
}

TEST_CASE("config: a missing key falls back to 1.5") {
    const mykam::ConfigResult empty = mykam::parseConfig("");
    CHECK(empty.config.transitionSeconds == doctest::Approx(1.5));
    CHECK_FALSE(empty.warning.empty());

    const mykam::ConfigResult other = mykam::parseConfig("something_else = 4\n");
    CHECK(other.config.transitionSeconds == doctest::Approx(1.5));
    CHECK_FALSE(other.warning.empty());
}

TEST_CASE("config: a value that is not a number falls back to 1.5") {
    const char* bad[] = {"transition_seconds = fast", "transition_seconds =", "transition_seconds = 2s",
                         "transition_seconds = nan", "transition_seconds = inf", "transition_seconds = 1,5"};
    for (const char* text : bad) {
        const mykam::ConfigResult r = mykam::parseConfig(text);
        INFO(text);
        CHECK(r.config.transitionSeconds == doctest::Approx(1.5));
        CHECK_FALSE(r.warning.empty());
    }
}

TEST_CASE("config: values outside 0 to 10 fall back to 1.5") {
    const char* bad[] = {"transition_seconds = -0.1", "transition_seconds = 10.01", "transition_seconds = 1e9",
                         "transition_seconds = -5"};
    for (const char* text : bad) {
        const mykam::ConfigResult r = mykam::parseConfig(text);
        INFO(text);
        CHECK(r.config.transitionSeconds == doctest::Approx(1.5));
        CHECK_FALSE(r.warning.empty());
    }
}

TEST_CASE("config: the boundaries 0 and 10 are valid") {
    const mykam::ConfigResult zero = mykam::parseConfig("transition_seconds = 0");
    CHECK(zero.config.transitionSeconds == doctest::Approx(0.0));
    CHECK(zero.warning.empty());
    const mykam::ConfigResult ten = mykam::parseConfig("transition_seconds = 10");
    CHECK(ten.config.transitionSeconds == doctest::Approx(10.0));
    CHECK(ten.warning.empty());
}

TEST_CASE("config: free_camera_handoff 1 and 0 are read with no warning") {
    const mykam::ConfigResult on = mykam::parseConfig("transition_seconds = 1\nfree_camera_handoff = 1\n");
    CHECK(on.config.handoffView == mykam::HandoffView::Saved);  // old 1 maps to saved
    CHECK(on.warning.empty());
    const mykam::ConfigResult off = mykam::parseConfig("transition_seconds = 1\n free_camera_handoff=0 \r\n");
    CHECK(off.config.handoffView == mykam::HandoffView::Off);  // old 0 maps to off
    CHECK(off.warning.empty());
}

TEST_CASE("config: a missing handoff key means saved, quietly") {
    const mykam::ConfigResult r = mykam::parseConfig("transition_seconds = 2\n");
    CHECK(r.config.handoffView == mykam::HandoffView::Saved);
    CHECK(r.warning.empty());
    CHECK(mykam::defaultConfig("file not found").config.handoffView == mykam::HandoffView::Saved);
}

TEST_CASE("config: handoff_view saved, free and off are read, in any letter case, with no warning") {
    struct Row {
        const char* line;
        mykam::HandoffView expected;
    };
    const Row rows[] = {{"handoff_view = saved", mykam::HandoffView::Saved},
                        {"handoff_view=free", mykam::HandoffView::Free},
                        {" handoff_view =  off \r", mykam::HandoffView::Off},
                        {"handoff_view = FREE", mykam::HandoffView::Free},
                        {"handoff_view = Off", mykam::HandoffView::Off}};
    for (const Row& row : rows) {
        const mykam::ConfigResult r = mykam::parseConfig(std::string("transition_seconds = 1\n") + row.line + "\n");
        INFO(row.line);
        CHECK(r.config.handoffView == row.expected);
        CHECK(r.warning.empty());
    }
}

TEST_CASE("config: an invalid handoff_view falls back to saved with one warning line") {
    const char* bad[] = {"handoff_view = chase", "handoff_view =", "handoff_view = 1", "handoff_view = freecam"};
    for (const char* bad1 : bad) {
        const mykam::ConfigResult r = mykam::parseConfig(std::string("transition_seconds = 1\n") + bad1 + "\n");
        INFO(bad1);
        CHECK(r.config.handoffView == mykam::HandoffView::Saved);
        CHECK(r.warning.find("handoff_view") != std::string::npos);
        CHECK(r.warning.find('\n') == std::string::npos);
        CHECK(r.config.transitionSeconds == doctest::Approx(1.0));
    }
}

TEST_CASE("config: handoff_view wins over the old free_camera_handoff key, even when it is invalid") {
    const mykam::ConfigResult a = mykam::parseConfig("transition_seconds = 1\nfree_camera_handoff = 0\nhandoff_view = free\n");
    CHECK(a.config.handoffView == mykam::HandoffView::Free);
    CHECK(a.warning.empty());
    const mykam::ConfigResult b = mykam::parseConfig("transition_seconds = 1\nhandoff_view = saved\nfree_camera_handoff = 0\n");
    CHECK(b.config.handoffView == mykam::HandoffView::Saved);  // order in the file does not matter
    CHECK(b.warning.empty());
    const mykam::ConfigResult c = mykam::parseConfig("transition_seconds = 1\nfree_camera_handoff = 0\nhandoff_view = nope\n");
    CHECK(c.config.handoffView == mykam::HandoffView::Saved);
    CHECK_FALSE(c.warning.empty());
}

TEST_CASE("config: an invalid free_camera_handoff falls back to saved with one warning line") {
    const char* bad[] = {"free_camera_handoff = 2", "free_camera_handoff = yes", "free_camera_handoff =",
                         "free_camera_handoff = 0.0", "free_camera_handoff = -1"};
    for (const char* bad1 : bad) {
        const mykam::ConfigResult r = mykam::parseConfig(std::string("transition_seconds = 1\n") + bad1 + "\n");
        INFO(bad1);
        CHECK(r.config.handoffView == mykam::HandoffView::Saved);
        CHECK(r.warning.find("free_camera_handoff") != std::string::npos);
        CHECK(r.warning.find('\n') == std::string::npos);
        CHECK(r.config.transitionSeconds == doctest::Approx(1.0));
    }
}

TEST_CASE("config: a bad handoff value does not disturb transition_seconds, and the last occurrence wins") {
    const mykam::ConfigResult r = mykam::parseConfig("transition_seconds = 4\nfree_camera_handoff = 1\nfree_camera_handoff = 0\n");
    CHECK(r.config.transitionSeconds == doctest::Approx(4.0));
    CHECK(r.config.handoffView == mykam::HandoffView::Off);

    const mykam::ConfigResult both = mykam::parseConfig("transition_seconds = fast\nfree_camera_handoff = 0\n");
    CHECK(both.config.transitionSeconds == doctest::Approx(1.5));
    CHECK(both.config.handoffView == mykam::HandoffView::Off);
    CHECK_FALSE(both.warning.empty());
}

// ---------------------------------------------------------------- quick_bar_views

namespace {
using Names = std::vector<std::string>;
}

TEST_CASE("config: quick_bar_views missing means cockpit, free, chase, quietly") {
    const mykam::ConfigResult r = mykam::parseConfig("transition_seconds = 2\n");
    CHECK(r.config.quickBarViews == Names({"cockpit", "free", "chase"}));
    CHECK(r.warning.empty());
    CHECK(mykam::Config().quickBarViews == Names({"cockpit", "free", "chase"}));
    CHECK(mykam::defaultConfig("file not found").config.quickBarViews == Names({"cockpit", "free", "chase"}));
}

TEST_CASE("config: quick_bar_views keeps the order of the list, any letter case and spaces") {
    const mykam::ConfigResult r =
        mykam::parseConfig("transition_seconds = 2\nquick_bar_views =  Wingman , chase,TOWER ,linear_spot,cockpit\n");
    CHECK(r.config.quickBarViews == Names({"wingman", "chase", "tower", "linear_spot", "cockpit"}));
    CHECK(r.warning.empty());

    const mykam::ConfigResult all = mykam::parseConfig(
        "transition_seconds = 2\nquick_bar_views = wingman,ridealong,still_spot,linear_spot,runway,tower,circle,chase,"
        "free,cockpit\n");
    CHECK(all.config.quickBarViews.size() == 10);
    CHECK(all.config.quickBarViews.front() == "wingman");
    CHECK(all.config.quickBarViews.back() == "cockpit");
    CHECK(all.warning.empty());
}

TEST_CASE("config: quick_bar_views drops repeated names quietly and keeps the first place") {
    const mykam::ConfigResult r =
        mykam::parseConfig("transition_seconds = 2\nquick_bar_views = chase,circle,CHASE,tower,circle,chase\n");
    CHECK(r.config.quickBarViews == Names({"chase", "circle", "tower"}));
    CHECK(r.warning.empty());
}

TEST_CASE("config: quick_bar_views ignores unknown names with one warning line") {
    const mykam::ConfigResult r =
        mykam::parseConfig("transition_seconds = 2\nquick_bar_views = cockpit,foo,sim/view/chase,free,bar\n");
    CHECK(r.config.quickBarViews == Names({"cockpit", "free"}));
    CHECK(r.warning.find("quick_bar_views") != std::string::npos);
    CHECK(r.warning.find("foo") != std::string::npos);
    CHECK(r.warning.find("bar") != std::string::npos);
    CHECK(r.warning.find('\n') == std::string::npos);
    CHECK(r.config.transitionSeconds == doctest::Approx(2.0));

    // Only unknown names: no buttons, still one warning.
    const mykam::ConfigResult none = mykam::parseConfig("transition_seconds = 2\nquick_bar_views = nope\n");
    CHECK(none.config.quickBarViews.empty());
    CHECK_FALSE(none.warning.empty());
}

TEST_CASE("config: an empty quick_bar_views means no X-Plane view buttons, quietly") {
    const char* const empties[] = {"quick_bar_views =", "quick_bar_views = ", "quick_bar_views = , ,", "quick_bar_views=\r"};
    for (const char* line : empties) {
        const mykam::ConfigResult r = mykam::parseConfig(std::string("transition_seconds = 2\n") + line + "\n");
        INFO(line);
        CHECK(r.config.quickBarViews.empty());
        CHECK(r.warning.empty());
    }
}

TEST_CASE("config: the last quick_bar_views line wins") {
    const mykam::ConfigResult r =
        mykam::parseConfig("transition_seconds = 2\nquick_bar_views = tower\nquick_bar_views = runway,circle\n");
    CHECK(r.config.quickBarViews == Names({"runway", "circle"}));
}

TEST_CASE("config: validateConfig accepts known unique view names and rejects the rest") {
    mykam::Config c;
    std::string problem;
    CHECK(mykam::validateConfig(c, problem));
    c.quickBarViews.clear();
    CHECK(mykam::validateConfig(c, problem));  // none is fine
    c.quickBarViews = {"tower", "cockpit"};
    CHECK(mykam::validateConfig(c, problem));
    c.quickBarViews = {"tower", "nope"};
    CHECK_FALSE(mykam::validateConfig(c, problem));
    CHECK(problem.find("nope") != std::string::npos);
    c.quickBarViews = {"tower", "cockpit", "tower"};
    CHECK_FALSE(mykam::validateConfig(c, problem));
    c.quickBarViews = {"Tower"};  // names are stored in lowercase
    CHECK_FALSE(mykam::validateConfig(c, problem));
}

TEST_CASE("config: updateConfigText writes quick_bar_views in place, appends it when missing, and it reads back") {
    mykam::Config c;
    c.quickBarViews = {"wingman", "cockpit"};
    const std::string appended = mykam::updateConfigText("# notes\ntransition_seconds = 1\nsomething = 7\n", c);
    CHECK(appended.find("quick_bar_views = wingman,cockpit\n") != std::string::npos);
    CHECK(appended.find("# notes\n") != std::string::npos);
    CHECK(appended.find("something = 7\n") != std::string::npos);
    CHECK(mykam::parseConfig(appended).config.quickBarViews == Names({"wingman", "cockpit"}));

    c.quickBarViews = {"circle"};
    const std::string replaced = mykam::updateConfigText(appended, c);
    CHECK(replaced.find("quick_bar_views = circle\n") != std::string::npos);
    CHECK(replaced.find("wingman,cockpit") == std::string::npos);
    CHECK(mykam::updateConfigText(replaced, c) == replaced);  // a second write changes nothing

    c.quickBarViews.clear();
    const std::string emptied = mykam::updateConfigText(replaced, c);
    CHECK(emptied.find("quick_bar_views =\n") != std::string::npos);
    const mykam::ConfigResult back = mykam::parseConfig(emptied);
    CHECK(back.config.quickBarViews.empty());
    CHECK(back.warning.empty());

    CHECK(mykam::quickBarViewsText({"a", "b", "c"}) == "a,b,c");
    CHECK(mykam::quickBarViewsText({}).empty());
}
