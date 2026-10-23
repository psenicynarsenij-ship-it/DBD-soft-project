#pragma once
#include <string>
#include <string_view>

namespace esp {
enum class SourceKind;
// Describes this binary's capabilities, not a discovered game process/session.
struct DbdConnectionStatus {
    std::string_view targetVersion="10.2.2";
    bool connected=false, adapterImplemented=false, liveSessionValidated=false;
    std::string_view state="NOT CONNECTED";
    std::string_view reason="No documented and verified DBD spatial adapter is configured.";
};
DbdConnectionStatus DescribeDbdConnection(SourceKind,bool demoFallback=false);
std::string DbdBuildStatusJson();
}
