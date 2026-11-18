#include <motiongl/tube_animator.hpp>
using namespace mgl;

TubeAnimator anim = TubeAnimatorBuilder()
    .size(1100, 640).variant(4).l(2.0)   // l = |AB|, m
    .build();

auto   seg1 = [](double t) { return t + t*t; };      // s(t) on AB, from A
std::vector<std::pair<double,double>> seg2 = ...;    // x(τ) on BC, from B

anim.run(seg1, seg2);               // function + samples (any combination)