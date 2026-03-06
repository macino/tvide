#include "app.h"

int main(int argc, char **argv)
{
    TVIDEApp app(argc, argv);
    app.run();
    app.shutDown();
    return 0;
}
