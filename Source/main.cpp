#include "Manager/Manager.h"
#include <iostream>

int main() {
    using namespace Configure::Manager;
    std::string path;
    std::getline(std::cin, path);
    auto warehouse = Warehouse(path+"/home");
    std::cout << "Repo loaded, updating" << std::endl;
    if (const auto repo = warehouse.GetCabinet("cn.newinfinideas.neworld"); !repo) {
        std::cout << "Fetching Cabinet" << std::endl;
        warehouse.ImportCabinet("https://github.com/NEWorldProject/Cabinet");
    }
    warehouse.GetCabinet("cn.newinfinideas.neworld")->Get("NRT")->Update();
    std::cout << "Nrt Update Complete" << std::endl;
    Warehouse::CheckoutArgs args;
    args.Name = "test";
    args.Modules.push_back({"NRT"});
    warehouse.CreateWorkspace(args);
}
