#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <deque>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Shared input state
// ---------------------------------------------------------------------------
struct InputState {
    float now = 0.f;                    // accumulated time (seconds)
    std::deque<float> clicks[2];        // timestamps within the last second
    int total[2] = {0, 0};
    bool held[2] = {false, false};

    static InputState& get() {
        static InputState s;
        return s;
    }

    void reset() {
        for (int i = 0; i < 2; i++) {
            clicks[i].clear();
            total[i] = 0;
            held[i] = false;
        }
    }

    void press(int p) {
        held[p] = true;
        total[p]++;
        clicks[p].push_back(now);
    }

    void release(int p) { held[p] = false; }

    int cps(int p) {
        while (!clicks[p].empty() && now - clicks[p].front() > 1.f)
            clicks[p].pop_front();
        return static_cast<int>(clicks[p].size());
    }
};

// ---------------------------------------------------------------------------
// The overlay node
// ---------------------------------------------------------------------------
class InputIndicator : public CCNode {
protected:
    CCScale9Sprite* m_box[2] = {nullptr, nullptr};
    CCLabelBMFont* m_title[2] = {nullptr, nullptr};
    CCLabelBMFont* m_cps[2] = {nullptr, nullptr};
    CCLabelBMFont* m_total[2] = {nullptr, nullptr};

    bool init() {
        if (!CCNode::init()) return false;

        const float w = 44.f, h = 62.f, gap = 8.f;
        const char* names[2] = {"P1", "P2"};

        for (int i = 0; i < 2; i++) {
            float x = i * (w + gap);

            m_box[i] = CCScale9Sprite::create("square02_001.png", {0, 0, 80, 80});
            m_box[i]->setContentSize({w, h});
            m_box[i]->setPosition({x, 0});
            this->addChild(m_box[i]);

            m_title[i] = CCLabelBMFont::create(names[i], "bigFont.fnt");
            m_title[i]->setScale(0.5f);
            m_title[i]->setPosition({x, h / 2 - 12.f});
            this->addChild(m_title[i]);

            m_cps[i] = CCLabelBMFont::create("0", "bigFont.fnt");
            m_cps[i]->setScale(0.4f);
            m_cps[i]->setPosition({x, 2.f});
            this->addChild(m_cps[i]);

            m_total[i] = CCLabelBMFont::create("0", "bigFont.fnt");
            m_total[i]->setScale(0.4f);
            m_total[i]->setPosition({x, -14.f});
            this->addChild(m_total[i]);
        }

        this->scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        auto& st = InputState::get();
        st.now += dt;

        auto mod = Mod::get();
        bool enabled = mod->getSettingValue<bool>("enabled");
        this->setVisible(enabled);
        if (!enabled) return;

        auto win = CCDirector::get()->getWinSize();
        this->setScale(static_cast<float>(mod->getSettingValue<double>("scale")));
        this->setPosition({
            win.width * static_cast<float>(mod->getSettingValue<double>("pos-x")) / 100.f,
            win.height * static_cast<float>(mod->getSettingValue<double>("pos-y")) / 100.f
        });

        auto baseOpacity = static_cast<GLubyte>(mod->getSettingValue<int64_t>("opacity"));

        for (int i = 0; i < 2; i++) {
            m_cps[i]->setString(std::to_string(st.cps(i)).c_str());
            m_total[i]->setString(std::to_string(st.total[i]).c_str());

            // Box lights up while the button is held
            m_box[i]->setColor(st.held[i] ? ccColor3B{255, 255, 255} : ccColor3B{40, 40, 40});
            m_box[i]->setOpacity(st.held[i] ? 255 : baseOpacity);
            m_title[i]->setColor(st.held[i] ? ccColor3B{0, 0, 0} : ccColor3B{255, 255, 255});
        }
    }

public:
    static InputIndicator* create() {
        auto ret = new InputIndicator();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------
class $modify(IndicatorPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        InputState::get().reset();
        if (auto node = InputIndicator::create()) {
            node->setID("input-indicator"_spr);
            this->addChild(node, 1000);
        }
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (Mod::get()->getSettingValue<bool>("reset-on-attempt"))
            InputState::get().reset();
        else {
            InputState::get().held[0] = false;
            InputState::get().held[1] = false;
        }
    }
};

class $modify(IndicatorPlayer, PlayerObject) {
    static int playerIndex(PlayerObject* self) {
        auto pl = PlayLayer::get();
        if (!pl) return -1;
        if (self == pl->m_player1) return 0;
        if (self == pl->m_player2) return 1;
        return -1;
    }

    bool pushButton(PlayerButton btn) {
        if (btn == PlayerButton::Jump) {
            int p = playerIndex(this);
            if (p >= 0) InputState::get().press(p);
        }
        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        if (btn == PlayerButton::Jump) {
            int p = playerIndex(this);
            if (p >= 0) InputState::get().release(p);
        }
        return PlayerObject::releaseButton(btn);
    }
};
