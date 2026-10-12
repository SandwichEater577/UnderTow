#include "motion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

namespace {
std::filesystem::path configPath() {
    if(const char* p=std::getenv("XDG_CONFIG_HOME")) return std::filesystem::path(p)/"undertow/settings";
    if(const char* p=std::getenv("HOME")) return std::filesystem::path(p)/".config/undertow/settings";
    return {};
}
double hash(int x,int y,uint32_t seed) {
    uint32_t h=uint32_t(x)*374761393u+uint32_t(y)*668265263u+seed*2246822519u;
    h=(h^(h>>13))*1274126177u;
    return double(h^(h>>16))/4294967295.0*2-1;
}
double smooth(double x) { return x*x*x*(x*(x*6-15)+10); }
double noise(double x,double y,uint32_t seed) {
    int ix=int(std::floor(x)),iy=int(std::floor(y));
    double u=smooth(x-ix),v=smooth(y-iy);
    return std::lerp(std::lerp(hash(ix,iy,seed),hash(ix+1,iy,seed),u),
                     std::lerp(hash(ix,iy+1,seed),hash(ix+1,iy+1,seed),u),v);
}

}
void Settings::clamp() {
    auto safe=[](float v,float lo,float hi,float fallback){ return std::isfinite(v)?std::clamp(v,lo,hi):fallback; };
    speed=safe(speed,0.1f,3,0.65f); strength=safe(strength,0.1f,2,1);
    density=safe(density,0.5f,2,1); brightness=safe(brightness,0.15f,1,0.85f);
    length=safe(length,60,300,130);
    palette=std::clamp(palette,0,5); style=std::clamp(style,0,2);
}
void Settings::load() {
    seed=std::random_device{}();
    std::ifstream file(configPath());
    if(file) {
        std::string version; file>>version;
        if(version=="v2") {
            Settings loaded=*this;
            if(file>>loaded.speed>>loaded.strength>>loaded.density>>loaded.brightness>>loaded.palette>>loaded.style>>loaded.length)*this=loaded;
        } else {
            // The old experimental tuning doesn't fit this calmer composition.
            // Preserve it before adopting the new visual defaults.
            std::error_code error;
            std::filesystem::copy_file(configPath(),configPath().string()+".before-simplify",std::filesystem::copy_options::skip_existing,error);
            save();
        }
    }
    clamp();
}
void Settings::save() const {
    auto path=configPath(); if(path.empty()) return;
    std::error_code error; std::filesystem::create_directories(path.parent_path(),error);
    std::ofstream file(path); file<<"v2 "<<speed<<' '<<strength<<' '<<density<<' '<<brightness<<' '<<palette<<' '<<style<<' '<<length<<'\n';
}
Motion::Motion()=default;
void Motion::tick(const Settings& s) {
    previousTime_=time_; previousLength_=length_; previousStrength_=strength_;
    speed_+=(s.speed-speed_)*0.018f;
    strength_+=(s.strength-strength_)*0.018f;
    length_+=(s.length-length_)*0.018f;
    style_+=(float(s.style)-style_)*0.018f;
    time_+=double(speed_)/120.0;
}
void Motion::sample(const Settings& s,float w,float h,float alpha,std::vector<Bar>& out) const {
    out.clear();
    float pitch=76.0f/s.density;
    int columns=std::min(510,int(w/pitch)+1);
    float length=std::lerp(previousLength_,length_,alpha);
    float strength=std::lerp(previousStrength_,strength_,alpha);
    double time=std::lerp(previousTime_,time_,double(alpha));
    // The repeating stream is taller than the display. Bars enter and leave
    // through its edges, so visible counts change without fading or spawning.
    float period=h*0.59f+length*0.5f;
    for(int column=0;column<columns;++column) {
        float x=(column+0.5f)*pitch;
        double offset=(hash(column,7,s.seed)+1)*0.5;
        double spacing=noise(column*0.14,0,s.seed+29)*0.16;
        double drift=time*(17.0-3.0*style_);
        double travel=noise(column*0.12,time*0.05,s.seed+31)*h*0.13*strength;
        double phase=std::fmod(offset*period+spacing*h+drift+travel,double(period));
        if(phase<0)phase+=period;
        float variation=0.78f+0.16f*float(noise(column*0.19,time*0.025,s.seed+43));
        float barLength=length*variation;
        for(int slot=-1;slot<=1;++slot) {
            float y=float(phase)+slot*period;
            if(y+barLength*0.5f<0||y-barLength*0.5f>h)continue;
            out.push_back({x,y,pitch*0.62f,barLength,1});
        }
    }
}
