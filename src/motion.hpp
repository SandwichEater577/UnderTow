#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <string>

struct Settings {
    float speed=0.55f, strength=1.0f, density=0.85f, brightness=0.95f, length=220.0f;
    int palette=0, style=0;
    uint32_t seed=0;
    void clamp();
    void load();
    void save() const;
};
struct Bar { float x,y,width,height; float opacity=1; };
struct Palette { const char* name; std::array<float,3> a,b; };
inline constexpr std::array<Palette,6> palettes{{
    {"Sunset",{1.0f,0.56f,0.31f},{0.73f,0.13f,0.57f}},
    {"Ember",{1.0f,0.64f,0.28f},{0.65f,0.12f,0.24f}},
    {"Orchid",{0.86f,0.55f,0.96f},{0.23f,0.20f,0.57f}},
    {"Moss",{0.80f,0.88f,0.46f},{0.12f,0.42f,0.33f}},
    {"Silver",{0.89f,0.91f,0.93f},{0.27f,0.32f,0.38f}},
    {"Ocean",{0.25f,0.70f,0.96f},{0.10f,0.15f,0.38f}}
}};
inline constexpr const char* styles[]={"Drift","Glide","Still"};
class Motion {
    double time_=0,previousTime_=0;
    float speed_=0.55f,strength_=1,previousStrength_=1;
    float length_=220,previousLength_=220,style_=0;
public:
    Motion();
    void tick(const Settings& settings);
    void sample(const Settings& settings,float width,float height,float alpha,std::vector<Bar>& output) const;
};
