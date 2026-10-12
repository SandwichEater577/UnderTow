#pragma once
#include "motion.hpp"
#include <SDL_opengl.h>
#include <string>
GLuint program(const char* vertex,const char* fragment);
class Renderer {
    GLuint program_=0,vao_=0,buffer_=0;
    size_t capacity_=512;
    std::array<float,3> colorA_=palettes[0].a,colorB_=palettes[0].b;
public:
    Renderer();
    ~Renderer();
    void draw(const std::vector<Bar>& bars,int w,int h,const Settings& s,float dt,float fade);
    void capture(const std::string& path,int w,int h);
};
