#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "Client.h"
#include "MockIOHandler.h"
#include "User.h"
#include "MockServer.h"
#include "MockClient.h"
#include <memory>

using ::testing::AtLeast;
using ::testing::Throw;
using ::testing::Return;
using ::testing::_;

class FakeUser : public MockClient, User
{
    public:

};

class UserTests : public ::testing::Test
{
    public:
    std::unique_ptr<MockIOHandler> unique_io_handler;
    std::shared_ptr<MockServer> mock_server;
    MockIOHandler *io_handler;
    std::shared_ptr<Client> client;

    void SetUp() override
    {
        mock_server = std::make_shared<MockServer>();
        unique_io_handler = std::make_unique<MockIOHandler>();
        io_handler = unique_io_handler.get();
        client = std::make_shared<Client>(std::move(unique_io_handler), mock_server);
    }
};

// TODO when we implement user coce
TEST_F(UserTests, todo)
{
    FakeUser hello;
}
