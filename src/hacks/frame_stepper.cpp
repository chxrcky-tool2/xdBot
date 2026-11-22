#include "../includes.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCParticleSystem.hpp>  
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>


class $modify(PlayLayer) {

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (Global::get().frameStepper)
            Global::toggleFrameStepper();
    }

};

class $modify(GJBaseGameLayer) {

    void update(float dt) {
        if (!PlayLayer::get()) return GJBaseGameLayer::update(dt);

        if (this->m_player1->m_isDead) {
            if (Global::get().mod->getSavedValue<bool>("macro_instant_respawn"))
                PlayLayer::get()->resetLevel();
        }

        auto& g = Global::get();

        if (g.frameStepper) {
            if (g.stepFrameParticle != 0)
                g.stepFrameParticle--;

            if (Macro::shouldStep()) {
                g.stepFrame = false;

                GJBaseGameLayer::update(1.f / Global::getTPS());

                return;
            }
            else {
                g.safeMode = true;
                return;
            }
        }

        GJBaseGameLayer::update(dt);
    }

};

class $modify(CCParticleSystem) {

    virtual void update(float dt) {
        auto& g = Global::get();
        if (!PlayLayer::get()) return CCParticleSystem::update(dt);

        if (g.frameStepper) {
            if (g.stepFrameParticle != 0) {
                CCParticleSystem::update(dt);
            }
            else
                return;
        }

        CCParticleSystem::update(dt);
    }
};
