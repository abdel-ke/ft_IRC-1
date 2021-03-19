#include <map>

#include "Numerics.h"
#include "Utilities.h"
#include "ChannelDatabase.h"
#include "MessageHandlers/JOINHandler.h"

#define CHANNEL_NAME_PARAM 0
#define CHANNEL_KEYS_PARAM 1

JOINHandler::JOINHandler(IClientDatabase *client_database, IChannelDatabase *channel_database)
	: client_database_(client_database), channel_database_(channel_database), logger("JOINHandler")
{}

JOINHandler::~JOINHandler()
{}

static auto CreateChannelKeyMap(std::string param_name,
	std::string param_key) -> const std::map<std::string, std::string>
{
	auto channel_keys = split(param_key, ",");
	auto channel_names = split(param_name, ",");

	std::map<std::string, std::string> nameKeyMap;

	for (size_t i = 0; i < channel_names.size(); ++i)
	{
		if (channel_keys.size() > i)
		{
			nameKeyMap[channel_names[i]] = channel_keys[i];
		}
		else
		{
			nameKeyMap[channel_names[i]] = std::string();
		}		
	}

	return (nameKeyMap);
}

static auto TryAddUserToChannel(IChannel* channel,
	const std::string key, IUser* user) -> void
{
	// TODO: Check channel modes to see if user has enough privilege.
	if (channel->GetKey() != key)
	{
		user->Push(std::to_string(ERR_BADCHANNELKEY));
		return;
	}

	channel->PushToLocal(":" + user->GetNickname() + " JOIN " + channel->GetName());
	channel->AddUser(user);
	
	user->Push(std::to_string(RPL_TOPIC) + " :" + channel->GetTopic());
	user->Push(std::to_string(RPL_NAMREPLY) + " " + channel->GetUserListAsString());
}

static auto StartJoinParsing(const std::vector<std::string> &params,
	IClient* client, IChannelDatabase *channel_database)
{
	auto keys = params.size() >= 2 ? params[CHANNEL_KEYS_PARAM] : std::string();
	auto channel_pairs = CreateChannelKeyMap(params[CHANNEL_NAME_PARAM], keys);

	for (auto kvp : channel_pairs)
	{
		// TODO: The length and prefix check should probably be part of the parser(?)
		if (kvp.first.at(0) != '#' || kvp.first.size() >= 50)
		{
			client->Push(std::to_string(ERR_NOSUCHCHANNEL));
			continue;
		}

		auto current_channel = channel_database->GetChannel(kvp.first);

		if (!current_channel)
		{
			current_channel = channel_database->CreateChannel(kvp.first, kvp.second, ChannelType::kLocal, ChannelMode::None);
		}

		TryAddUserToChannel(*current_channel, kvp.second, dynamic_cast<IUser*>(client));
	}
}

auto JOINHandler::Handle(IMessage &message) -> void
{
	auto params = message.GetParams();
    auto client = *(client_database_->GetClient(message.GetClientUUID()));

	// Handle unregistered client.
	if (client->GetState() == IClient::kUnRegistered)
	{
		client->Push(std::to_string(ERR_NOTREGISTERED));
		return;
	}	
	// Handle not enough parameters.
	if (params.size() == 0)
	{
		client->Push(std::to_string(ERR_NEEDMOREPARAMS));
		return;
	}

	// Handle server message.
	if (client->GetType() == IClient::kServer)
	{
        auto remote_client_nickname = message.GetNickname();
        if (remote_client_nickname == std::nullopt)
        {
            client->Push(std::to_string(ERR_NONICKNAMEGIVEN) + ":No nickname given"); //TODO
            return ;
        }
        //TODO validate nickname
        auto remote_client = client_database_->GetClient(*remote_client_nickname);
        if (remote_client == std::nullopt)
        {
            client->Push(std::to_string(ERR_NOSUCHNICK) + *remote_client_nickname + " :Can't find nickname"); //TODO
            return ;
        }
        client = *remote_client;
	}
	
	// TOOD: JOIN should be broadcast to all connected servers.

	StartJoinParsing(params, client, channel_database_);
}