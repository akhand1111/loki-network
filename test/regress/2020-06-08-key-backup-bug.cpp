#include <llarp.h>
#include <llarp.hpp>
#include <config/config.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <catch2/catch.hpp>

/// make a llarp_main* with 1 endpoint that specifies a keyfile
llarp_main*
make_context(std::optional<fs::path> keyfile)
{
  auto config = llarp_default_config();
  REQUIRE(config != nullptr);
  config->impl.network.m_endpointType = "null";
  config->impl.network.m_keyfile = keyfile;
  config->impl.bootstrap.skipBootstrap = true;
  config->impl.api.m_enableRPCServer = false;
  auto ptr = llarp_main_init_from_config(config, false);
  REQUIRE(ptr != nullptr);
  llarp_config_free(config);
  return ptr;
}

/// test that we dont back up all keys when self.signed is missing or invalid as client
TEST_CASE("key backup bug regression test", "[regress]")
{
  // kill logging, this code is noisy
  llarp::LogSilencer shutup;
  // test 2 explicitly provided keyfiles, empty keyfile and no keyfile
  for (std::optional<fs::path> path : {std::optional<fs::path>{"regress-1.private"},
                                       std::optional<fs::path>{"regress-2.private"},
                                       std::optional<fs::path>{""},
                                       {std::nullopt}})
  {
    llarp::service::Address endpointAddress{};
    // try 10 start up and shut downs and see if our key changes or not
    for (size_t index = 0; index < 10; index++)
    {
      auto context = make_context(path);
      REQUIRE(llarp_main_setup(context, false) == 0);
      auto ctx = llarp::Context::Get(context);
      ctx->CallSafe([ctx, index, &endpointAddress, &path]() {
        auto ep = ctx->router->hiddenServiceContext().GetDefault();
        REQUIRE(ep != nullptr);
        if (index == 0)
        {
          REQUIRE(endpointAddress.IsZero());
          // first iteration, we are getting our identity that we start with
          endpointAddress = ep->GetIdentity().pub.Addr();
          REQUIRE(not endpointAddress.IsZero());
        }
        else
        {
          REQUIRE(not endpointAddress.IsZero());
          if (path.has_value() and not path->empty())
          {
            // we have a keyfile provided
            // after the first iteration we expect the keys to stay the same
            REQUIRE(endpointAddress == ep->GetIdentity().pub.Addr());
          }
          else
          {
            // we want the keys to shift because no keyfile was provided
            REQUIRE(endpointAddress != ep->GetIdentity().pub.Addr());
          }
        }
        // close the router right away
        ctx->router->Die();
      });
      REQUIRE(llarp_main_run(context, llarp_main_runtime_opts{}) == 0);
      llarp_main_free(context);
    }
    // remove keys if provied
    if (path.has_value() and not path->empty())
      fs::remove(*path);
  }
}
