#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <deque>
#include <numeric>

using namespace geode::prelude;

class $modify(SmootherLayer, GJBaseGameLayer) {
    struct Fields {
        double now = 0.0;
        double lastPress = -1.0;
        double nextFill = -1.0;
        double releaseAt = -1.0;
        bool synthetic = false;
        bool realHeld = false;
        int consecutiveFills = 0;
        int mode = 0;
        std::deque<double> intervals;
    };

    int currentMode() {
        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled") || !m_player1) return 0;
        if (m_player1->m_isDart)
            return mod->getSettingValue<bool>("wave-enabled") ? 1 : 0;
        if (m_player1->m_isShip)
            return mod->getSettingValue<bool>("ship-enabled") ? 2 : 0;
        return 0;
    }

    double modeMinCps(int mode) {
        auto mod = Mod::get();
        if (mode == 1) return mod->getSettingValue<double>("wave-cps");
        return mod->getSettingValue<double>("ship-cps");
    }

    double avgInterval() {
        auto f = m_fields.self();
        if (f->intervals.empty()) return 0.0;
        return std::accumulate(f->intervals.begin(), f->intervals.end(), 0.0)
            / static_cast<double>(f->intervals.size());
    }

    void resetRhythm() {
        auto f = m_fields.self();
        f->intervals.clear();
        f->lastPress = -1.0;
        f->nextFill = -1.0;
        f->releaseAt = -1.0;
        f->consecutiveFills = 0;
    }

    void recordPress(bool real, int mode) {
        auto f = m_fields.self();
        double minCps = modeMinCps(mode);
        double tol = Mod::get()->getSettingValue<double>("tolerance");
        double window = (1.0 / minCps) * tol;

        if (f->lastPress >= 0.0) {
            double gap = f->now - f->lastPress;
            if (gap <= window) {
                f->intervals.push_back(gap);
                if (f->intervals.size() > 6) f->intervals.pop_front();
            } else {
                f->intervals.clear();
            }
        }

        f->lastPress = f->now;
        if (real) f->consecutiveFills = 0;

        if (f->intervals.size() >= 4 && avgInterval() <= 1.0 / minCps)
            f->nextFill = f->now + avgInterval() * tol;
        else
            f->nextFill = -1.0;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto f = m_fields.self();
        if (!f->synthetic && button == 1 && isPlayer1) {
            if (down) {
                f->realHeld = true;
                int mode = currentMode();
                if (mode != f->mode) {
                    resetRhythm();
                    f->mode = mode;
                }
                if (mode != 0) recordPress(true, mode);
            } else {
                f->realHeld = false;
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);

        auto f = m_fields.self();
        f->now += dt;

        int mode = currentMode();
        if (mode != f->mode) {
            resetRhythm();
            f->mode = mode;
        }
        if (mode == 0) return;

        if (f->releaseAt >= 0.0 && f->now >= f->releaseAt) {
            f->releaseAt = -1.0;
            f->synthetic = true;
            GJBaseGameLayer::handleButton(false, 1, true);
            f->synthetic = false;
        }

        if (f->nextFill >= 0.0 && f->now >= f->nextFill) {
            int maxFills = static_cast<int>(
                Mod::get()->getSettingValue<int64_t>("max-fills")
            );

            if (!f->realHeld && f->consecutiveFills < maxFills) {
                f->synthetic = true;
                GJBaseGameLayer::handleButton(true, 1, true);
                f->synthetic = false;

                f->releaseAt = f->now + 0.02;
                f->consecutiveFills++;
                f->lastPress = f->now;
                f->nextFill = f->now + avgInterval();
            } else {
                f->nextFill = -1.0;
            }
        }
    }
};

$on_game(Loaded) {
    listenForKeybindSettingPresses(
        "open-menu",
        [](Keybind const&, bool down, bool repeat, double) {
            if (down && !repeat) {
                geode::openSettingsPopup(Mod::get());
            }
        }
    );
}
