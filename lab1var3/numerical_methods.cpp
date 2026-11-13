#include "numerical_methods.h"
#include <cmath>
#include <fstream>
#include <iostream>

const double g = 9.8;
const double angle = 3.14159265 / 3.0; // 60 градусов 
const double h = 0.01;
const double T = 20.0;

// Внешняя сила: F = 32*v + 4
double force(double t, double x, double v, Parameters p) {
    return 32.0 * v + 4.0; 
}

double acceleration(double t, double x, double v, Parameters p) {
    double F_ext = force(t, x, v, p);
    double F_gravity = p.m * g * std::sin(angle); // Скатывающая сила тяжести
    double N = p.m * g * std::cos(angle);         // Реакция опоры
    double F_friction_max = p.f * N;              // Максимальная сила трения

    // Результирующая сила без трения
    double F_res = F_ext - F_gravity; 

    // Модель трения
    if (std::abs(v) < 1e-6) {
        // Тело в покое
        if (std::abs(F_res) <= F_friction_max) {
            return 0.0; // Стоит на месте
        } else {
            // Начинает движение
            return (F_res - std::copysign(F_friction_max, F_res)) / p.m;
        }
    } else {
        // Тело скользит
        return (F_res - std::copysign(F_friction_max, v)) / p.m;
    }
}

// RK4 для системы уравнений (dx/dt = v, dv/dt = a)
Samples calculate(Parameters p) {
    Samples result;
    double x = p.x0;
    double v = p.v0;
    double t = 0;
    
    result.push_back({t, x});
    
    while (t < T - 1e-9) {
        // k для скорости (v)
        double k1v = acceleration(t, x, v, p);
        double k2v = acceleration(t + h/2, x + h/2 * v, v + h/2 * k1v, p);
        double k3v = acceleration(t + h/2, x + h/2 * (v + h/2 * k1v), v + h/2 * k2v, p);
        double k4v = acceleration(t + h, x + h * (v + h/2 * k2v), v + h * k3v, p);

        // k для координаты (x)
        double k1x = v;
        double k2x = v + h/2 * k1v;
        double k3x = v + h/2 * k2v;
        double k4x = v + h * k3v;

        // Обновление
        double oldV = v;
        x += h / 6.0 * (k1x + 2*k2x + 2*k3x + k4x);
        v += h / 6.0 * (k1v + 2*k2v + 2*k3v + k4v);
        
        // Обработка остановки (если скорость меняет знак из-за трения)
        if (oldV * v < 0 && std::abs(force(t, x, v, p) - p.m * g * std::sin(angle)) <= p.f * p.m * g * std::cos(angle)) {
            v = 0;
        }

        t += h;
        result.push_back({t, x});
    }
    return result;
}

void saveGraph(Samples a, Samples b, Samples c, Samples d) {
    std::ofstream file("result.csv");
    file << "t, original, mass, speed, friction\n";
    for(int i = 0; i < a.size(); ++i){
        file << a[i].first << "," << a[i].second << "," << b[i].second << "," << c[i].second << "," << d[i].second << "\n";
    }
    file.close();
    std::cout << "Result saved to result.csv\n";
}