#pragma once

#include <memory>

#include "Enemy.h"

class ActorAPI;

class EnemySuckerFloat : public Enemy {
public:
    EnemySuckerFloat(const ActorInstantiationDetails& initData);
    ~EnemySuckerFloat();
    void tickEvent() override;
    bool perish() override;

private:
    double phase;
    CoordinatePair originPos;

    const static double CIRCLING_RADIUS;
};
