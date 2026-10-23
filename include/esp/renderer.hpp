#pragma once
#include "config.hpp"
#include "data_provider.hpp"
namespace esp {
void RenderEntities(const Frame&,const VisualConfig&,float width,float height);
void RenderMenu(VisualConfig&,bool& menuOpen,const std::filesystem::path&,std::string& status,const std::string& offsetsStatus);
}
