#include <Geode/Geode.hpp>
#include <Geode/modify/CCBlockLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    bool g_previewNoclip = false;
    bool g_autoPlayAssist = false;

    bool shouldIgnoreDeath() {
        return g_previewNoclip || g_autoPlayAssist;
    }
}

class $modify(ModGDPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* statusLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        g_previewNoclip = false;
        g_autoPlayAssist = false;

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto status = CCLabelBMFont::create("", "bigFont.fnt");
        if (status) {
            status->setScale(0.32f);
            status->setColor(ccc3(80, 230, 255));
            status->setPosition(CCPoint(winSize.width / 2.f, 20.f));
            status->setZOrder(9999);
            status->setVisible(false);
            this->addChild(status, 9999);
            m_fields->statusLabel = status;
        }

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!m_fields->statusLabel) {
            return;
        }

        if (g_previewNoclip) {
            m_fields->statusLabel->setString("MOD-GD  |  PREVIEW NOCLIP");
            m_fields->statusLabel->setVisible(true);
        } else if (g_autoPlayAssist) {
            m_fields->statusLabel->setString("MOD-GD  |  AUTO-PLAY ASSIST");
            m_fields->statusLabel->setVisible(true);
        } else {
            m_fields->statusLabel->setVisible(false);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (shouldIgnoreDeath()) {
            // Preview/assist mode: ignore collision-based death.
            // This does not generate jump or steering inputs.
            return;
        }

        PlayLayer::destroyPlayer(player, object);
    }
};

// PauseLayer::init is inline in GD 2.2081, so attach at the non-inline
// CCBlockLayer::init entry point and filter for actual PauseLayer instances.
class $modify(ModGDBlockLayer, CCBlockLayer) {
    struct Fields {
        CCLayerColor* overlay = nullptr;
        CCLabelBMFont* previewLabel = nullptr;
        CCLabelBMFont* autoLabel = nullptr;
    };

    bool init() {
        if (!CCBlockLayer::init()) {
            return false;
        }

        if (dynamic_cast<PauseLayer*>(this) == nullptr) {
            return true;
        }

        this->setupModGDPanel();
        return true;
    }

    void setupModGDPanel() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Small launcher shown in the pause screen (the user reaches this screen with Esc).
        auto launcherMenu = CCMenu::create();
        launcherMenu->setPosition(CCPointZero);

        auto launcherSprite = ButtonSprite::create(
            "MGD", 82, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.65f
        );
        auto launcher = CCMenuItemSpriteExtra::create(
            launcherSprite, this, menu_selector(ModGDBlockLayer::onOpenModMenu)
        );
        launcher->setPosition(CCPoint(winSize.width * 0.14f, winSize.height * 0.50f));
        launcher->setID("mod-gd-launcher"_spr);
        launcherMenu->addChild(launcher);
        this->addChild(launcherMenu, 1000);

        // Full-screen dim layer with a centered control panel.
        auto overlay = CCLayerColor::create(
            ccc4(0, 0, 0, 185), winSize.width, winSize.height
        );
        if (!overlay) {
            return;
        }
        overlay->setVisible(false);
        overlay->setID("mod-gd-overlay"_spr);
        this->addChild(overlay, 2000);
        m_fields->overlay = overlay;

        constexpr float panelWidth = 430.f;
        constexpr float panelHeight = 285.f;
        auto panel = CCLayerColor::create(
            ccc4(15, 24, 58, 255), panelWidth, panelHeight
        );
        panel->setPosition(CCPoint(
            (winSize.width - panelWidth) / 2.f,
            (winSize.height - panelHeight) / 2.f
        ));
        overlay->addChild(panel);

        auto title = CCLabelBMFont::create("MOD-GD AUTOPILOT", "bigFont.fnt");
        title->setScale(0.58f);
        title->setPosition(CCPoint(panelWidth / 2.f, 245.f));
        panel->addChild(title);

        auto subtitle = CCLabelBMFont::create(
            "Preview and experimental assist", "bigFont.fnt"
        );
        subtitle->setScale(0.36f);
        subtitle->setColor(ccc3(155, 205, 255));
        subtitle->setPosition(CCPoint(panelWidth / 2.f, 214.f));
        panel->addChild(subtitle);

        auto menu = CCMenu::create();
        menu->setPosition(CCPointZero);
        panel->addChild(menu);

        auto previewText = CCLabelBMFont::create(
            "PREVIEW NOCLIP: OFF", "bigFont.fnt"
        );
        previewText->setScale(0.48f);
        auto previewButton = CCMenuItemSpriteExtra::create(
            previewText, this, menu_selector(ModGDBlockLayer::onTogglePreview)
        );
        previewButton->setPosition(CCPoint(panelWidth / 2.f, 166.f));
        menu->addChild(previewButton);
        m_fields->previewLabel = previewText;

        auto autoText = CCLabelBMFont::create(
            "AUTO-PLAY ASSIST: OFF", "bigFont.fnt"
        );
        autoText->setScale(0.48f);
        auto autoButton = CCMenuItemSpriteExtra::create(
            autoText, this, menu_selector(ModGDBlockLayer::onToggleAutoAssist)
        );
        autoButton->setPosition(CCPoint(panelWidth / 2.f, 112.f));
        menu->addChild(autoButton);
        m_fields->autoLabel = autoText;

        auto note = CCLabelBMFont::create(
            "Assist blocks deaths; it does not tap or steer yet.",
            "bigFont.fnt"
        );
        note->setScale(0.30f);
        note->setColor(ccc3(255, 220, 130));
        note->setPosition(CCPoint(panelWidth / 2.f, 68.f));
        panel->addChild(note);

        auto closeText = CCLabelBMFont::create("CLOSE", "bigFont.fnt");
        closeText->setScale(0.50f);
        auto closeButton = CCMenuItemSpriteExtra::create(
            closeText, this, menu_selector(ModGDBlockLayer::onCloseModMenu)
        );
        closeButton->setPosition(CCPoint(panelWidth / 2.f, 30.f));
        menu->addChild(closeButton);
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
        g_previewNoclip = !g_previewNoclip;
        if (g_previewNoclip) {
            g_autoPlayAssist = false;
        }
        this->refreshLabels();
    }

    void onToggleAutoAssist(CCObject*) {
        g_autoPlayAssist = !g_autoPlayAssist;
        if (g_autoPlayAssist) {
            g_previewNoclip = false;
        }
        this->refreshLabels();
    }

    void refreshLabels() {
        if (m_fields->previewLabel) {
            m_fields->previewLabel->setString(
                g_previewNoclip ? "PREVIEW NOCLIP: ON" : "PREVIEW NOCLIP: OFF"
            );
        }
        if (m_fields->autoLabel) {
            m_fields->autoLabel->setString(
                g_autoPlayAssist ? "AUTO-PLAY ASSIST: ON" : "AUTO-PLAY ASSIST: OFF"
            );
        }
    }
};
