#ifndef LUME_EXTRA_STEREO_STRENGTH_H
#define LUME_EXTRA_STEREO_STRENGTH_H
/* The physical slider remains the master control. 100 percent is identity;
   a closed slider stays closed even with an application gain above 100. */
static inline unsigned lume_stereo_adjust(unsigned physical,unsigned percent) {
 if(physical>256)physical=256;
 if(percent>300)percent=300;
 unsigned adjusted=(physical*percent+50)/100;
 if(physical&&percent&&!adjusted)adjusted=1;
 return adjusted>256?256:adjusted;
}
#endif
