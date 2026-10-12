#include "overlay.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <vector>
Overlay::Overlay() {
    FT_Library library; if(FT_Init_FreeType(&library)) throw std::runtime_error("Cannot initialize FreeType");
    FT_Face face=nullptr;
    for(const char* path:{"/usr/share/fonts/TTF/DejaVuSansMono.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"})
        if(!FT_New_Face(library,path,0,&face))break;
    if(!face){FT_Done_FreeType(library);throw std::runtime_error("Install the DejaVu Sans Mono font for the options overlay");}
    FT_Set_Pixel_Sizes(face,0,18);
    std::vector<unsigned char> atlas(512*256,0); atlas[0]=255;
    for(int c=32;c<127;++c) {
        if(FT_Load_Char(face,c,FT_LOAD_RENDER))continue;
        auto& b=face->glyph->bitmap; int x=(c%16)*32,y=(c/16)*32;
        for(unsigned int row=0;row<b.rows;++row)for(unsigned int col=0;col<b.width;++col)atlas[(y+row)*512+x+col]=b.buffer[row*b.pitch+col];
        glyphs_[c]={float(x)/512,float(y)/256,float(b.width),float(b.rows),float(face->glyph->bitmap_left),float(face->glyph->bitmap_top),float(face->glyph->advance.x)/64};
    }
    FT_Done_Face(face); FT_Done_FreeType(library);
    program_=program(R"(#version 330 core
layout(location=0) in vec2 p;layout(location=1) in vec2 uv;layout(location=2) in vec4 color;
uniform vec2 resolution;out vec2 tex;out vec4 tint;
void main(){gl_Position=vec4(p.x/resolution.x*2-1,1-p.y/resolution.y*2,0,1);tex=uv;tint=color;})",R"(#version 330 core
in vec2 tex;in vec4 tint;uniform sampler2D atlas;out vec4 frag;
void main(){frag=vec4(tint.rgb,tint.a*texture(atlas,tex).r);})");
    glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,512,256,0,GL_RED,GL_UNSIGNED_BYTE,atlas.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    for(int i=0;i<3;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==2?4:2,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(size_t(i)*2*sizeof(float)));}
}
Overlay::~Overlay(){glDeleteTextures(1,&texture_);glDeleteBuffers(1,&buffer_);glDeleteVertexArrays(1,&vao_);glDeleteProgram(program_);}
void Overlay::draw(int w,int h,const Settings& s,int selected,double fps,bool paused,float opacity) {
    std::vector<float> vertices;
    float scale=std::min(float(w)/760,float(h)/620);scale=std::clamp(scale,0.5f,2.0f);
    float left=(w-550*scale)*0.5f,top=(h-527*scale)*0.5f;
    auto quad=[&](float x,float y,float bw,float bh,float u,float v,float uw,float vh,float r,float g,float b,float a) {
        for(auto p:{std::pair{0.f,0.f},std::pair{1.f,0.f},std::pair{1.f,1.f},std::pair{0.f,0.f},std::pair{1.f,1.f},std::pair{0.f,1.f}})
            vertices.insert(vertices.end(),{left+(x+p.first*bw)*scale,top+(y+p.second*bh)*scale,u+p.first*uw,v+p.second*vh,r,g,b,a*opacity});
    };
    auto rect=[&](float x,float y,float bw,float bh,float r,float g,float b,float a){quad(x,y,bw,bh,0.5f/512,0.5f/256,0,0,r,g,b,a);};
    auto text=[&](float x,float y,const std::string& str,float r=0.82f,float g=0.86f,float b=0.90f){
        for(unsigned char c:str){if(c>=128)continue;auto glyph=glyphs_[c];quad(x+glyph.bx,y-glyph.by,glyph.w,glyph.h,glyph.u,glyph.v,glyph.w/512,glyph.h/256,r,g,b,1);x+=glyph.advance;}
    };
    rect(0,0,550,527,0.026f,0.034f,0.046f,0.97f);
    rect(0,0,3,527,0.37f,0.88f,0.87f,0.9f);
    text(30,42,"U N D E R T O W",0.65f,0.94f,0.93f);
    text(30,72,"A field of moving color",0.46f,0.54f,0.62f);
    rect(30,94,490,1,0.3f,0.4f,0.5f,0.35f);
    const char* labels[]={"Palette","Motion","Speed","Motion depth","Density","Brightness","Bar length"};
    char value[64];
    for(int i=0;i<7;++i) {
        float y=130+i*39.f;
        if(i==selected)rect(20,y-24,510,35,0.19f,0.32f,0.36f,0.4f);
        text(32,y,labels[i]);
        if(i==0)std::snprintf(value,sizeof(value),"< %s >",palettes[s.palette].name);
        if(i==1)std::snprintf(value,sizeof(value),"< %s >",styles[s.style]);
        if(i>=2){float v=i==2?s.speed:i==3?s.strength:i==4?s.density:i==5?s.brightness:s.length;std::snprintf(value,sizeof(value),"< %.2f >",double(v));}
        text(292,y,value,0.65f,0.94f,0.93f);
    }
    rect(30,389,490,1,0.3f,0.4f,0.5f,0.35f);
    text(30,421,"Arrows adjust   Space pause   F fullscreen",0.56f,0.64f,0.70f);
    text(30,450,"H hide   R new pattern   Esc quit",0.56f,0.64f,0.70f);
    std::snprintf(value,sizeof(value),"%.1f FPS / 120 Hz simulation%s",fps,paused?" / PAUSED":"");
    text(30,495,value,0.46f,0.54f,0.62f);
    glUseProgram(program_);glUniform2f(glGetUniformLocation(program_,"resolution"),float(w),float(h));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture_);glUniform1i(glGetUniformLocation(program_,"atlas"),0);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data(),GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/8));
}
