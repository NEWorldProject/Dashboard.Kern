#include <utility>

#include "InterOp.h"
#include "Json/Json.h"
#include "Utils/Exception.h"

using namespace Configure::Manager::InterOp;

namespace Configure::Manager {
    namespace {
        class Checkout {
        public:
            explicit Checkout(
                    Warehouse& warehouse,
                    std::filesystem::path home,
                    const Warehouse::CheckoutArgs& args
            ) noexcept
                    :mrHouse(warehouse), mrArgs(args), mHome(std::move(home)) { }

            Workspace Run() {
                ValidateName(mrArgs.Name);
                IndexModules();
                ResolveRequests();
                PathLink();
                WriteOut();
                return Workspace(mHome / mrArgs.Name);
            }
        private:
            void IndexModules() {
                mrHouse.EnumerateCabinets([this](Cabinet& cab) {
                    cab.Enumerate([this, prefix = cab.Namespace()+'.'](Module& mod) {
                        mIndex.insert_or_assign(prefix+mod.Id(), &mod);
                    });
                });
            }

            void ResolveRequests() {
                const auto ws = mHome/mrArgs.Name;
                const auto wsIn = mHome/WarehouseDir/WarehouseWorkspaceDir/mrArgs.Name;
                std::filesystem::create_directories(ws);
                std::filesystem::create_directories(wsIn);
                for (auto&& x : mrArgs.Modules) {
                    if (const auto i = mIndex.find(x.Name); i!=mIndex.end()) {
                        // insert the requested module itself
                        mRequests.insert_or_assign(x.Name, x.InTree ? ws/i->second->Id() : wsIn/x.Name);
                    }
                }
                for (auto&& x : mrArgs.Modules) {
                    if (const auto i = mIndex.find(x.Name); i!=mIndex.end()) {
                        const auto mif = i->second->GetContentPath()/"module.json";
                        const auto list = Json::Load(mif);
                        const std::unordered_map<std::string, std::string> imports = list["import"];
                        std::vector<std::string> depends = list["depends"];
                        // try resolve all dependency names
                        for (auto&& y: depends) {
                            std::string rKey{}, rRep;
                            for (auto&&[key, rep]:  imports) {
                                if (y.find(key)==0) if (key.length()>rKey.length()) (rKey = key, rRep = rep); // NOLINT
                            }
                            if (!rKey.empty()) y = rRep+y.substr(rKey.length()); // NOLINT
                        }
                        for (auto&& y: depends) {
                            if (mIndex.find(y)!=mIndex.end()) mRequests.insert_or_assign(y, wsIn/y);
                        }
                    }
                }
            }

            void PathLink() {
                for (auto&&[uri, pth]: mRequests) {
                    std::filesystem::create_directory_symlink(mIndex[uri]->GetContentPath(), pth);
                }
            }

            void WriteOut() {
                auto list = nlohmann::json::object();
                for (auto&& [_, x] : mRequests) x = x.lexically_proximate(mHome);
                list["checkout"] = mRequests;
                auto roots = nlohmann::json::object();
                for (auto&& x : mrArgs.Modules) {
                    auto root = nlohmann::json::object();
                    root["inTree"] = x.InTree;
                    roots[x.Name] = root;
                }
                list["roots"]=std::move(roots);
                Json::Save(mHome/WarehouseDir/WarehouseWorkspaceDir/mrArgs.Name/InfoPath, list);
            }

            Warehouse& mrHouse;
            std::filesystem::path mHome;
            const Warehouse::CheckoutArgs& mrArgs;
            std::unordered_map<std::string, Module*> mIndex;
            std::unordered_map<std::string, std::filesystem::path> mRequests;
        };
    }

    Workspace::Workspace(const std::filesystem::path& home) {

    }

    void Workspace::Reload() {

    }

    void Workspace::Update() {
        std::vector<std::nested_exception> exceptions{};
        for (auto& m : mList) {
            try { std::get<1>(m)->Update(); }
            catch (...) { exceptions.emplace_back(); }
        }
        if (!exceptions.empty()) throw Utils::AggregateException(std::move(exceptions));
    }

    namespace InterOp {
        Workspace WorkspaceCheckoutHelper(
                Warehouse& warehouse,
                const std::filesystem::path& home,
                const Warehouse::CheckoutArgs& args
        ) {
            return Checkout(warehouse, home, args).Run();
        }
    }
}