#include <motiongl/animator.hpp>
using namespace mgl;

// x(t) and y(t) - your solution:
auto fx = [](double t) { return 125.0 * (1.0 - std::exp(-0.1 * t)); };
auto fy = [](double t) { return 1197.5 * (1.0 - std::exp(-0.1 * t)) - 98.1 * t; };

Animator anim = AnimatorBuilder()
    .size(1100, 640).title("Projectile").tickStep(5).autoFit(true)
    .build();

anim.run(fx, fy, 4.2);          // two functions + duration [s]