#include <llarp.hpp>
#include <constants/version.hpp>

#include <config/config.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <dht/context.hpp>
#include <ev/ev.hpp>
#include <ev/vpnio.hpp>
#include <nodedb.hpp>
#include <router/router.hpp>
#include <service/context.hpp>
#include <util/logging/logger.hpp>

#include <cxxopts.hpp>
#include <csignal>

#if (__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
  bool
  Context::CallSafe(std::function<void(void)> f)
  {
    return logic && LogicCall(logic, f);
  }

  bool
  Context::Configure(bool isRelay, std::optional<fs::path> dataDir)
  {
    fs::path defaultDataDir = dataDir ? *dataDir : GetDefaultDataDir();

    if (configfile.size())
    {
      if (!config->Load(configfile.c_str(), isRelay, defaultDataDir))
      {
        config.release();
        llarp::LogError("failed to load config file ", configfile);
        return false;
      }
    }

    logic = std::make_shared<Logic>();

    nodedb_dir = fs::path(config->router.m_dataDir / nodedb_dirname).string();

    return true;
  }

  bool
  Context::IsUp() const
  {
    return router && router->IsRunning();
  }

  bool
  Context::LooksAlive() const
  {
    return router && router->LooksAlive();
  }

  int
  Context::LoadDatabase()
  {
    llarp_nodedb::ensure_dir(nodedb_dir.c_str());
    return 1;
  }

  int
  Context::Setup(bool isRelay)
  {
    llarp::LogInfo(llarp::VERSION_FULL, " ", llarp::RELEASE_MOTTO);
    llarp::LogInfo("starting up");
    if (mainloop == nullptr)
    {
      auto jobQueueSize = std::max(event_loop_queue_size, config->router.m_JobQueueSize);
      mainloop = llarp_make_ev_loop(jobQueueSize);
    }
    logic->set_event_loop(mainloop.get());

    mainloop->set_logic(logic);

    crypto = std::make_unique<sodium::CryptoLibSodium>();
    cryptoManager = std::make_unique<CryptoManager>(crypto.get());

    router = std::make_unique<Router>(mainloop, logic);

    nodedb = std::make_unique<llarp_nodedb>(
        nodedb_dir,
        [r = router.get()](std::function<void(void)> call) { r->QueueDiskIO(std::move(call)); });

    if (!router->Configure(config.get(), isRelay, nodedb.get()))
    {
      llarp::LogError("Failed to configure router");
      return 1;
    }

    // must be done after router is made so we can use its disk io worker
    // must also be done after configure so that netid is properly set if it
    // is provided by config
    if (!this->LoadDatabase())
      return 2;

    return 0;
  }

  int
  Context::Run(llarp_main_runtime_opts opts)
  {
    if (router == nullptr)
    {
      // we are not set up so we should die
      llarp::LogError("cannot run non configured context");
      return 1;
    }

    if (!opts.background)
    {
      if (!router->Run())
        return 2;
    }

    // run net io thread
    llarp::LogInfo("running mainloop");

    llarp_ev_loop_run_single_process(mainloop, logic);
    if (closeWaiter)
    {
      // inform promise if called by CloseAsync
      closeWaiter->set_value();
    }
    return 0;
  }

  void
  Context::CloseAsync()
  {
    /// already closing
    if (closeWaiter)
      return;
    if (CallSafe(std::bind(&Context::HandleSignal, this, SIGTERM)))
      closeWaiter = std::make_unique<std::promise<void>>();
  }

  void
  Context::Wait()
  {
    if (closeWaiter)
    {
      closeWaiter->get_future().wait();
      closeWaiter.reset();
    }
  }

  void
  Context::HandleSignal(int sig)
  {
    if (sig == SIGINT || sig == SIGTERM)
    {
      SigINT();
    }
    // TODO: Hot reloading would be kewl
    //       (it used to exist here, but wasn't maintained)
  }

  void
  Context::SigINT()
  {
    if (router)
    {
      /// async stop router on sigint
      router->Stop();
    }
    else
    {
      if (logic)
        logic->stop();
      llarp_ev_loop_stop(mainloop);
      Close();
    }
  }

  void
  Context::Close()
  {
    llarp::LogDebug("free config");
    config.release();

    llarp::LogDebug("free nodedb");
    nodedb.release();

    llarp::LogDebug("free router");
    router.release();

    llarp::LogDebug("free logic");
    logic.reset();
  }

  bool
  Context::LoadConfig(const std::string& fname, bool isRelay)
  {
    config = std::make_unique<Config>();
    configfile = fname;
    const fs::path filepath(fname);
    return Configure(isRelay, filepath.parent_path());
  }

