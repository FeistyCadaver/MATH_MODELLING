#include <motiongl/linear_animator.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace img

// для анимации
struct Params { double m; double v0; double f; };

// внешняя сила для F = 32*v + 4
double force(double t, double x, double v) {
    return 32.0 * v + 4.0; 
}

// Ускорение
double accel(double t, double x, double v, Params p) {
    double g = 9.8;
    double angle = 3.14159265 / 3.0; // 60 градусов
    double F_ext = force(t, x, v);
    double F_grav = p.m * g * std::sin(angle); // Скатывающая сила
    double F_norm = p.m * g * std::cos(angle); // Сила нормальной реакции
    double F_fric_max = p.f * F_norm;          // Макс сила трения

    double F_res = F_ext - F_grav; // Равнодействующая без трения

    if (std::abs(v) < 1e-6) {
        // Тело в покое
        if (std::abs(F_res) <= F_fric_max) return 0.0; // Стоит
        return (F_res - std::copysign(F_fric_max, F_res)) / p.m; // Стартует
    } else {
        // Тело скользит
        return (F_res - std::copysign(F_fric_max, v)) / p.m;
    }
}

// Функция для решения ОДУ методом RK4 и создания массива сэмплов
std::vector<std::pair<double, double>> solve(Params p) {
    std::vector<std::pair<double, double>> samples;
    double t = 0, x = 0.4, v = p.v0; // x0 = 0.4 из таблицы
    const double h = 0.01;
    
    samples.emplace_back(t, x);
    while (t < 20.0) {
        double k1x = v, k1v = accel(t, x, v, p);
        double k2x = v + .5 * h * k1v, k2v = accel(t + .5 * h, x + .5 * h * k1x, v + .5 * h * k1v, p);
        double k3x = v + .5 * h * k2v, k3v = accel(t + .5 * h, x + .5 * h * k2x, v + .5 * h * k2v, p);
        double k4x = v + h * k3v, k4v = accel(t + h, x + h * k3x, v + h * k3v, p);
        
        x += h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
        v += h / 6 * (k1v + 2 * k2v + 2 * k3v + k4v);
        t += h;
        samples.emplace_back(t, x);
    }
    return samples;
}

int main(int argc, char **argv) {
    using namespace mgl;
    
    //аниматор для схемы 3
    LinearAnimator anim = LinearAnimatorBuilder()
                              .size(1100, 640)
                              .title("Variant 3: motion along Ox")
                              .variant(3) 
                              .timeScale(0.1) // Замедляем время, так как модель разгоняется очень быстро
                              .loop(false)
                              .build();

    Params base = {2.0, 0.2, 0.2}; // m=2, v0=0.2, f=0.2

    std::cout << "1. Original experiment" << std::endl;
    anim.run(solve(base));

    base.m *= 2;
    std::cout << "2. Mass x2" << std::endl;
    anim.run(solve(base));
    base.m /= 2;

    base.v0 *= 2;
    std::cout << "3. Velocity x2" << std::endl;
    anim.run(solve(base));
    base.v0 /= 2;

    base.f *= 1.5;
    std::cout << "4. Friction +50%" << std::endl;
    anim.run(solve(base));

    return 0;
}