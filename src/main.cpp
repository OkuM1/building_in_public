
#include "Engine.h"
#include <iostream>

int main() {
    std::cout << "Gentleman, start your engine!" << std::endl;
    Engine engine(640, 480, "Hello Engine");
    engine.Run();
    return 0;
}
