#include "Manager/Manager.h"
#include <iostream>

int main() {
    using namespace Configure::Manager;
    std::string path;
    std::getline(std::cin, path);
    //auto repo = Cabinet::Fetch(path+"/home", "https://github.com/NEWorldProject/Cabinet");
    auto repo = Cabinet::Open(path+"/home");
    repo.Get("NRT")->Update();
    std::cout << repo.Namespace() << std::endl;
}
