#include "esp/dbd_connection.hpp"
#include "esp/frame_sources.hpp"
#include <nlohmann/json.hpp>

namespace esp {
DbdConnectionStatus DescribeDbdConnection(SourceKind kind,bool fallback) {
    DbdConnectionStatus status;
    if(fallback) status.reason="Synthetic fallback is not a DBD connection.";
    else switch(kind) {
    case SourceKind::LiveMemory:status.reason="Generic process reads do not verify a DBD layout or live game connection.";break;
    case SourceKind::Demo:status.reason="Synthetic demo frames are not game data.";break;
    case SourceKind::JsonFile:status.reason="A local JSON file does not verify DBD provenance or a game connection.";break;
    case SourceKind::Replay:status.reason="Playback does not establish a live game connection.";break;
    }
    return status;
}
std::string DbdBuildStatusJson() {
    const DbdConnectionStatus status;
    return nlohmann::json{
        {"schema_version",1},{"scope","binary_capabilities"},{"target_version",std::string(status.targetVersion)},
        {"state",std::string(status.state)},{"connected",status.connected},
        {"generic_external_reader",true},{"adapter_implemented",status.adapterImplemented},{"live_session_validated",status.liveSessionValidated},
        {"reason",std::string(status.reason)},
        {"missing_feed_fields",{"survivor_positions","killer_position","generator_positions","hook_positions", "exit_gate_positions","camera_position","camera_view_projection","killer_look_direction"}}
    }.dump(2);
}
}
