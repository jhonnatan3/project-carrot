#pragma once

#include "../Enemy.h"
#include "../CommonActor.h"
#include "../CarrotQt5.h"
#include <SFML/Graphics.hpp>

class Enemy_Lizard : public Enemy {
    public:
        Enemy_Lizard(CarrotQt5* root, double x = 0.0, double y = 0.0);
        ~Enemy_Lizard();
        void tickEvent() override;
        Hitbox getHitbox();
};
