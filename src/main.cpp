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

    struct LevelAnalysis {
        int objects = 0;
        int hazards = 0;
        int portals = 0;
        int orbs = 0;
        int pads = 0;
        int triggers = 0;
        int slopes = 0;
        bool platformer = false;
        bool dual = false;
        bool cubePilotSupported = false;
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

    // The first AI prototype deliberately targets static, cube-only sections.
    // It scans the full object list before play, then times short jumps against
    // upcoming hazard objects. It does not claim to solve every GD mechanic.
    bool g_aiRequested = false;
    bool g_aiRunning = false;
    bool g_aiFailed = false;
    bool g_aiButtonDown = false;
    bool g_aiAnalysisReady = false;
    bool g_aiAnalysisSupported = false;
    std::uint32_t g_aiReleaseFrame = 0;
    std::uint32_t g_aiNextAllowedFrame = 0;
    int g_aiRetries = 0;
    float g_aiLeadDistance = 132.f;
    std::string g_aiAnalysisKey;
    std::string g_aiAnalysisSummary;

    std::string routeKey(GJGameLevel* level) {
        if (!level) {
            return {};
        }

        const auto levelID = level->m_levelID.value();
        if (levelID > 0) {
            return "route_" + std::to_string(levelID) +
                "_rev_" + std::to_string(level->m_levelRev);
        }

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
        const auto key = routeKey(level);
        return key.empty() ? std::string{} : key + "_practice_mark";
    }

    std::string analysisKey(GJGameLevel* level) {
        const auto key = routeKey(level);
        return key.empty() ? std::string{} : key + "_analysis";
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

    bool isModePortal(GameObjectType type) {
        switch (type) {
            case GameObjectType::InverseGravityPortal:
            case GameObjectType::NormalGravityPortal:
            case GameObjectType::ShipPortal:
            case GameObjectType::CubePortal:
            case GameObjectType::InverseMirrorPortal:
            case GameObjectType::NormalMirrorPortal:
            case GameObjectType::BallPortal:
            case GameObjectType::RegularSizePortal:
            case GameObjectType::MiniSizePortal:
            case GameObjectType::UfoPortal:
            case GameObjectType::DualPortal:
            case GameObjectType::SoloPortal:
            case GameObjectType::WavePortal:
            case GameObjectType::RobotPortal:
            case GameObjectType::TeleportPortal:
            case GameObjectType::SpiderPortal:
            case GameObjectType::SwingPortal:
            case GameObjectType::GravityTogglePortal:
                return true;
            default:
                return false;
        }
    }

    bool isOrb(GameObjectType type) {
        switch (type) {
            case GameObjectType::YellowJumpRing:
            case GameObjectType::PinkJumpRing:
            case GameObjectType::GravityRing:
            case GameObjectType::GreenRing:
            case GameObjectType::DropRing:
            case GameObjectType::RedJumpRing:
            case GameObjectType::CustomRing:
            case GameObjectType::DashRing:
            case GameObjectType::GravityDashRing:
            case GameObjectType::SpiderOrb:
            case GameObjectType::TeleportOrb:
                return true;
            default:
                return false;
        }
    }

    bool isPad(GameObjectType type) {
        switch (type) {
            case GameObjectType::YellowJumpPad:
            case GameObjectType::PinkJumpPad:
            case GameObjectType::GravityPad:
            case GameObjectType::RedJumpPad:
            case GameObjectType::SpiderPad:
                return true;
            default:
                return false;
        }
    }

    bool isHazard(GameObject* object) {
        if (!object) {
            return false;
        }

        const auto type = object->getType();
        return type == GameObjectType::Hazard ||
            type == GameObjectType::AnimatedHazard ||
            (type == GameObjectType::Slope && object->m_slopeIsHazard);
    }

    LevelAnalysis analyzeLevelObjects(PlayLayer* layer) {
        LevelAnalysis result;
        if (!layer || !layer->m_level || !layer->m_objects) {
            return result;
        }

        result.platformer = layer->m_level->isPlatformer();
        result.dual = layer->m_level->m_twoPlayerMode;

        const int count = layer->m_objects->count();
        for (int i = 0; i < count; ++i) {
            auto* object = static_cast<GameObject*>(layer->m_objects->objectAtIndex(i));
            if (!object) {
                continue;
            }

            ++result.objects;
            const auto type = object->getType();

            if (isHazard(object)) {
                ++result.hazards;
            }
            if (isModePortal(type)) {
                ++result.portals;
            }
            if (isOrb(type)) {
                ++result.orbs;
            }
            if (isPad(type)) {
                ++result.pads;
            }
            if (object->m_isTrigger) {
                ++result.triggers;
            }
            if (type == GameObjectType::Slope) {
                ++result.slopes;
            }
        }

        // This version can only make a useful prediction for a non-platformer,
        // single-player, cube-only route with no orbs, pads or form portals.
        result.cubePilotSupported =
            !result.platformer &&
            !result.dual &&
            result.portals == 0 &&
            result.orbs == 0 &&
            result.pads == 0;
        return result;
    }

    std::string makeAnalysisSummary(const LevelAnalysis& a) {
        std::ostringstream out;
        out << "SCAN: " << a.objects
            << " OBJECTS  |  " << a.hazards << " HAZARDS"
            << "  |  " << a.portals << " PORTALS"
            << "  |  " << a.orbs << " ORBS"
            << "  |  " << a.pads << " PADS";

        if (a.platformer) {
            out << "  |  PLATFORMER";
        }
        if (a.dual) {
            out << "  |  DUAL";
        }
        if (a.triggers > 0) {
            out << "  |  " << a.triggers << " TRIGGERS";
        }
        out << (a.cubePilotSupported
            ? "\nAI STATUS: CUBE PILOT AVAILABLE"
            : "\nAI STATUS: SPECIAL GAMEPLAY FOUND");
        return out.str();
    }

    bool currentPlayerIsCube(PlayerObject* player) {
        return player &&
            !player->m_isShip &&
            !player->m_isBird &&
            !player->m_isBall &&
            !player->m_isDart &&
            !player->m_isRobot &&
            !player->m_isSpider &&
            !player->m_isSwing;
    }

    void showNotice(const char* message) {
        FLAlertLayer::create("Mod-GD", message, "OK")->show();
    }
}

// Capture manual, AI-generated and replay inputs at Geometry Dash's own input
// handler so each event follows the game's normal press/release processing.
class $modify(ModGDBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto* layer = PlayLayer::get();
        const bool isCurrentPlayLayer =
            layer && static_cast<GJBaseGameLayer*>(layer) == this;

        if (isCurrentPlayLayer && !g_injecting) {
            if (g_routeMode == RouteMode::Playing) {
                return;
            }

            const bool shouldRecordRoute =
                g_routeMode == RouteMode::Recording &&
                !g_previewNoDeath &&
                !layer->m_isPracticeMode &&
                !layer->m_hasCompletedLevel;

            const bool shouldRecordAI =
                g_aiRunning &&
                !g_previewNoDeath &&
                !layer->m_isPracticeMode &&
                !layer->m_hasCompletedLevel;

            if ((shouldRecordRoute || shouldRecordAI) && button == 1) {
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
};

class $modify(ModGDPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* statusLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        g_previewNoDeath = false;
        g_aiButtonDown = false;
        if (g_routeMode != RouteMode::Playing &&
            g_routeMode != RouteMode::Recording &&
            !g_aiRequested) {
            g_routeMode = RouteMode::Idle;
            g_routeEvents.clear();
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto status = CCLabelBMFont::create("", "bigFont.fnt");
        if (status) {
            status->setScale(0.36f);
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

        if (g_aiRequested) {
            g_aiRequested = false;
            g_aiRunning = true;
            g_aiFailed = false;
            g_aiButtonDown = false;
            g_aiRetries = 0;
            g_aiLeadDistance = 132.f;
            g_aiReleaseFrame = 0;
            g_aiNextAllowedFrame = 0;
            g_routeMode = RouteMode::Idle;
            g_routeEvents.clear();
        } else if (g_routeMode != RouteMode::Playing &&
                   g_routeMode != RouteMode::Recording) {
            g_routeEvents.clear();
            g_routeMode = RouteMode::Idle;
            g_playIndex = 0;
            g_lastFrame = 0;
            g_replayRetries = 0;
            g_replayFailed = false;
        }
    }

    void updateAICubePilot() {
        if (!g_aiRunning || !m_player1 || m_hasCompletedLevel) {
            return;
        }

        const auto frame = m_gameState.m_currentProgress;

        if (m_player1->m_isDead) {
            return;
        }

        if (!currentPlayerIsCube(m_player1)) {
            if (g_aiButtonDown) {
                this->handleButton(false, 1, true);
                g_aiButtonDown = false;
            }
            g_aiRunning = false;
            g_aiFailed = false;
            showNotice(
                "AI stopped: this prototype currently handles cube-only gameplay. "
                "The level analysis found a game mode it cannot steer yet."
            );
            return;
        }

        if (g_aiButtonDown && frame >= g_aiReleaseFrame) {
            this->handleButton(false, 1, true);
            g_aiButtonDown = false;
        }

        if (g_aiButtonDown ||
            !m_player1->m_isOnGround ||
            frame < g_aiNextAllowedFrame ||
            !m_objects) {
            return;
        }

        const float playerX = m_player1->getPositionX();
        float nearestDistance = g_aiLeadDistance + 1.f;
        GameObject* nearestHazard = nullptr;

        for (int i = 0; i < m_objects->count(); ++i) {
            auto* object = static_cast<GameObject*>(m_objects->objectAtIndex(i));
            if (!isHazard(object)) {
                continue;
            }

            const float distance = object->getPositionX() - playerX;
            if (distance > 0.f && distance < nearestDistance) {
                nearestDistance = distance;
                nearestHazard = object;
            }
        }

        if (nearestHazard && nearestDistance <= g_aiLeadDistance) {
            this->handleButton(true, 1, true);
            g_aiButtonDown = true;
            g_aiReleaseFrame = frame + 3;
            g_aiNextAllowedFrame = frame + 17;
        }
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (g_aiRunning) {
            updateAICubePilot();
        }

        auto* label = m_fields->statusLabel;
        if (!label) {
            return;
        }

        if (g_previewNoDeath) {
            label->setString("MOD-GD  |  PREVIEW (NO DEATH)");
            label->setVisible(true);
        } else if (g_aiRunning) {
            const auto text = "MOD-GD  |  AI CUBE PILOT  |  TRY " +
                std::to_string(g_aiRetries + 1);
            label->setString(text.c_str());
            label->setVisible(true);
        } else if (g_routeMode == RouteMode::Playing) {
            const auto text = "MOD-GD  |  SAVED ROUTE  " +
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
        if (g_aiRunning) {
            g_aiFailed = true;
            if (g_aiButtonDown) {
                this->handleButton(false, 1, true);
                g_aiButtonDown = false;
            }
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        if (g_routeMode == RouteMode::Recording && !m_isPracticeMode) {
            g_routeEvents.clear();
        }

        if (g_routeMode == RouteMode::Playing) {
            g_replayFailed = false;
            g_playIndex = 0;
            g_lastFrame = 0;
        }

        if (g_aiRunning) {
            g_aiFailed = false;
            g_aiButtonDown = false;
            g_aiReleaseFrame = 0;
            g_aiNextAllowedFrame = 0;
            g_routeEvents.clear();
        }
    }

    void restartAIFromStart() {
        if (g_aiRunning && !m_hasCompletedLevel) {
            this->resumeAndRestart(true);
        }
    }

    void showRetryLayer() {
        if (g_aiRunning && g_aiFailed) {
            if (g_aiRetries < 8) {
                ++g_aiRetries;
                g_aiFailed = false;

                // Small bounded timing variation between attempts. The bot
                // does not claim to learn every level or to solve special modes.
                if (g_aiRetries % 2 == 1) {
                    g_aiLeadDistance = std::min(172.f, g_aiLeadDistance + 12.f);
                } else {
                    g_aiLeadDistance = std::max(92.f, g_aiLeadDistance - 18.f);
                }

                this->runAction(CCSequence::create(
                    CCDelayTime::create(0.35f),
                    CCCallFunc::create(
                        this,
                        callfunc_selector(ModGDPlayLayer::restartAIFromStart)
                    ),
                    nullptr
                ));
                return;
            }

            g_aiRunning = false;
            g_aiFailed = false;
            g_aiButtonDown = false;
        }

        if (g_routeMode == RouteMode::Playing && g_replayFailed) {
            if (g_replayRetries < 3) {
                ++g_replayRetries;
                g_replayFailed = false;

                this->runAction(CCSequence::create(
                    CCDelayTime::create(0.35f),
                    CCCallFunc::create(
                        this,
                        callfunc_selector(ModGDPlayLayer::restartSavedRouteNow)
                    ),
                    nullptr
                ));
                return;
            }

            g_routeMode = RouteMode::Idle;
            g_replayFailed = false;
            g_playIndex = 0;
        }

        PlayLayer::showRetryLayer();
    }

    void restartSavedRouteNow() {
        if (g_routeMode == RouteMode::Playing && !m_hasCompletedLevel) {
            this->resumeAndRestart(true);
        }
    }

    void levelComplete() {
        if (((g_routeMode == RouteMode::Recording) ||
             (g_aiRunning)) &&
            !g_previewNoDeath &&
            !m_isPracticeMode) {
            if (saveCurrentRoute(this)) {
                log::info(
                    "[Mod-GD] Saved a successful input route with {} events",
                    g_routeEvents.size()
                );
            }
        }

        if (g_routeMode == RouteMode::Playing) {
            g_routeMode = RouteMode::Idle;
        }
        if (g_aiRunning) {
            g_aiRunning = false;
            g_aiFailed = false;
            g_aiButtonDown = false;
        }

        PlayLayer::levelComplete();
    }

    void onQuit() {
        g_aiRequested = false;
        g_aiRunning = false;
        g_aiFailed = false;
        g_aiButtonDown = false;
        g_routeMode = RouteMode::Idle;
        PlayLayer::onQuit();
    }
};

// PauseLayer::customSetup is the correct extension point for the in-game launcher.
// The launcher intentionally uses a vanilla ButtonSprite instead of a custom
// texture frame, so a missing sprite can never turn into a purple placeholder.
class $modify(ModGDPauseLayer, PauseLayer) {
    struct Fields {
        CCLayerColor* overlay = nullptr;
        CCLabelBMFont* analysisStatus = nullptr;
        CCLabelBMFont* previewStatus = nullptr;
        CCLabelBMFont* routeStatus = nullptr;
        CCLabelBMFont* practiceStatus = nullptr;
    };

    void customSetup() {
        PauseLayer::customSetup();

        const auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto launcherMenu = CCMenu::create();
        launcherMenu->setPosition(CCPointZero);
        launcherMenu->setID("mod-gd-launcher-menu"_spr);

        auto icon = ButtonSprite::create(
            "MGD", 112, true, "goldFont.fnt",
            "GJ_button_01.png", 42.f, 0.95f
        );
        auto launcher = CCMenuItemSpriteExtra::create(
            icon, this, menu_selector(ModGDPauseLayer::onOpenModMenu)
        );
        launcher->setPosition(CCPoint(88.f, winSize.height * 0.5f));
        launcher->setSizeMult(1.65f);
        launcher->setID("mod-gd-launcher"_spr);
        launcherMenu->addChild(launcher);
        this->addChild(launcherMenu, 1000);

        auto overlay = CCLayerColor::create(
            ccc4(0, 0, 0, 185), winSize.width, winSize.height
        );
        if (!overlay) {
            log::error("[Mod-GD] Could not create the menu overlay");
            return;
        }

        overlay->setVisible(false);
        overlay->setID("mod-gd-overlay"_spr);
        this->addChild(overlay, 2000);
        m_fields->overlay = overlay;

        constexpr float panelWidth = 560.f;
        constexpr float panelHeight = 440.f;
        const CCPoint panelPos(
            (winSize.width - panelWidth) / 2.f,
            (winSize.height - panelHeight) / 2.f
        );

        auto frame = CCLayerColor::create(
            ccc4(65, 208, 255, 255),
            panelWidth + 6.f,
            panelHeight + 6.f
        );
        frame->setPosition(CCPoint(panelPos.x - 3.f, panelPos.y - 3.f));
        overlay->addChild(frame);

        auto panel = CCLayerColor::create(
            ccc4(27, 43, 91, 252),
            panelWidth,
            panelHeight
        );
        panel->setPosition(panelPos);
        overlay->addChild(panel);

        auto topBar = CCLayerColor::create(
            ccc4(255, 210, 80, 255),
            panelWidth - 8.f,
            5.f
        );
        topBar->setPosition(CCPoint(4.f, panelHeight - 8.f));
        panel->addChild(topBar);

        auto title = CCLabelBMFont::create("MOD-GD AUTOPILOT", "goldFont.fnt");
        title->setScale(0.78f);
        title->setPosition(CCPoint(panelWidth / 2.f, 402.f));
        panel->addChild(title);

        auto subtitle = CCLabelBMFont::create(
            "SCAN LEVEL  /  AI PLAY  /  SAVE", "bigFont.fnt"
        );
        subtitle->setScale(0.42f);
        subtitle->setColor(ccc3(170, 230, 255));
        subtitle->setPosition(CCPoint(panelWidth / 2.f, 374.f));
        panel->addChild(subtitle);

        auto analysisStatus = CCLabelBMFont::create(
            "LEVEL NOT SCANNED", "bigFont.fnt"
        );
        analysisStatus->setScale(0.34f);
        analysisStatus->setColor(ccc3(210, 235, 255));
        analysisStatus->setPosition(CCPoint(panelWidth / 2.f, 338.f));
        panel->addChild(analysisStatus);
        m_fields->analysisStatus = analysisStatus;

        auto menu = CCMenu::create();
        menu->setPosition(CCPointZero);
        panel->addChild(menu);

        auto makeButton = [&](const char* text, const char* frameName,
                              float x, float y, cocos2d::SEL_MenuHandler selector) {
            auto sprite = ButtonSprite::create(
                text, 220, true, "goldFont.fnt", frameName, 38.f, 0.72f
            );
            auto button = CCMenuItemSpriteExtra::create(sprite, this, selector);
            button->setPosition(CCPoint(x, y));
            menu->addChild(button);
        };

        makeButton(
            "ANALYZE LEVEL", "GJ_button_01.png",
            145.f, 265.f, menu_selector(ModGDPauseLayer::onAnalyzeLevel)
        );
        makeButton(
            "AI AUTO-PLAY", "GJ_button_02.png",
            415.f, 265.f, menu_selector(ModGDPauseLayer::onAIPlay)
        );
        makeButton(
            "LEARN ROUTE", "GJ_button_03.png",
            145.f, 195.f, menu_selector(ModGDPauseLayer::onLearnRoute)
        );
        makeButton(
            "PLAY SAVED", "GJ_button_04.png",
            415.f, 195.f, menu_selector(ModGDPauseLayer::onPlaySavedRoute)
        );
        makeButton(
            "SAVE LEVEL", "GJ_button_05.png",
            145.f, 125.f, menu_selector(ModGDPauseLayer::onSaveLevel)
        );
        makeButton(
            "EXIT LEVEL", "GJ_button_06.png",
            415.f, 125.f, menu_selector(ModGDPauseLayer::onExitLevel)
        );

        auto previewStatus = CCLabelBMFont::create("PREVIEW: OFF", "bigFont.fnt");
        previewStatus->setScale(0.32f);
        previewStatus->setColor(ccc3(255, 222, 125));
        previewStatus->setPosition(CCPoint(panelWidth / 2.f, 80.f));
        panel->addChild(previewStatus);
        m_fields->previewStatus = previewStatus;

        auto routeStatus = CCLabelBMFont::create("ROUTE: NOT SAVED", "bigFont.fnt");
        routeStatus->setScale(0.32f);
        routeStatus->setColor(ccc3(255, 185, 150));
        routeStatus->setPosition(CCPoint(panelWidth / 2.f, 58.f));
        panel->addChild(routeStatus);
        m_fields->routeStatus = routeStatus;

        auto practiceStatus = CCLabelBMFont::create("PRACTICE MARK: NONE", "bigFont.fnt");
        practiceStatus->setScale(0.30f);
        practiceStatus->setColor(ccc3(255, 215, 120));
        practiceStatus->setPosition(CCPoint(panelWidth / 2.f, 38.f));
        panel->addChild(practiceStatus);
        m_fields->practiceStatus = practiceStatus;

        auto closeSprite = ButtonSprite::create(
            "CLOSE", 100, true, "goldFont.fnt",
            "GJ_button_04.png", 30.f, 0.72f
        );
        auto closeButton = CCMenuItemSpriteExtra::create(
            closeSprite, this, menu_selector(ModGDPauseLayer::onCloseModMenu)
        );
        closeButton->setPosition(CCPoint(panelWidth / 2.f, 14.f));
        menu->addChild(closeButton);

        log::info("[Mod-GD] MGD launcher and AI analysis menu created");
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

    void onAnalyzeLevel(CCObject*) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first, then scan it.");
            return;
        }

        const auto result = analyzeLevelObjects(layer);
        g_aiAnalysisReady = true;
        g_aiAnalysisSupported = result.cubePilotSupported;
        g_aiAnalysisKey = analysisKey(layer->m_level);
        g_aiAnalysisSummary = makeAnalysisSummary(result);
        if (!g_aiAnalysisKey.empty()) {
            Mod::get()->setSavedValue<std::string>(
                g_aiAnalysisKey,
                g_aiAnalysisSummary
            );
        }

        this->refreshLabels();

        if (result.cubePilotSupported) {
            showNotice(
                "Level scan complete. The current AI prototype can attempt "
                "static cube-only sections. Triggers and moving objects may still "
                "need more advanced prediction."
            );
        } else {
            showNotice(
                "Level scan complete. This level contains portals, orbs, pads, "
                "dual mode or platformer gameplay. The cube-only AI will not start "
                "because it cannot steer those sections yet."
            );
        }
    }

    void dismissPauseAndRestart(PlayLayer* layer, CCObject* sender) {
        if (!layer) {
            return;
        }

        if (m_fields->overlay) {
            m_fields->overlay->setVisible(false);
        }

        // Use the vanilla pause handler first so the pause menu is removed.
        // Then restart from the beginning after it has been dismissed.
        this->onResume(sender);
        layer->resumeAndRestart(true);
    }

    void onAIPlay(CCObject* sender) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first.");
            return;
        }

        const auto currentKey = analysisKey(layer->m_level);
        if (!g_aiAnalysisReady || g_aiAnalysisKey != currentKey) {
            showNotice("Press ANALYZE LEVEL first so the AI can inspect this level.");
            return;
        }
        if (!g_aiAnalysisSupported) {
            showNotice(
                "AI cannot start on this level yet. This first prototype supports "
                "only static cube-only gameplay without portals, orbs, pads, dual "
                "mode or platformer sections."
            );
            return;
        }
        if (layer->m_isPracticeMode) {
            showNotice("Turn Practice Mode off before starting AI Auto-Play.");
            return;
        }

        g_previewNoDeath = false;
        g_aiRequested = true;
        g_aiRunning = false;
        g_aiFailed = false;
        g_aiButtonDown = false;
        g_aiRetries = 0;
        g_aiLeadDistance = 132.f;
        g_routeMode = RouteMode::Idle;
        g_routeEvents.clear();

        this->dismissPauseAndRestart(layer, sender);
    }

    void onTogglePreview(CCObject*) {
        g_previewNoDeath = !g_previewNoDeath;

        if (g_previewNoDeath) {
            g_aiRequested = false;
            g_aiRunning = false;
            g_aiButtonDown = false;
            g_routeMode = RouteMode::Idle;
            g_routeEvents.clear();
        }

        this->refreshLabels();
    }

    void onLearnRoute(CCObject* sender) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first.");
            return;
        }
        if (layer->m_isPracticeMode) {
            showNotice(
                "Learn Route records a clean normal-mode run. Practice deaths "
                "are still saved as percentage markers."
            );
            return;
        }

        g_previewNoDeath = false;
        g_aiRequested = false;
        g_aiRunning = false;
        g_aiButtonDown = false;
        g_routeEvents.clear();
        g_routeMode = RouteMode::Recording;
        g_playIndex = 0;
        g_lastFrame = 0;
        g_replayRetries = 0;
        g_replayFailed = false;

        this->dismissPauseAndRestart(layer, sender);
    }

    void onPlaySavedRoute(CCObject* sender) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first.");
            return;
        }
        if (layer->m_isPracticeMode) {
            showNotice("Saved clean routes run in normal mode. Turn Practice Mode off first.");
            return;
        }

        std::vector<InputEvent> parsed;
        const auto data = loadSavedRoute(layer->m_level);
        if (!parseRoute(data, parsed)) {
            showNotice(
                "No route is saved for this level yet. Complete Learn Route once, "
                "or let AI finish a supported level."
            );
            return;
        }

        g_aiRequested = false;
        g_aiRunning = false;
        g_aiButtonDown = false;
        g_routeEvents = std::move(parsed);
        g_routeMode = RouteMode::Playing;
        g_playIndex = 0;
        g_lastFrame = 0;
        g_replayRetries = 0;
        g_replayFailed = false;
        g_previewNoDeath = false;

        this->dismissPauseAndRestart(layer, sender);
    }

    void onSaveLevel(CCObject*) {
        auto* layer = PlayLayer::get();
        if (!layer || !layer->m_level) {
            showNotice("Open a level first.");
            return;
        }

        bool savedRoute = saveCurrentRoute(layer);
        const auto key = analysisKey(layer->m_level);
        if (!key.empty() && g_aiAnalysisReady &&
            g_aiAnalysisKey == key && !g_aiAnalysisSummary.empty()) {
            Mod::get()->setSavedValue<std::string>(key, g_aiAnalysisSummary);
        }

        if (savedRoute || g_aiAnalysisReady) {
            showNotice(
                "Mod-GD saved this level's analysis and any recorded route locally. "
                "This does not edit or upload the original Geometry Dash level."
            );
        } else {
            showNotice(
                "Nothing to save yet. Analyze the level or record a route first."
            );
        }

        this->refreshLabels();
    }

    void onExitLevel(CCObject* sender) {
        if (m_fields->overlay) {
            m_fields->overlay->setVisible(false);
        }
        this->onQuit(sender);
    }

    void refreshLabels() {
        auto* layer = PlayLayer::get();

        if (m_fields->analysisStatus) {
            if (g_aiAnalysisReady && layer && layer->m_level &&
                g_aiAnalysisKey == analysisKey(layer->m_level)) {
                m_fields->analysisStatus->setString(g_aiAnalysisSummary.c_str());
                m_fields->analysisStatus->setColor(
                    g_aiAnalysisSupported
                        ? ccc3(150, 255, 185)
                        : ccc3(255, 190, 145)
                );
            } else {
                m_fields->analysisStatus->setString("LEVEL NOT SCANNED");
                m_fields->analysisStatus->setColor(ccc3(210, 235, 255));
            }
        }

        if (m_fields->previewStatus) {
            m_fields->previewStatus->setString(
                g_previewNoDeath ? "PREVIEW: ON" : "PREVIEW: OFF"
            );
            m_fields->previewStatus->setColor(
                g_previewNoDeath ? ccc3(110, 255, 145) : ccc3(255, 222, 125)
            );
        }

        if (m_fields->routeStatus) {
            const auto route = layer && layer->m_level
                ? loadSavedRoute(layer->m_level)
                : std::string{};
            m_fields->routeStatus->setString(
                route.empty() ? "ROUTE: NOT SAVED" : "ROUTE: READY"
            );
            m_fields->routeStatus->setColor(
                route.empty() ? ccc3(255, 185, 150) : ccc3(153, 255, 177)
            );
        }

        if (m_fields->practiceStatus) {
            const int mark = layer && layer->m_level
                ? lastPracticeMark(layer->m_level)
                : -1;
            const auto text = mark >= 0
                ? "PRACTICE MARK: " + std::to_string(mark) + "%"
                : "PRACTICE MARK: NONE";
            m_fields->practiceStatus->setString(text.c_str());
        }
    }
};
