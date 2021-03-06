#include "PulsatingLight.h"

#include <cmath>
#include "../../struct/Constants.h"
#include "../../gamestate/ActorAPI.h"

#define BASE_CYCLE_FRAMES 700

PulsatingLight::PulsatingLight(const ActorInstantiationDetails& initData, quint16 alpha, quint16 speed, quint16 sync)
    : CommonActor(initData), RadialLightSource(100.0, 150.0, sf::Color(0, 0, 0, alpha), initData.coords), speed(4.0 / (speed + 1)) {
    phase = fmod(BASE_CYCLE_FRAMES - (api->getFrame() % BASE_CYCLE_FRAMES + sync * 175) * speed,
                 BASE_CYCLE_FRAMES);
    isCollidable = false;
}

PulsatingLight::~PulsatingLight() {
}

void PulsatingLight::tickEvent() {
    phase = fmod(phase + speed, BASE_CYCLE_FRAMES);
    if (phase < 0) {
        phase -= BASE_CYCLE_FRAMES;
    }

    lightRadiusFar = 120.0 + std::sin(2 * PI * phase / BASE_CYCLE_FRAMES) * 50.0;
    lightRadiusNear = lightRadiusFar - 50;
}
