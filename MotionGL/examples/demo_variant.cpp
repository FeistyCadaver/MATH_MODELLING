#include <motiongl/linear_animator.hpp>
using namespace mgl;

// variant A: analytic law
anim.run([](double t) { return 0.2 + 0.1*t + 4.0*t*t; }, 20.0);

// variant B: samples from a numerical solver (linear interpolation inside)
std::vector<std::pair<double,double>> samples = {{0,0.2},{0.01,0.201},...};
anim.run(samples);