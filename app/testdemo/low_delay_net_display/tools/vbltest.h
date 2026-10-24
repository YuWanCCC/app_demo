
typedef void (*VSYNCCB)();
void test_vblank_thread(int drmFd);
void  setVsyncCallback(VSYNCCB func, int disp_type);