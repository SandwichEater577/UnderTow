#pragma once
#include "renderer.hpp"
#include <array>
class Overlay {
    struct Glyph { float u=0,v=0,w=0,h=0,bx=0,by=0,advance=0; };
    std::array<Glyph,128> glyphs_{};
    GLuint program_=0,texture_=0,vao_=0,buffer_=0;
public:
    Overlay();
    ~Overlay();
    void draw(int w,int h,const Settings& settings,int selected,double fps,bool paused,float opacity);
};