#ifdef LOKINET_HIVE
  void
  Context::InjectHive(tooling::RouterHive* hive)
  {
    router->hive = hive;
  }
#endif
}  // namespace llarp

struct llarp_main
{
  llarp_main(llarp_config* conf);
  ~llarp_main() = default;
  std::shared_ptr<llarp::Context> ctx;
};

llarp_config::llarp_config(const llarp_config* other) : impl(other->impl)
{
}

namespace llarp
{
  llarp_config*
  Config::Copy() const
  {
    llarp_config* ptr = new llarp_config();
    ptr->impl = *this;
    return ptr;
  }
}  // namespace llarp

extern "C"
{
  size_t
  llarp_main_size()
  {
    return sizeof(llarp_main);
  }

  size_t
  llarp_config_size()
  {
    return sizeof(llarp_config);
  }

  static llarp_config*
  _llarp_default_config(bool isRelay)
  {
    llarp_config* conf = new llarp_config();

    try
    {
      if (not conf->impl.LoadDefault(isRelay, llarp::GetDefaultDataDir()))
      {
        delete conf;
        return nullptr;
      }
    }
    catch (std::exception&)
    {
      delete conf;
      return nullptr;
    }
    if (not isRelay)
    {
#ifdef ANDROID
      // put andrid config overrides here
#endif
#ifdef IOS
      // put IOS config overrides here
#endif
    }
    return conf;
  }

  llarp_config*
  llarp_default_client_config()
  {
    return _llarp_default_config(false);
  }

  llarp_config*
  llarp_default_relay_config()
  {
    return _llarp_default_config(true);
  }

  void
  llarp_config_free(struct llarp_config* conf)
  {
    if (conf)
      delete conf;
  }

  struct llarp_main*
  llarp_main_init_from_config(struct llarp_config* conf, bool isRelay)
  {
    if (conf == nullptr)
      return nullptr;
    llarp_main* m = new llarp_main(conf);
    if (m->ctx->Configure(isRelay, {}))
      return m;
    delete m;
    return nullptr;
  }

  bool
  llarp_config_load_file(const char* fname, struct llarp_config** conf, bool isRelay)
  {
    llarp_config* c = new llarp_config();
    const fs::path filepath(fname);
    if (c->impl.Load(fname, isRelay, filepath.parent_path()))
    {
      *conf = c;
      return true;
    }
    delete c;
    *conf = nullptr;
    return false;
  }

  void
  llarp_main_signal(struct llarp_main* ptr, int sig)
  {
    LogicCall(ptr->ctx->logic, std::bind(&llarp::Context::HandleSignal, ptr->ctx.get(), sig));
  }

  int
  llarp_main_setup(struct llarp_main* ptr, bool isRelay)
  {
    return ptr->ctx->Setup(isRelay);
  }

  int
  llarp_main_run(struct llarp_main* ptr, struct llarp_main_runtime_opts opts)
  {
    return ptr->ctx->Run(opts);
  }

  const char*
  llarp_version()
  {
    return llarp::VERSION_FULL;
  }

  ssize_t
  llarp_vpn_io_readpkt(struct llarp_vpn_pkt_reader* r, unsigned char* dst, size_t dstlen)
  {
    if (r == nullptr)
      return -1;
    if (not r->queue.enabled())
      return -1;
    auto pkt = r->queue.popFront();
    ManagedBuffer mbuf = pkt.ConstBuffer();
    const llarp_buffer_t& buf = mbuf;
    if (buf.sz > dstlen || buf.sz == 0)
      return -1;
    std::copy_n(buf.base, buf.sz, dst);
    return buf.sz;
  }

  bool
  llarp_vpn_io_writepkt(struct llarp_vpn_pkt_writer* w, unsigned char* pktbuf, size_t pktlen)
  {
    if (pktlen == 0 || pktbuf == nullptr)
      return false;
    if (w == nullptr)
      return false;
    llarp_vpn_pkt_queue::Packet_t pkt;
    llarp_buffer_t buf(pktbuf, pktlen);
    if (not pkt.Load(buf))
      return false;
    return w->queue.pushBack(std::move(pkt)) == llarp::thread::QueueReturn::Success;
  }

  bool
  llarp_main_inject_vpn_by_name(
      struct llarp_main* ptr,
      const char* name,
      struct llarp_vpn_io* io,
      struct llarp_vpn_ifaddr_info info)
  {
    if (name == nullptr || io == nullptr)
      return false;
    if (ptr == nullptr || ptr->ctx == nullptr || ptr->ctx->router == nullptr)
      return false;
    auto ep = ptr->ctx->router->hiddenServiceContext().GetEndpointByName(name);
    return ep && ep->InjectVPN(io, info);
  }

  void
  llarp_vpn_io_close_async(struct llarp_vpn_io* io)
  {
    if (io == nullptr || io->impl == nullptr)
      return;
    static_cast<llarp_vpn_io_impl*>(io->impl)->AsyncClose();
  }

  bool
  llarp_vpn_io_init(struct llarp_main* ptr, struct llarp_vpn_io* io)
  {
    if (io == nullptr || ptr == nullptr)
      return false;
    llarp_vpn_io_impl* impl = new llarp_vpn_io_impl(ptr, io);
    io->impl = impl;
    return true;
  }

  struct llarp_vpn_pkt_writer*
  llarp_vpn_io_packet_writer(struct llarp_vpn_io* io)
  {
    if (io == nullptr || io->impl == nullptr)
      return nullptr;
    llarp_vpn_io_impl* vpn = static_cast<llarp_vpn_io_impl*>(io->impl);
    return &vpn->writer;
  }

  struct llarp_vpn_pkt_reader*
  llarp_vpn_io_packet_reader(struct llarp_vpn_io* io)
  {
    if (io == nullptr || io->impl == nullptr)
      return nullptr;
    llarp_vpn_io_impl* vpn = static_cast<llarp_vpn_io_impl*>(io->impl);
    return &vpn->reader;
  }

  void
  llarp_main_free(struct llarp_main* ptr)
  {
    delete ptr;
  }

  const char*
  llarp_main_get_default_endpoint_name(struct llarp_main*)
  {
    return "default";
  }

  void
  llarp_main_stop(struct llarp_main* ptr)
  {
    if (ptr == nullptr)
      return;
    ptr->ctx->CloseAsync();
    ptr->ctx->Wait();
  }

  bool
  llarp_main_configure(struct llarp_main* ptr, struct llarp_config* conf, bool isRelay)
  {
    if (ptr == nullptr || conf == nullptr)
      return false;
    // give new config
    ptr->ctx->config.reset(new llarp::Config(conf->impl));
    return ptr->ctx->Configure(isRelay, {});
  }

  bool
  llarp_main_is_running(struct llarp_main* ptr)
  {
    return ptr && ptr->ctx->router && ptr->ctx->router->IsRunning();
  }
}

llarp_main::llarp_main(llarp_config* conf)

    : ctx(new llarp::Context())
{
  ctx->config.reset(new llarp::Config(conf->impl));
}

namespace llarp
{
  std::shared_ptr<Context>
  Context::Get(llarp_main* m)
  {
    if (m == nullptr || m->ctx == nullptr)
      return nullptr;
    return m->ctx;
  }
}  // namespace llarp
