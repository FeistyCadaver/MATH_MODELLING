#include <iostream>
#include "numerical_methods.h"
#include <motiongl/linear_animator.hpp>

int main() { 
    // m = 2.0, v0 = 0.2, f = 0.2, x0 = 0.4
    Parameters p1 = {2.0, 0.2, 0.2, 0.4};       // Исходный
    Parameters p2 = {4.0, 0.2, 0.2, 0.4};       // Масса x2
    Parameters p3 = {2.0, 0.4, 0.2, 0.4};       // Скорость x2
    Parameters p4 = {2.0, 0.2, 0.3, 0.4};       // Трение +50%

    Samples a = calculate(p1);
    Samples b = calculate(p2);
    Samples c = calculate(p3);
    Samples d = calculate(p4);

    saveGraph(a, b, c, d);

    // Схема 3
    LinearAnimator anim = LinearAnimatorBuilder().size(1100, 640).variant(3).build();
    
    std::cout << "Original experiment (Variant 3)" << std::endl;
    anim.run(a);
    std::cout << "Mass x2" << std::endl;
    anim.run(b);
    std::cout << "Velocity x2" << std::endl;
    anim.run(c);
    std::cout << "Friction +50%" << std::endl;
    anim.run(d);
    
    return 0;
}