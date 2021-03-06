#include <hook/shell.hpp>
#if defined(_WIN32)
/** put win32 stuff here */
#else
#include <util/threadpool.h>
#include <util/logger.hpp>
#include <sys/wait.h>
#include <unistd.h>
#endif
#if defined(Darwin)
#include <crt_externs.h>
#endif

namespace llarp
{
  namespace hooks
  {
#if defined(_WIN32)
    Backend_ptr ExecShellBackend(std::string)
    {
      return nullptr;
    }
#else
    struct ExecShellHookJob
    {
      const std::string &m_File;
      const std::unordered_map< std::string, std::string > m_env;
      ExecShellHookJob(
          const std::string &f,
          const std::unordered_map< std::string, std::string > _env)
          : m_File(f), m_env(std::move(_env))
      {
      }

      static void
      Exec(void *user)
      {
        ExecShellHookJob *self = static_cast< ExecShellHookJob * >(user);
        std::vector< std::string > _args;
        std::vector< char * > args;
        std::istringstream s(self->m_File);
        for(std::string arg; std::getline(s, arg, ' ');)
        {
          _args.emplace_back(std::move(arg));
          char *ptr = (char *)_args.back().c_str();
          args.push_back(ptr);
        }
        args.push_back(0);
        std::vector< std::string > _env(self->m_env.size() + 1);
        std::vector< char * > env;
        // copy environ
#if defined(Darwin)
        char **ptr = *_NSGetEnviron();
#else
        char **ptr = environ;
#endif
        do
        {
          env.emplace_back(*ptr);
          ++ptr;
        } while(ptr && *ptr);
        // put in our variables
        for(const auto &item : self->m_env)
        {
          _env.emplace_back(item.first + "=" + item.second);
          char *ptr = (char *)_env.back().c_str();
          env.push_back(ptr);
        }
        env.push_back(0);
        const auto exe  = _args[0].c_str();
        const auto argv = args.data();
        const auto argp = env.data();

        pid_t child_process = ::fork();
        if(child_process == -1)
        {
          LogError("failed to fork");
          delete self;
          return;
        }
        if(child_process)
        {
          int status = 0;
          ::waitpid(child_process, &status, 0);
          LogInfo(_args[0], " exit code: ", status);
          delete self;
        }
        else if(::execve(exe, argv, argp) == -1)
        {
          LogError("failed to exec ", _args[0], " : ", strerror(errno));
        }
      }
    };

    struct ExecShellHookBackend : public IBackend
    {
      llarp_threadpool *m_ThreadPool;
      const std::string m_ScriptFile;

      ExecShellHookBackend(std::string script)
          : m_ThreadPool(llarp_init_threadpool(1, script.c_str()))
          , m_ScriptFile(std::move(script))
      {
      }

      ~ExecShellHookBackend()
      {
        llarp_threadpool_stop(m_ThreadPool);
        llarp_free_threadpool(&m_ThreadPool);
      }

      bool
      Start() override
      {
        llarp_threadpool_start(m_ThreadPool);
        return true;
      }

      bool
      Stop() override
      {
        llarp_threadpool_stop(m_ThreadPool);
        return true;
      }

      void
      NotifyAsync(
          std::unordered_map< std::string, std::string > params) override
      {
        ExecShellHookJob *job =
            new ExecShellHookJob(m_ScriptFile, std::move(params));
        llarp_threadpool_queue_job(m_ThreadPool,
                                   {job, &ExecShellHookJob::Exec});
      }
    };

    Backend_ptr
    ExecShellBackend(std::string execFilePath)
    {
      Backend_ptr ptr = std::make_unique< ExecShellHookBackend >(execFilePath);
      if(!ptr->Start())
        return nullptr;
      return ptr;
    }
#endif
  }  // namespace hooks
}  // namespace llarp
