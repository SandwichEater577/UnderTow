#include "renderer.hpp"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
GLuint program(const char* vs,const char* fs) {
    GLuint p=glCreateProgram();
    for(auto [kind,source]:{std::pair{GL_VERTEX_SHADER,vs},std::pair{GL_FRAGMENT_SHADER,fs}}) {
        GLuint shader=glCreateShader(kind); glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
        GLint ok; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok) { char log[4096]; glGetShaderInfoLog(shader,sizeof(log),nullptr,log); glDeleteShader(shader); glDeleteProgram(p); throw std::runtime_error(log); }
        glAttachShader(p,shader); glDeleteShader(shader);
    }
    glLinkProgram(p); GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok) { char log[4096]; glGetProgramInfoLog(p,sizeof(log),nullptr,log); glDeleteProgram(p); throw std::runtime_error(log); }
    return p;
}
Renderer::Renderer() {
    program_=program(R"(#version 330 core
layout(location=0) in vec4 bar;
layout(location=1) in float opacity;
uniform vec2 resolution;
out vec2 local;
out vec2 size;
out vec2 screen;
out float visibility;
void main(){
    vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    visibility=opacity;
    size=bar.zw; local=corners[gl_VertexID]*(size*0.5+vec2(2));
    vec2 p=bar.xy+local; screen=p/resolution;
    gl_Position=vec4(p.x/resolution.x*2-1,1-p.y/resolution.y*2,0,1);
})",R"(#version 330 core
in vec2 local; in vec2 size; in vec2 screen;
in float visibility;
uniform vec3 colorA,colorB;
uniform float brightness,fade;
out vec4 frag;
void main(){
    float radius=min(20.0,min(size.x,size.y)*0.5);
    vec2 q=abs(local)-size*0.5+radius;
    float distance=length(max(q,0.0))+min(max(q.x,q.y),0.0)-radius;
    float alpha=1-smoothstep(-0.7,0.7,distance);
    float gradient=clamp((local.y/size.y+0.5)*0.76+screen.x*0.24,0,1);
    vec3 color=mix(colorA,colorB,smoothstep(0,1,gradient));
    frag=vec4(color*brightness*fade,alpha*visibility);
})");
    glGenVertexArrays(1,&vao_); glBindVertexArray(vao_);
    glGenBuffers(1,&buffer_); glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glBufferData(GL_ARRAY_BUFFER,512*sizeof(Bar),nullptr,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,sizeof(Bar),nullptr); glVertexAttribDivisor(0,1);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,sizeof(Bar),reinterpret_cast<void*>(4*sizeof(float))); glVertexAttribDivisor(1,1);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
Renderer::~Renderer(){glDeleteBuffers(1,&buffer_);glDeleteVertexArrays(1,&vao_);glDeleteProgram(program_);}
void Renderer::draw(const std::vector<Bar>& bars,int w,int h,const Settings& s,float dt,float fade) {
    glViewport(0,0,w,h); glClearColor(0.018f,0.023f,0.032f,1); glClear(GL_COLOR_BUFFER_BIT);
    float blend=1-std::exp(-dt*2.0f);
    for(int i=0;i<3;++i){colorA_[i]=std::lerp(colorA_[i],palettes[s.palette].a[i],blend);colorB_[i]=std::lerp(colorB_[i],palettes[s.palette].b[i],blend);}
    glUseProgram(program_); glUniform2f(glGetUniformLocation(program_,"resolution"),float(w),float(h));
    glUniform3fv(glGetUniformLocation(program_,"colorA"),1,colorA_.data()); glUniform3fv(glGetUniformLocation(program_,"colorB"),1,colorB_.data());
    glUniform1f(glGetUniformLocation(program_,"brightness"),s.brightness); glUniform1f(glGetUniformLocation(program_,"fade"),fade);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    if(bars.size()>capacity_){capacity_=bars.size()*2;glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(capacity_*sizeof(Bar)),nullptr,GL_STREAM_DRAW);}
    glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(bars.size()*sizeof(Bar)),bars.data());
    glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(bars.size()));
}
void Renderer::capture(const std::string& path,int w,int h) {
    std::vector<unsigned char> pixels(size_t(w)*h*4),flipped(pixels.size());
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    for(int y=0;y<h;++y)std::copy_n(pixels.data()+size_t(h-1-y)*w*4,w*4,flipped.data()+size_t(y)*w*4);
    SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(flipped.data(),w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
    if(!surface)throw std::runtime_error(SDL_GetError());
    int result=SDL_SaveBMP(surface,path.c_str()); SDL_FreeSurface(surface);
    if(result<0)throw std::runtime_error(SDL_GetError());
}
