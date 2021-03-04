#include "Message.h"

Message::Message(IRC::UUID uuid, IRC::RawMessage message) :
    uuid_(uuid),
    raw_message_(message)
{}

Message::~Message()
{}

auto Message::GetClientUUID() const noexcept -> const IRC::UUID {
    return uuid_;
}

auto Message::GetCommand() const noexcept -> const std::string &{
    return raw_message_.command.name;
}

auto Message::GetParams() const noexcept -> const std::vector<std::string> & {
    return raw_message_.command.parameters;
}

auto Message::GetServername() const noexcept -> const std::optional<std::string>
{
    if (raw_message_.prefix)
        return raw_message_.prefix->name;
    return std::nullopt;
}

auto Message::GetNickname() const noexcept -> const std::optional<std::string>
{
    if (raw_message_.prefix)
        return raw_message_.prefix->name;
    return std::nullopt;
}

