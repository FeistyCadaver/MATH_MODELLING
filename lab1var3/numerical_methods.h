#pragma once
#include <vector>
#include <utility>

struct Parameters {
    double m;      // масса
    double v0;     // начальная скорость
    double f;      // коэффициент трения
    double x0;     // начальная координата
};

using Samples = std::vector<std::pair<double, double>>;

double force(double t, double x, double v, Parameters p);
double acceleration(double t, double x, double v, Parameters p);
Samples calculate(Parameters p);
void saveGraph(Samples a, Samples b, Samples c, Samples d);