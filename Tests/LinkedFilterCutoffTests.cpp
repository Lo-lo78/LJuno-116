#include "../Source/LinkedFilterCutoff.h"
#include <cmath>
#include <iostream>
#include <random>
using ljuno::linkedFilter::Scale;
using ljuno::linkedFilter::Cutoffs;
int main()
{
    std::mt19937 rng (135);
    std::uniform_real_distribution<float> unit (0.0f, 1.0f);
    for (double rate : { 22050.0, 44100.0, 48000.0, 96000.0 })
    {
        const Scale scale (rate);
        for (int i = 0; i < 10000; ++i)
        {
            const Cutoffs start { unit (rng) * 2.0f - 1.0f, unit (rng) };
            const float target = unit (rng);
            const auto moved = scale.move (start, target);
            if (moved.lp < -1.0f || moved.lp > 1.0f || moved.hp < 0.0f || moved.hp > 1.0f
                || std::abs ((moved.lp - start.lp) * scale.lpLog
                             - (moved.hp - start.hp) * scale.hpLog) > 0.00001
                || (target > scale.centre (start) && moved.lp + 0.000001f < start.lp)
                || (target < scale.centre (start) && moved.hp - 0.000001f > start.hp))
                return 1;
            const auto back = scale.move (moved, scale.centre (start));
            if (std::abs (back.lp - start.lp) > 0.00001f
                || std::abs (back.hp - start.hp) > 0.00001f)
                { std::cerr << rate << " " << start.lp << " " << start.hp << " " << moved.lp << " " << moved.hp << " " << back.lp << " " << back.hp << "\n"; return 2; }
        }
        const Cutoffs start { 0.50f, 0.25f };
        const auto top = scale.move (start, 1.0f);
        const auto bottom = scale.move (start, 0.0f);
        if (std::abs (top.lp - 1.0f) > 0.00001f || top.hp >= 1.0f
            || std::abs (bottom.hp) > 0.00001f || bottom.lp <= -1.0f)
            return 3;
    }
    std::cout << "TEST135 linked filter: 40000 pairs, range, ratio, direction and reversibility OK\n";
}
