#pragma once
#include "config.hpp"
#include "runtime_provider.hpp"
namespace esp {
struct RenderStats {std::size_t total{},visible{},drawn{};};
RenderStats RenderEntities(const Frame&,const VisualConfig&,float width,float height,const RuntimeDiagnostics&);
void RenderMenu(VisualConfig&,bool& menuOpen,const std::filesystem::path&,std::string& status,const std::string& offsetsStatus,bool& restartSource);
}
