#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    enum class RouteMode {
        Idle,
        Recording,
        Playing
    };

    struct InputEvent {
        std::uint32_t frame = 0;
        int button = 1;
        bool down = false;
        bool playerOne = true;
    };

    bool g_previewNoDeath = false;
    bool g_inCommands = false;
    bool g_injecting = false;
    bool g_replayFailed = false;

    RouteMode g_routeMode = RouteMode::Idle;
    std::vector<InputEvent> g_routeEvents;
    std::size_t g_playIndex = 0;
    std::uint32_t g_lastFrame = 0;
    int g_replayRetries = 0;

    std::string routeKey(GJGameLevel* level) {
        if (!level) {
            return {};
        }

        const auto levelID = level->m_levelID.value();
        if (levelID > 0) {
            return "route_" + std::to_string(levelID) +
                "_rev_" + std::to_string(level->m_levelRev);
        }

        // Local levels often have no online level ID. Hash the name so each
        // local level gets its own saved route key.
        std::uint32_t hash = 2166136261u;
        const char* name = level->m_levelName.c_str();
        if (name) {
            for (const unsigned char* p =
                    reinterpret_cast<const unsigned char*>(name); *p; ++p) {
                hash ^= *p;
                hash *= 16777619u;
            }
        }
        return "route_local_" + std::to_string(hash);
    }

    std::string practiceMarkKey(GJGameLevel* level) {
        auto key = routeKey(level);
        return key.empty() ? std::string{} : key + "_practice_mark";
    }

    std::string serializeRoute() {
        std::ostringstream out;
        out << "MODGD-INPUT-1\n";
        for (const auto& event : g_routeEvents) {
            out << event.frame << ' '
                << event.button << ' '
                << (event.down ? 1 : 0) << ' '
                << (event.playerOne ? 1 : 0) << '\n';
        }
        return out.str();
    }

    bool parseRoute(const std::string& data, std::vector<InputEvent>& events) {
        std::istringstream in(data);
        std::string header;
        if (!std::getline(in, header) || header != "MODGD-INPUT-1") {
            return false;
        }

        std::vector<InputEvent> parsed;
        InputEvent event;
        int down = 0;
        int playerOne = 1;
        while (in >> event.frame >> event.button >> down >> playerOne) {
            event.down = down != 0;
            event.playerOne = playerOne != 0;
            parsed.push_back(event);
        }

        if (parsed.empty()) {
            return false;
        }

        events = std::move(parsed);
        return true;
    }

    std::string loadSavedRoute(GJGameLevel* level) {
        const auto key = routeKey(level);
        if (key.empty()) {
            return {};
        }
        return Mod::get()->getSavedValue<std::string>(key, "");
    }

    bool saveCurrentRoute(PlayLayer* layer) {
        if (!layer || !layer->m_level || g_routeEvents.empty()) {
            return false;
        }

        const auto key = routeKey(layer->m_level);
        if (key.empty()) {
            return false;
        }

        Mod::get()->setSavedValue<std::string>(key, serializeRoute());
        return true;
    }

    int lastPracticeMark(GJGameLevel* level) {
        const auto key = practiceMarkKey(level);
        if (key.empty()) {
            return -1;
        }
        return Mod::get()->getSavedValue<int>(key, -1);
    }

    void showNotice(const char* message) {
        FLAlertLayer::create("Mod-GD", message, "OK")->show();
    }
}

