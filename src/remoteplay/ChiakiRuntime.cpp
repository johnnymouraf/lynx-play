#include "remoteplay/ChiakiRuntime.h"

#include <chiaki/common.h>

namespace mfplay::remoteplay
{

std::expected<void, std::string> initializeChiaki()
{
    const ChiakiErrorCode result = chiaki_lib_init();

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_lib_init failed: ") +
                               chiaki_error_string(result));
    }

    return {};
}

} // namespace mfplay::remoteplay
