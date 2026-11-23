#include "../includes.hpp"
#include "xd_menu.hpp"

#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCDirector.hpp>

class XDFab : public CCLayer {
public:
    CircleButtonSprite* m_spr = nullptr;
    CCLabelBMFont* m_label = nullptr;
    CCPoint m_startPos;
    bool m_dragging = false;

    static XDFab* create() {
        auto ret = new XDFab();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        CCSize win = CCDirector::get()->getWinSize();

        m_label = CCLabelBMFont::create("xd", "bigFont.fnt");
        m_spr = CircleButtonSprite::create(m_label, CircleBaseColor::Pink, CircleBaseSize::Medium);
        m_spr->setScale(0.8f);

        float x = Mod::get()->getSavedValue<float>("xd_fab_x", 40.f);
        float y = Mod::get()->getSavedValue<float>("xd_fab_y", win.height * 0.6f);
        m_spr->setPosition(ccp(x, y));
        this->addChild(m_spr);

        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-1000);
        this->setTouchEnabled(true);
        this->scheduleUpdate();

        return true;
    }

    void update(float dt) override {
        bool hidden = Mod::get()->getSavedValue<bool>("xd_hide_button");
        m_spr->setVisible(!hidden);

        auto& g = Global::get();
        if (g.state == state::recording) m_label->setColor(ccc3(255, 70, 70));
        else if (g.state == state::playing) m_label->setColor(ccc3(80, 255, 120));
        else m_label->setColor(ccc3(255, 255, 255));
    }

    bool hit(CCTouch* touch) {
        if (!m_spr || !m_spr->isVisible()) return false;
        CCPoint p = this->convertTouchToNodeSpace(touch);
        float radius = m_spr->getContentSize().width * m_spr->getScale() * 0.5f + 10.f;
        return ccpDistance(p, m_spr->getPosition()) <= radius;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (!this->hit(touch)) return false;
        m_startPos = touch->getLocation();
        m_dragging = false;
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        CCPoint now = touch->getLocation();
        if (!m_dragging && ccpDistance(now, m_startPos) > 12.f) m_dragging = true;

        if (m_dragging) {
            CCPoint delta = now - touch->getPreviousLocation();
            CCSize win = CCDirector::get()->getWinSize();
            CCPoint pos = m_spr->getPosition() + delta;
            pos.x = std::max(0.f, std::min(pos.x, win.width));
            pos.y = std::max(0.f, std::min(pos.y, win.height));
            m_spr->setPosition(pos);
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (m_dragging) {
            Mod::get()->setSavedValue<float>("xd_fab_x", m_spr->getPositionX());
            Mod::get()->setSavedValue<float>("xd_fab_y", m_spr->getPositionY());
        }
        else {
            XDMenu::toggle();
        }
        m_dragging = false;
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        m_dragging = false;
    }
};

namespace {
    XDFab* s_fab = nullptr;
}

class $modify(XDFabMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (!s_fab) {
            s_fab = XDFab::create();
            if (s_fab) s_fab->retain();
        }

        return true;
    }
};

class $modify(XDFabDirector, CCDirector) {
    void drawScene() {
        CCScene* scene = this->getRunningScene();

        if (s_fab && scene && s_fab->getParent() != scene) {
            if (s_fab->getParent()) s_fab->removeFromParentAndCleanup(false);
            scene->addChild(s_fab, 100000);
            s_fab->unscheduleUpdate();
            s_fab->scheduleUpdate();
        }

        CCDirector::drawScene();
    }
};
