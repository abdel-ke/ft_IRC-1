#include "Numerics.h"
#include "MessageHandlers/KILLHandler.h"

#define NICKNAME_PARAM 0

auto KILLHandler(std::shared_ptr<IClientDatabase> client_database, IMessage &message) -> void
{
    std::shared_ptr<IRC::Mutex<IClient>> client = message.GetClient();

	auto params = message.GetParams();
	if (params.size() < 2)
	{
		client->Take()->Push(std::to_string(ERR_NEEDMOREPARAMS));
		return;
	}

	// TODO: Check if sending client has privileges in the given channel and return ERR_NOPRIVILEGES

	auto nickname = params[NICKNAME_PARAM];

	// Attempt to KILL all clients with the given username.
	if (auto otherClient = client_database->Find(nickname))
	{
		if ((*otherClient)->Take()->GetType() == IClient::Type::kServer)
		{
			client->Take()->Push(std::to_string(ERR_CANTKILLSERVER));
			return;
		}

		// TODO: Disconnect the selected user.

	}
	else
	{
		// TODO: Forward to other known servers?
		client->Take()->Push(std::to_string(ERR_NOSUCHNICK));
		return;
	}
}