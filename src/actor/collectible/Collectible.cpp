#include <memory>
#include "Collectible.h"
#include "../Player.h"
#include "../enemy/TurtleShell.h"
#include "../../struct/Constants.h"
#include "../weapon/Ammo.h"

Collectible::Collectible(std::shared_ptr<CarrotQt5> root, double x, double y, bool fromEventMap)
    : CommonActor(root, x, y, fromEventMap), untouched(true), scoreValue(100) {
    canBeFrozen = false;
    phase = ((x / 100.0) + (y / 100.0));
    elasticity = 0.6;
    loadResources("Object/Collectible");
}

Collectible::~Collectible() {

}

void Collectible::tickEvent() {
    phase += 0.1;

    if (!untouched) {
        // Do not apply default movement if we haven't been shot yet
        CommonActor::tickEvent();
    }
}

void Collectible::drawUpdate(std::shared_ptr<GameView>& view) {
    double waveOffset = 2.4 * cos((phase * 0.3) * PI);

    // The position of the actor is altered for the draw event.
    // We want to keep the actual position of the actor constant, though.
    posY += waveOffset;
    CommonActor::drawUpdate(view);
    posY -= waveOffset;
}

void Collectible::collect(std::shared_ptr<Player> player) {
    player->addScore(scoreValue);
}

void Collectible::handleCollision(std::shared_ptr<CommonActor> other) {
    // TODO: Use actor type specifying function instead when available
    bool impactable;
    {
        auto ptr = std::dynamic_pointer_cast<Ammo>(other);
        if (ptr != nullptr) {
            impactable = true;
        }
    }
    {
        auto ptr = std::dynamic_pointer_cast<TurtleShell>(other);
        if (ptr != nullptr) {
            impactable = true;
        }
    }

    if (impactable) {
        if (untouched) {
            externalForceX += other->getSpeedX() / 5.0 * (0.9 + (qrand() % 2000) / 10000.0);
            externalForceY += -other->getSpeedY() / 10.0 * (0.9 + (qrand() % 2000) / 10000.0);
        }
        untouched = false;
    }
}

void Collectible::setFacingDirection() {
    if ((qRound(posX + posY) / 32) % 2 == 1) {
        isFacingLeft = true;
    }
}
