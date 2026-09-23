#include "app-window.h"

#ifdef __ANDROID__
extern "C" void slint_main()
#else
int main(int argc, char **argv)
#endif

{
    auto ui = OBMainScreen::create();
    ui->run();
}
