// SPDX-License-Identifier: GPL-3.0-only
#include "package.hpp"
static const char* cancel_phase=nullptr;
static bool progress(const char* phase,uint64_t,uint64_t){return !cancel_phase||strcmp(phase,cancel_phase)!=0;}
int main(int argc,char** argv){try{if(argc!=4&&argc!=5)return 2;if(argc==5)cancel_phase=argv[4];lume_setup::unseal(argv[1],argv[2],argv[3],progress);return 0;}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
