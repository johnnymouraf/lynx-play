#include "remoteplay/ClientDuid.h"

#include <chiaki/remote/holepunch.h>

namespace mfplay::remoteplay
{

std::expected<ClientDuid, std::string> resolveClientDuid(const std::string& saved,
                                                         const DuidGenerator& generate)
{
    if (!saved.empty())
    {
        return ClientDuid{.value = saved, .generated = false};
    }

    auto created = generate();

    if (!created)
    {
        return std::unexpected(std::move(created.error()));
    }

    if (created->empty())
    {
        return std::unexpected("the generator returned an empty identifier");
    }

    return ClientDuid{.value = std::move(*created), .generated = true};
}

std::expected<std::string, std::string> generateClientDuid()
{
    char buffer[CHIAKI_DUID_STR_SIZE] = {};
    size_t size = sizeof(buffer);

    const ChiakiErrorCode result = chiaki_holepunch_generate_client_device_uid(buffer, &size);

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_holepunch_generate_client_device_uid failed: ") +
                               chiaki_error_string(result));
    }

    return std::string(buffer);
}

} // namespace mfplay::remoteplay
