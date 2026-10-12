#include "motion.hpp"
#include "renderer.hpp"
#include "overlay.hpp"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

using Clock=std::chrono::steady_clock;
struct Options {bool windowed=false,saver=false,benchmark=false,overlay=false,vsync=false;double seconds=0;std::string capture;};
Options parse(int argc,char** argv,Settings& settings) {
    Options o;
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];
        auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing value for "+a);return std::string(argv[i]);};
        if(a=="--windowed")o.windowed=true;
        else if(a=="--screensaver")o.saver=true;
        else if(a=="--benchmark")o.benchmark=true;
        else if(a=="--options")o.overlay=true;
        else if(a=="--vsync")o.vsync=true;
        else if(a=="--seconds")o.seconds=std::stod(value());
        else if(a=="--capture")o.capture=value();
        else if(a=="--speed")settings.speed=std::stof(value());
        else if(a=="--strength")settings.strength=std::stof(value());
        else if(a=="--density")settings.density=std::stof(value());
        else if(a=="--brightness")settings.brightness=std::stof(value());
        else if(a=="--length")settings.length=std::stof(value());
        else if(a=="--seed")settings.seed=uint32_t(std::stoul(value()));
        else if(a=="--palette")settings.palette=std::stoi(value());
        else if(a=="--style")settings.style=std::stoi(value());
        else if(a=="--help") {
            std::cout<<"Undertow - GPU rounded-bar screensaver\n"
                "--windowed       1280x800 preview (default fullscreen)\n"
                "--screensaver    Exit on keyboard, click, or mouse movement\n"
                "--options        Show settings overlay\n"
                "--benchmark      Uncapped rendering (default 120 FPS cap)\n"
                "--vsync          Display-synchronized presentation\n"
                "--seconds N      Exit after N seconds\n--capture PATH   Save final frame as BMP\n"
                "--palette 0..5   Sunset / Ember / Orchid / Moss / Silver / Ocean\n"
                "--style 0..2     Drift / Glide / Still\n"
                "--speed 0.1..3 --strength 0.1..2 --density 0.5..2 --brightness 0.15..1\n"
                "--length 60..300  Bar length in pixels\n"
                "--seed N         Reproducible composition\n"
                "Keys: H options, arrows adjust, P palette, M motion, R reseed,\n"
                "      Space pause, F fullscreen, Esc quit. Changes save automatically.\n";
            std::exit(0);
        } else throw std::runtime_error("Unknown option: "+a);
    }
    if(!std::isfinite(o.seconds)||o.seconds<0)throw std::runtime_error("Invalid duration");
    settings.clamp();return o;
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;
    try {
        Settings settings;settings.load();Options options=parse(argc,argv,settings);
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER))throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        window=SDL_CreateWindow("Undertow",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,800,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(options.windowed?0:SDL_WINDOW_FULLSCREEN_DESKTOP));
        if(!window)throw std::runtime_error(SDL_GetError());
        context=SDL_GL_CreateContext(window);if(!context)throw std::runtime_error(SDL_GetError());
        if(SDL_GL_SetSwapInterval(options.vsync?1:0)<0)std::cerr<<"Swap interval: "<<SDL_GetError()<<'\n';
        SDL_ShowCursor(options.overlay?SDL_ENABLE:SDL_DISABLE);
        SDL_DisplayMode mode{};SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window),&mode);
        std::cout<<"GPU: "<<glGetString(GL_RENDERER)<<" | display "<<mode.refresh_rate<<" Hz | simulation 120 Hz\n";
        {
            Renderer renderer;std::unique_ptr<Overlay> overlay;
            Motion motion;for(int i=0;i<2;++i)motion.tick(settings);
            std::vector<Bar> bars;bars.reserve(8192);
            bool running=true,paused=false,show=options.overlay,fullscreen=!options.windowed,dirty=false,captured=false;
            int selected=0,mouseTravel=0;double accumulator=0,framesTime=0,fps=0;uint64_t frames=0,ticks=0,intervalFrames=0;
            float visibility=show?1:0,density=settings.density;
            const auto start=Clock::now();auto last=start,deadline=start;
            std::vector<double> frameTimes;frameTimes.reserve(10000);
            while(running) {
                auto now=Clock::now();double elapsed=std::chrono::duration<double>(now-start).count();
                double dt=std::chrono::duration<double>(now-last).count();last=now;
                double simulationDt=std::min(dt,0.25);
                SDL_Event event;
                while(SDL_PollEvent(&event)) {
                    if(event.type==SDL_QUIT)running=false;
                    if(event.type==SDL_MOUSEMOTION&&elapsed>1.0)mouseTravel+=std::abs(event.motion.xrel)+std::abs(event.motion.yrel);
                    if(options.saver&&(event.type==SDL_KEYDOWN||event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEWHEEL||mouseTravel>8)){running=false;continue;}
                    if(event.type!=SDL_KEYDOWN)continue;
                    auto key=event.key.keysym.sym;
                    if(key==SDLK_ESCAPE)running=false;
                    if(key==SDLK_h){show=!show;SDL_ShowCursor(show?SDL_ENABLE:SDL_DISABLE);}
                    if(key==SDLK_SPACE)paused=!paused;
                    if(key==SDLK_f){fullscreen=!fullscreen;SDL_SetWindowFullscreen(window,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}
                    if(key==SDLK_r)settings.seed=std::random_device{}();
                    if(key==SDLK_p){settings.palette=(settings.palette+1)%6;dirty=true;}
                    if(key==SDLK_m){settings.style=(settings.style+1)%3;dirty=true;}
                    if(show) {
                        if(key==SDLK_UP)selected=(selected+6)%7;
                        if(key==SDLK_DOWN)selected=(selected+1)%7;
                        int direction=key==SDLK_LEFT?-1:key==SDLK_RIGHT?1:0;
                        if(direction) {
                            if(selected==0)settings.palette=(settings.palette+direction+6)%6;
                            if(selected==1)settings.style=(settings.style+direction+3)%3;
                            if(selected==2)settings.speed+=direction*0.1f;
                            if(selected==3)settings.strength+=direction*0.1f;
                            if(selected==4)settings.density+=direction*0.1f;
                            if(selected==5)settings.brightness+=direction*0.05f;
                            if(selected==6)settings.length+=direction*10.f;
                            settings.clamp();dirty=true;
                        }
                    }
                }
                if(!running)break;
                if(!paused)accumulator+=simulationDt;
                while(accumulator>=1.0/120){motion.tick(settings);accumulator-=1.0/120;++ticks;}
                int w,h;SDL_GL_GetDrawableSize(window,&w,&h);
                if(w<=0||h<=0){SDL_Delay(20);continue;}
                density+=(settings.density-density)*float(1-std::exp(-simulationDt*3));
                Settings rendered=settings;rendered.density=density;
                motion.sample(rendered,float(w),float(h),float(accumulator*120),bars);
                renderer.draw(bars,w,h,rendered,float(simulationDt),float(std::clamp(elapsed/1.8,0.0,1.0)));
                visibility+=(float(show)-visibility)*float(1-std::exp(-simulationDt*12));
                if(visibility>0.002f){if(!overlay)overlay=std::make_unique<Overlay>();overlay->draw(w,h,settings,selected,fps,paused,visibility);}
                bool ending=options.seconds>0&&elapsed>=options.seconds;
                if(ending&&!options.capture.empty()){renderer.capture(options.capture,w,h);captured=true;}
                SDL_GL_SwapWindow(window);
                ++frames;++intervalFrames;framesTime+=dt;
                if(framesTime>=0.5){fps=double(intervalFrames)/framesTime;intervalFrames=0;framesTime=0;}
                if(elapsed>1)frameTimes.push_back(dt*1000);
                if(ending)running=false;
                if(!options.benchmark) {
                    deadline+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/120));
                    if(deadline<Clock::now())deadline=Clock::now();
                    std::this_thread::sleep_until(deadline);
                }
            }
            if(!options.capture.empty()&&!captured){int w,h;SDL_GL_GetDrawableSize(window,&w,&h);renderer.draw(bars,w,h,settings,0,1);renderer.capture(options.capture,w,h);}
            if(dirty)settings.save();
            double total=std::chrono::duration<double>(Clock::now()-start).count();
            std::sort(frameTimes.begin(),frameTimes.end());
            std::cout<<"Frames "<<frames<<" | FPS "<<double(frames)/total<<" | ticks "<<ticks<<" | TPS "<<double(ticks)/total;
            if(!frameTimes.empty())std::cout<<" | frame p50 "<<frameTimes[frameTimes.size()/2]<<" ms | p99 "<<frameTimes[std::min(frameTimes.size()-1,size_t(frameTimes.size()*0.99))]<<" ms";
            std::cout<<'\n';
        }
        SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Undertow: "<<error.what()<<'\n';
        if(context)SDL_GL_DeleteContext(context);
        if(window)SDL_DestroyWindow(window);
        SDL_Quit();return 1;
    }
}
