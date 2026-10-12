#include "motion.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
    try {
        for(int style=0;style<3;++style)for(float density:{0.5f,2.f}){
            Settings s;s.seed=42;s.style=style;s.density=density;s.speed=3;s.strength=2;
            Motion a,b;std::vector<Bar> left,right,old;
            bool moved=false;
            bool countsSeen[4]={};
            std::map<float,int> priorCounts;
            bool countsChanged=false;
            for(int tick=0;tick<7200;++tick){
                a.tick(s);b.tick(s);
                if(tick%60)continue;
                a.sample(s,3840,2160,0.5,left);b.sample(s,3840,2160,0.5,right);
                require(left.size()==right.size()&&!left.empty()&&left.size()<=65536,"Bar count");
                std::map<float,int> perColumn;
                for(size_t i=0;i<left.size();++i){
                    auto v=left[i];require(std::isfinite(v.y)&&std::isfinite(v.height),"Nonfinite motion");
                    require(v.width>0&&v.height>0&&v.height<=s.length*1.01f,"Invalid or oversized bar");
                    require(v.y==right[i].y&&v.height==right[i].height,"Seed not deterministic");
                    require(++perColumn[v.x]<=3,"More than three bars in a column");
                    require(v.opacity==1,"Unexpected random fading");
                    if(i<old.size())moved|=std::abs(v.y-old[i].y)>1;
                }
                for(auto [x,count]:perColumn){
                    countsSeen[count]=true;
                    if(priorCounts.contains(x)&&priorCounts[x]!=count)countsChanged=true;
                }
                priorCounts=perColumn;
                old=left;
            }
            require(moved,"Missing vertical movement");
            require(countsSeen[1]&&countsSeen[2],"Missing natural variation in column counts");
            require(countsChanged,"Column counts never evolve");
            require(left.size()<=3*(size_t(3840/(36/density))+2),"Column cap exceeded");
        }
        Settings s;s.speed=std::numeric_limits<float>::infinity();s.density=-5;s.palette=99;s.clamp();
        require(std::isfinite(s.speed)&&s.density==0.5f&&s.palette==5,"Settings validation");
        std::cout<<"PASS: all motion modes, density limits, seeded reproducibility, short bars, maximum three per column, natural edge entry and exit, finite 60-second simulation\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
