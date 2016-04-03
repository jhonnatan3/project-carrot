#pragma once

#include <memory>

#include "../../CarrotQt5.h"
#include "Enemy.h"

class Enemy_Lizard : public Enemy {
    public:
        Enemy_Lizard(std::shared_ptr<CarrotQt5> root, double x = 0.0, double y = 0.0);
        ~Enemy_Lizard();
        void tickEvent() override;
        Hitbox getHitbox();
};
