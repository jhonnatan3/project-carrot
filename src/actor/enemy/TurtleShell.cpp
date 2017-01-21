#include "TurtleShell.h"

#include <cmath>
#include "Enemy.h"
#include "../collectible/Collectible.h"
#include "../weapon/Ammo.h"
#include "../weapon/AmmoFreezer.h"
#include "../weapon/AmmoToaster.h"
#include "../../gamestate/ActorAPI.h"
#include "../../gamestate/TileMap.h"
#include "../../struct/Constants.h"


TurtleShell::TurtleShell(const ActorInstantiationDetails& initData, double initSpeedX,
    double initSpeedY, bool fromEventMap) : CommonActor(initData, fromEventMap) {
    loadResources("Enemy/TurtleShell");
    setAnimation(AnimState::IDLE);

    speedX = initSpeedX;
    speedY = initSpeedY;
    friction = api->getGravity() / 100;
    elasticity = 0.5;
    // TODO: test the actual number
    maxHealth = health = 8;

    playSound("ENEMY_TURTLE_SHELL_FLY");
}

TurtleShell::~TurtleShell() {
}

void TurtleShell::tickEvent() {
    speedX = std::max(std::abs(speedX) - friction, 0.0) * (speedX > EPSILON ? 1 : -1);

    double posYBefore = posY;
    CommonActor::tickEvent();
    if (posYBefore - posY > 0.5 && std::abs(speedY) < 1) {
        speedX = std::max(std::abs(speedX) - 10 * friction, 0.0) * (speedX > EPSILON ? 1 : -1);
    }

    auto collisions = api->findCollisionActors(shared_from_this());
    for (auto collider : collisions) {
        if (collider.expired()) {
            continue;
        }

        auto colliderPtr = collider.lock();

        {
            auto specializedPtr = std::dynamic_pointer_cast<Ammo>(colliderPtr);
            if (specializedPtr != nullptr) {
                playSound("ENEMY_TURTLE_SHELL_FLY");
                continue;
            }
        }

        {
            auto specializedPtr = std::dynamic_pointer_cast<Enemy>(colliderPtr);
            if (specializedPtr != nullptr && std::abs(speedX) > 0.5) {
                specializedPtr->decreaseHealth(1);
                speedX = std::max(std::abs(speedX), 2.0) * (speedX > 0 ? -1 : 1);
                continue;
            }
        }

        {
            auto specializedPtr = std::dynamic_pointer_cast<Collectible>(colliderPtr);
            if (specializedPtr != nullptr) {
                specializedPtr->handleCollision(shared_from_this());
                continue;
            }
        }

        {
            auto specializedPtr = std::dynamic_pointer_cast<TurtleShell>(colliderPtr);
            if (specializedPtr != nullptr) {
                if (speedY - specializedPtr->speedY > 1 && speedY > 0) {
                    specializedPtr->decreaseHealth(10);
                    continue;
                }

                if (std::abs(speedX) > std::abs(specializedPtr->speedX)) {
                    // Handle this only in the faster of the two.
                    posX = specializedPtr->posX + (speedX > 0 ? -1 : 1) * (currentGraphicState.dimensions.x + 1);
                    double totalSpeed = std::abs(speedX) + std::abs(specializedPtr->speedX);

                    specializedPtr->speedX = totalSpeed / 2 * (speedX > 0 ? 1 : -1);
                    speedX = totalSpeed / 2 * (speedX > 0 ? -1 : 1);

                    specializedPtr->decreaseHealth(1);
                    playSound("ENEMY_TURTLE_SHELL_IMPACT_SHELL");
                }
                continue;
            }
        }
    }

    auto tiles = api->getGameTiles().lock();
    if (tiles != nullptr) {
        tiles->checkSpecialDestructible(currentHitbox);
        tiles->checkCollapseDestructible(currentHitbox);
        tiles->checkWeaponDestructible(posX, posY, WEAPON_BLASTER);
    }
}

void TurtleShell::updateHitbox() {
    CommonActor::updateHitbox(24u, 16u);
}

void TurtleShell::handleCollision(std::shared_ptr<CommonActor> other) {
    CommonActor::handleCollision(other);

    // TODO: Use actor type specifying function instead when available
    if (std::dynamic_pointer_cast<Ammo>(other) != nullptr &&
        std::dynamic_pointer_cast<AmmoFreezer>(other) == nullptr) {
        if (std::dynamic_pointer_cast<AmmoToaster>(other) != nullptr) {
            health -= maxHealth;
            return;
        }

        double otherSpeed = other->getSpeedX();
        speedX = std::max(4.0, std::abs(otherSpeed)) * (otherSpeed < 0 ? -1 : 1) / 2;
    }
}

void TurtleShell::onHitFloorHook() {
    if (std::abs(speedY) > 1) {
        playSound("ENEMY_TURTLE_SHELL_IMPACT_GROUND");
    }
}

