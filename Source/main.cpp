#include "Manager/Manager.h"
#include <iostream>

int main() {
    using namespace Configure::Manager;
    std::string path;
    std::getline(std::cin, path);
    auto warehouse = Warehouse(path+"/home");
    std::cout << "Repo loaded, updating" << std::endl;
    if (const auto repo = warehouse.GetCabinet("cn.newinfinideas.neworldrt"); !repo) {
        std::cout << "Fetching Cabinet" << std::endl;
        warehouse.ImportCabinet("https://github.com/NEWorldProject/CabRt");
    }
    Warehouse::CheckoutArgs args;
    args.Name = "Test";
    //args.Modules.push_back({"Crt"});
    args.Modules.push_back({"cn.newinfinideas.neworldrt.cfx"});
    warehouse.CreateWorkspace(args);
}