// Capture and inject inputs at Geometry Dash's own simulation-step boundary.
// Because the game still handles the press/release, mode changes, portals,
// pads, orbs and dual-player rules remain native to Geometry Dash.
class $modify(ModGDBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto* layer = PlayLayer::get();
        const bool isCurrentPlayLayer =
            layer && static_cast<GJBaseGameLayer*>(layer) == this;

        if (isCurrentPlayLayer && !g_injecting) {
            if (g_routeMode == RouteMode::Playing) {
                // Do not mix physical clicks into a saved route playback.
                return;
            }

            if (g_routeMode == RouteMode::Recording &&
                !g_previewNoDeath &&
                !layer->m_isPracticeMode &&
                !layer->m_hasCompletedLevel) {
                const auto frame = m_gameState.m_currentProgress +
                    (g_inCommands ? 0u : 1u);
                g_routeEvents.push_back({
                    frame,
                    button,
                    down,
                    isPlayer1
                });
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* layer = PlayLayer::get();
        const bool isCurrentPlayLayer =
            layer && static_cast<GJBaseGameLayer*>(layer) == this;

        if (isCurrentPlayLayer && g_routeMode == RouteMode::Playing) {
            const auto frame = m_gameState.m_currentProgress;

            // Practice/checkpoint loads can move the simulation counter
            // backwards. Seek to the first event at the restored frame.
            if (frame < g_lastFrame) {
                auto it = std::lower_bound(
                    g_routeEvents.begin(),
                    g_routeEvents.end(),
                    frame,
                    [](const InputEvent& event, std::uint32_t value) {
                        return event.frame < value;
                    }
                );
                g_playIndex = static_cast<std::size_t>(
                    it - g_routeEvents.begin()
                );
            }
            g_lastFrame = frame;

            g_injecting = true;
            while (g_playIndex < g_routeEvents.size() &&
                   g_routeEvents[g_playIndex].frame <= frame) {
                const auto event = g_routeEvents[g_playIndex++];
                GJBaseGameLayer::handleButton(
                    event.down,
                    event.button,
                    event.playerOne
                );
            }
            g_injecting = false;
        }

        const bool wasInCommands = g_inCommands;
        if (isCurrentPlayLayer) {
            g_inCommands = true;
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        g_inCommands = wasInCommands;
    }
}

class $modify(ModGDPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* statusLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        g_previewNoDeath = false;
        if (g_routeMode != RouteMode::Playing) {
            g_routeEvents.clear();
            g_routeMode = RouteMode::Idle;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto status = CCLabelBMFont::create("", "bigFont.fnt");
        if (status) {
            status->setScale(0.34f);
            status->setColor(ccc3(90, 235, 255));
            status->setPosition(CCPoint(winSize.width / 2.f, 22.f));
            status->setZOrder(9999);
            status->setVisible(false);
            this->addChild(status, 9999);
            m_fields->statusLabel = status;
        }

        return true;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (g_routeMode != RouteMode::Playing) {
            g_routeEvents.clear();
            g_routeMode = RouteMode::Recording;
            g_playIndex = 0;
            g_lastFrame = 0;
            g_replayRetries = 0;
            g_replayFailed = false;
        }
    }

    void update(float dt) {
        PlayLayer::update(dt);

        auto* label = m_fields->statusLabel;
        if (!label) {
            return;
        }

        if (g_previewNoDeath) {
            label->setString("MOD-GD  |  PREVIEW (NO DEATH)");
            label->setVisible(true);
        } else if (g_routeMode == RouteMode::Playing) {
            const auto text = "MOD-GD  |  AUTO-PLAY  " +
                std::to_string(std::min(g_playIndex, g_routeEvents.size())) +
                "/" + std::to_string(g_routeEvents.size());
            label->setString(text.c_str());
            label->setVisible(true);
        } else if (g_routeMode == RouteMode::Recording) {
            label->setString("MOD-GD  |  LEARNING INPUT ROUTE");
            label->setVisible(true);
        } else if (g_replayRetries >= 3) {
            label->setString("MOD-GD  |  ROUTE STOPPED");
            label->setVisible(true);
        } else {
            label->setVisible(false);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_previewNoDeath) {
            // Preview mode suppresses the death transition. It does not turn
            // off collision or calculate a valid path.
            return;
        }

        if (m_isPracticeMode && m_level) {
            Mod::get()->setSavedValue<int>(
                practiceMarkKey(m_level),
                this->getCurrentPercentInt()
            );
        }

        if (g_routeMode == RouteMode::Playing) {
            g_replayFailed = true;
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        if (g_routeMode == RouteMode::Recording && !m_isPracticeMode) {
            // A normal-mode retry starts a fresh trace from the beginning.
            g_routeEvents.clear();
        }

        if (g_routeMode == RouteMode::Playing) {
            if (g_replayFailed) {
                ++g_replayRetries;
                if (g_replayRetries >= 3) {
                    g_routeMode = RouteMode::Idle;
                    g_replayFailed = false;
                    g_playIndex = 0;
                    return;
                }
            }

            g_replayFailed = false;
            g_playIndex = 0;
            g_lastFrame = 0;
        }
    }

    void levelComplete() {
        if (g_routeMode == RouteMode::Recording &&
            !g_previewNoDeath &&
            !m_isPracticeMode) {
            if (saveCurrentRoute(this)) {
                log::info(
                    "[Mod-GD] Saved a clean route with {} input events",
                    g_routeEvents.size()
                );
            }
        }

        if (g_routeMode == RouteMode::Playing) {
            g_routeMode = RouteMode::Idle;
        }

        PlayLayer::levelComplete();
    }
};

// PauseLayer::customSetup is the correct place for the MGD launcher in GD 2.2081.
class $modify(ModGDPauseLayer, PauseLayer) {
    struct Fields {
        CCLayerColor* overlay = nullptr;
        CCLabelBMFont* previewStatus = nullptr;
        CCLabelBMFont* routeStatus = nullptr;
        CCLabelBMFont* practiceStatus = nullptr;
    };

    void customSetup() {
        PauseLayer::customSetup();

        const auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Keep the larger launcher at the left side, far from the pause
        // settings controls on the right.
        auto launcherMenu = CCMenu::create();
        launcherMenu->setPosition(CCPointZero);
        launcherMenu->setID("mod-gd-launcher-menu"_spr);

        auto icon = CCSprite::create("mod-gd-icon.png"_spr);
        if (icon) {
            icon->setScale(0.19f);
        } else {
            icon = ButtonSprite::create(
                "MGD", 84, true, "bigFont.fnt", "GJ_button_04.png", 34.f, 0.72f
            );
        }

        auto launcher = CCMenuItemSpriteExtra::create(
            icon, this, menu_selector(ModGDPauseLayer::onOpenModMenu)
        );
        launcher->setPosition(CCPoint(68.f, winSize.height * 0.5f));
        launcher->setID("mod-gd-launcher"_spr);
        launcherMenu->addChild(launcher);
        this->addChild(launcherMenu, 1000);

        auto overlay = CCLayerColor::create(
            ccc4(0, 0, 0, 175), winSize.width, winSize.height
        );
        if (!overlay) {
            log::error("[Mod-GD] Could not create the menu overlay");
            return;
        }

        overlay->setVisible(false);
        overlay->setID("mod-gd-overlay"_spr);
        this->addChild(overlay, 2000);
        m_fields->overlay = overlay;

        constexpr float panelWidth = 520.f;
        constexpr float panelHeight = 380.f;
        const CCPoint panelPos(
            (winSize.width - panelWidth) / 2.f,
            (winSize.height - panelHeight) / 2.f
        );

        // Two-tone frame and blue body match Geometry Dash's in-game palette.
        auto frame = CCLayerColor::create(
            ccc4(68, 203, 255, 255),
            panelWidth + 6.f,
            panelHeight + 6.f
        );
        frame->setPosition(CCPoint(panelPos.x - 3.f, panelPos.y - 3.f));
        overlay->addChild(frame);

        auto panel = CCLayerColor::create(
            ccc4(30, 48, 96, 250),
            panelWidth,
            panelHeight
        );
        panel->setPosition(panelPos);
        overlay->addChild(panel);

        auto topBar = CCLayerColor::create(
            ccc4(255, 207, 77, 255),
            panelWidth - 8.f,
            5.f
        );
        topBar->setPosition(CCPoint(4.f, panelHeight - 8.f));
        panel->addChild(topBar);

        auto title = CCLabelBMFont::create("MOD-GD AUTOPILOT", "goldFont.fnt");
        title->setScale(0.78f);
        title->setPosition(CCPoint(panelWidth / 2.f, 340.f));
        panel->addChild(title);

        auto subtitle = CCLabelBMFont::create(
            "PREVIEW  /  LEARN  /  REPLAY", "bigFont.fnt"
        );
        subtitle->setScale(0.43f);
        subtitle->setColor(ccc3(170, 230, 255));
        subtitle->setPosition(CCPoint(panelWidth / 2.f, 309.f));
        panel->addChild(subtitle);

        auto info = CCLabelBMFont::create(
            "Routes use Geometry Dash's native press and release input.", "bigFont.fnt"
        );
        info->setScale(0.34f);
        info->setColor(ccc3(218, 229, 250));
        info->setPosition(CCPoint(panelWidth / 2.f, 280.f));
        panel->addChild(info);

        auto menu = CCMenu::create();
        menu->setPosition(CCPointZero);
        panel->addChild(menu);

        auto previewSprite = ButtonSprite::create(
            "PREVIEW NO-DEATH", 220, true, "bigFont.fnt",
            "GJ_button_01.png", 40.f, 0.68f
        );
        auto previewButton = CCMenuItemSpriteExtra::create(
            previewSprite, this, menu_selector(ModGDPauseLayer::onTogglePreview)
        );
        previewButton->setPosition(CCPoint(148.f, 230.f));
        menu->addChild(previewButton);

        auto previewStatus = CCLabelBMFont::create("OFF", "bigFont.fnt");
        previewStatus->setScale(0.48f);
        previewStatus->setColor(ccc3(255, 222, 125));
        previewStatus->setPosition(CCPoint(390.f, 230.f));
        panel->addChild(previewStatus);
        m_fields->previewStatus = previewStatus;

        auto learnSprite = ButtonSprite::create(
            "LEARN ROUTE", 220, true, "goldFont.fnt",
            "GJ_button_02.png", 40.f, 0.72f
        );
        auto learnButton = CCMenuItemSpriteExtra::create(
            learnSprite, this, menu_selector(ModGDPauseLayer::onLearnRoute)
        );
        learnButton->setPosition(CCPoint(148.f, 162.f));
        menu->addChild(learnButton);

        auto routeStatus = CCLabelBMFont::create("RECORD", "bigFont.fnt");
        routeStatus->setScale(0.43f);
        routeStatus->setColor(ccc3(153, 255, 177));
        routeStatus->setPosition(CCPoint(390.f, 162.f));
        panel->addChild(routeStatus);
        m_fields->routeStatus = routeStatus;

        auto playSprite = ButtonSprite::create(
            "PLAY SAVED ROUTE", 220, true, "goldFont.fnt",
            "GJ_button_03.png", 40.f, 0.67f
        );
        auto playButton = CCMenuItemSpriteExtra::create(
            playSprite, this, menu_selector(ModGDPauseLayer::onPlaySavedRoute)
        );
        playButton->setPosition(CCPoint(148.f, 94.f));
        menu->addChild(playButton);

        auto practiceStatus = CCLabelBMFont::create("NO PRACTICE MARK", "bigFont.fnt");
        practiceStatus->setScale(0.34f);
        practiceStatus->setColor(ccc3(255, 215, 120));
        practiceStatus->setPosition(CCPoint(panelWidth / 2.f, 55.f));
        panel->addChild(practiceStatus);
        m_fields->practiceStatus = practiceStatus;

        auto closeSprite = ButtonSprite::create(
            "CLOSE", 120, true, "goldFont.fnt",
            "GJ_button_04.png", 34.f, 0.75f
        );
        auto closeButton = CCMenuItemSpriteExtra::create(
            closeSprite, this, menu_selector(ModGDPauseLayer::onCloseModMenu)
        );
        closeButton->setPosition(CCPoint(panelWidth / 2.f, 22.f));
        menu->addChild(closeButton);

        log::info("[Mod-GD] In-game styled launcher and route menu added");
    }

    void onOpenModMenu(CCObject*) {
        if (m_fields->overlay) {
            m_fields->overlay->setVisible(true);
        }
        this->refreshLabels();
    }

    void onCloseModMenu(CCObject*) {
        if (m_fields->overlay) {
            m_fields->overlay->setVisible(false);
        }
    }

    void onTogglePreview(CCObject*) {
        g_previewNoDeath = !g_previewNoDeath;

        if (g_previewNoDeath) {
            g_routeMode = RouteMode::Idle;
            g_routeEvents.clear();
        }

        this->refreshLabels();

        if (!g_previewNoDeath) {
            showNotice(
                "Preview is off. Press LEARN ROUTE to restart from the beginning "
                "and record a clean normal-mode run."
            );
        }
    }

    void onLearnRoute(CCObject*) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first.");
            return;
        }
        if (layer->m_isPracticeMode) {
            showNotice(
                "Route learning is for a normal-mode clear. Practice deaths are "
                "saved as markers, but practice attempts are not stored as a clean route."
            );
            return;
        }

        g_previewNoDeath = false;
        g_routeEvents.clear();
        g_routeMode = RouteMode::Recording;
        g_playIndex = 0;
        g_lastFrame = 0;
        g_replayRetries = 0;
        g_replayFailed = false;

        if (m_fields->overlay) {
            m_fields->overlay->setVisible(false);
        }

        layer->resumeAndRestart(true);
    }

    void onPlaySavedRoute(CCObject*) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first.");
            return;
        }
        if (layer->m_isPracticeMode) {
            showNotice(
                "Saved clean routes run in normal mode. Turn Practice Mode off first."
            );
            return;
        }

        std::vector<InputEvent> parsed;
        const auto data = loadSavedRoute(layer->m_level);
        if (!parseRoute(data, parsed)) {
            showNotice(
                "No clean route is saved for this level yet. Finish one normal-mode "
                "run, or press LEARN ROUTE and complete the level."
            );
            return;
        }

        g_routeEvents = std::move(parsed);
        g_routeMode = RouteMode::Playing;
        g_playIndex = 0;
        g_lastFrame = 0;
        g_replayRetries = 0;
        g_replayFailed = false;
        g_previewNoDeath = false;

        if (m_fields->overlay) {
            m_fields->overlay->setVisible(false);
        }

        layer->resumeAndRestart(true);
    }

    void refreshLabels() {
        if (m_fields->previewStatus) {
            m_fields->previewStatus->setString(
                g_previewNoDeath ? "ON" : "OFF"
            );
            m_fields->previewStatus->setColor(
                g_previewNoDeath ? ccc3(110, 255, 145) : ccc3(255, 222, 125)
            );
        }

        auto* layer = PlayLayer::get();
        if (m_fields->routeStatus) {
            const auto route = layer && layer->m_level
                ? loadSavedRoute(layer->m_level)
                : std::string{};
            m_fields->routeStatus->setString(
                route.empty() ? "NOT SAVED" : "READY"
            );
            m_fields->routeStatus->setColor(
                route.empty() ? ccc3(255, 157, 130) : ccc3(153, 255, 177)
            );
        }

        if (m_fields->practiceStatus) {
            const int mark = layer && layer->m_level
                ? lastPracticeMark(layer->m_level)
                : -1;
            const auto text = mark >= 0
                ? "LAST PRACTICE DEATH: " + std::to_string(mark) + "%"
                : "NO PRACTICE MARK";
            m_fields->practiceStatus->setString(text.c_str());
        }
    }
};
