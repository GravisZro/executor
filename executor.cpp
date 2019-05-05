// POSIX
#include <sys/resource.h>

// PUT
#include <put/cxxutils/posix_helpers.h>
#include <put/cxxutils/socket_helpers.h>
#include <put/cxxutils/misc_helpers.h>
#include <put/cxxutils/hashing.h>
#include <put/cxxutils/vterm.h>

#define PARSE_ERROR_RETURN(x)     (01 + x)
#define POSIX_LIMIT_RETURN(x)     (10 + x)
#define POSIX_PRIORITY_RETURN(x)  (20 + x)
#define POSIX_ID_RETURN(x)        (30 + x)
#define LINUX_LIMIT_RETURN(x)     (40 + x)

typedef uint32_t malterminator; // ensure malformed multibyte strings are terminated

struct entry_t
{
  uint16_t bytewidth;
  uint16_t count;
  char* data;

  entry_t(void) noexcept
    : bytewidth(0),
      count(0),
      data(NULL) { }

  ~entry_t(void) noexcept
  {
    if(data != NULL)
      posix::free(data);
    data = NULL;
  }

  bool reserve(void) noexcept
  {
    data = static_cast<char*>(posix::malloc(size() + 1)); // add one for string terminator
    data[size()] = '\0'; // add string terminator
    return data != NULL;
  }

  posix::size_t   size(void) const noexcept { return bytewidth * count; }
  posix::ssize_t ssize(void) const noexcept { return bytewidth * count; }
};

static bool starts_with(entry_t* entry, const char* const str)
  { return posix::memcmp(entry->data, str, posix::strlen(str)) == 0; }


static void strtoargs(entry_t* entry, char** array, size_t arr_length)
{
  char** arg_pos = array;
  char** arg_end = array + arr_length;
  *arg_pos = entry->data; // store first argument;

  bool quote_open = false;
  for(size_t pos = 0; pos < entry->count && arg_pos != arg_end; ++pos)
  {
    if(posix::isspace(entry->data[pos]))
    {
      if(!quote_open)
      {
        entry->data[pos] = 0;
        ++arg_pos;
        if(arg_pos != arg_end)
          *arg_pos = &entry->data[pos + 1];
      }
    }
    else if(entry->data[pos] == '\\' &&
            pos + 1 < entry->count)
    {
      switch(entry->data[pos + 1])
      {
        case 't' : entry->data[pos] = '\t'; break;
        case 'n' : entry->data[pos] = '\n'; break;
        case 'v' : entry->data[pos] = '\v'; break;
        case 'f' : entry->data[pos] = '\f'; break;
        case 'r' : entry->data[pos] = '\r'; break;
        case '"' : entry->data[pos] = '"' ; break;
        case ' ' : entry->data[pos] = ' ' ; break;
        case '\\': entry->data[pos] = '\\'; break;
        default:
          continue; // skip resizing!
      }
      for(size_t mpos = pos + 2; mpos < entry->count; ++mpos) // shift the rest of the string one char
        entry->data[mpos - 1] = entry->data[mpos];
      --entry->count; // reduce size of string
      entry->data[entry->count] = 0; // terminate properly
    }
    else if(entry->data[pos] == '"')
    {
      quote_open ^= true; // toggle

      for(size_t mpos = pos + 1; mpos < entry->count; ++mpos) // shift the rest of the string one char
        entry->data[mpos - 1] = entry->data[mpos];
      --entry->count; // reduce size of string
      entry->data[entry->count] = 0; // terminate properly
    }
  }
}


//#define DEBUGABLE_TEST

int main(int /*argc*/ , char * /*argv*/ [])
{
  char* arguments[0x100] = { NULL };
  char* executable    = nullptr;
  char* workingdir    = nullptr;
  char* priority      = nullptr;
  char* user          = nullptr;
  char* group         = nullptr;
  char* euser         = nullptr;
  char* egroup        = nullptr;
  char* limit_core    = nullptr;
  char* limit_cpu     = nullptr;
  char* limit_data    = nullptr;
  char* limit_fsize   = nullptr;
  char* limit_nofile  = nullptr;
  char* limit_stack   = nullptr;
  char* limit_as      = nullptr;

#if defined(__linux__)
  char* limit_rss         = nullptr;
  char* limit_nproc       = nullptr;
  char* limit_memlock     = nullptr;
  char* limit_locks       = nullptr;
  char* limit_sigpending  = nullptr;
  char* limit_msgqueue    = nullptr;
  char* limit_nice        = nullptr;
  char* limit_rtprio      = nullptr;
  char* limit_rttime      = nullptr;
#endif

  entry_t entry_data[1024]; // 512 possible entry pairs
  entry_t* entry_pos = entry_data;
  entry_t* entry_end = entry_data + arraylength(entry_data);

#ifdef DEBUGABLE_TEST
#undef STDIN_FILENO
#define STDIN_FILENO  iopipe[0]

  int iopipe[2] = { 0 };

  if(!posix::pipe(iopipe))
  {
    posix::printf("error: %s\n", posix::strerror(errno));
    posix::fflush(stdout);
    return EXIT_FAILURE;
  }
  char input_data[] =
  {
    1, 0,
    sizeof("/Process/Arguments") - 1, 0,
    '/', 'P', 'r', 'o', 'c', 'e', 's', 's', '/', 'A', 'r', 'g', 'u', 'm', 'e', 'n', 't', 's',

    1, 0,
    sizeof("/bin/sh -c \"echo Hello World\"") - 1, 0,
    '/', 'b', 'i', 'n', '/', 's', 'h', ' ', '-', 'c', ' ', '\"', 'e', 'c', 'h', 'o', ' ', 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '\"',

    1, 0,
    sizeof("Execute") - 1, 0,
    'L', 'a', 'u', 'n', 'c', 'h',
  };
  posix::write(iopipe[1], input_data, sizeof(input_data));
#endif

  pollfd pset = { STDIN_FILENO, POLLIN, 0 };
  bool ok = true;
  bool done = false;
  bool iscmd = true;

  while(ok && !done)
  {
    ok = entry_pos < entry_end; // ensure we're still within the data buffer
    if(ok)
      ok = posix::poll(&pset, 1, 1000) != posix::error_response; // have new entry to read
    if(ok)
      ok = posix::read(STDIN_FILENO, &entry_pos->bytewidth, sizeof(uint16_t)) == sizeof(uint16_t); // reading entry type bytewidth worked
    if(ok)
      ok = posix::read(STDIN_FILENO, &entry_pos->count    , sizeof(uint16_t)) == sizeof(uint16_t); // reading entry type count worked
    if(ok)
      ok = entry_pos->reserve();
    if(ok)
      ok = posix::read(STDIN_FILENO, entry_pos->data, entry_pos->size()) == entry_pos->ssize();

    if(ok)
    {
      if(iscmd) // if even numbered string then it's a key
      {
        if(entry_pos->bytewidth == 1 && // key is a narrow character string AND
           entry_pos->count == 7 && // it's seven characters long AND
           !posix::strcmp(entry_pos->data, "Execute")) // is "Execute"
          done = true; // we are done!
        else
          ok &= entry_pos->bytewidth == 1 && // key is a narrow character string
                entry_pos->data[0] == '/'; // key must begin with an absolute path
      }
      iscmd ^= true;
    }

    if(ok)
      ++entry_pos; // move to next entry
    else
      return PARSE_ERROR_RETURN(0);
  }

  entry_end = entry_pos - 1; // save new ending location (minus the "Launch" entry)
  for(entry_pos = entry_data; // return to start
      entry_pos <= entry_end; // exit when you reach the end
      entry_pos += 2) // move toward the end
  {
    entry_t* key = entry_pos;
    entry_t* value = entry_pos + 1;

    switch(hash(key->data, key->count)) // exclude null terminator from count
    {
      case "/Process/Executable"_hash:
        executable = value->data;
      break;

      case "/Process/WorkingDirectory"_hash:
        workingdir = value->data;
      break;

      case "/Process/Arguments"_hash:
        strtoargs(value, arguments, arraylength(arguments));
        break;

      case "/Process/Priority"_hash:
        priority = value->data;
        break;

      case "/Process/User"_hash:
        user = value->data;
        break;

      case "/Process/Group"_hash:
        group = value->data;
        break;

      case "/Process/EffectiveUser"_hash:
        euser = value->data;
        break;

      case "/Process/EffectiveGroup"_hash:
        egroup = value->data;
        break;

// POSIX
      case "/Limits/CoreDumpSize"_hash: // LimitCORE
        limit_core = value->data;
        break;
      case "/Limits/CPUTime"_hash: // LimitCPU
        limit_cpu = value->data;
        break;
      case "/Limits/DataSegmentSize"_hash: // LimitDATA
        limit_data = value->data;
        break;
      case "/Limits/FileSize"_hash: // LimitFSIZE
        limit_fsize = value->data;
        break;
      case "/Limits/FilesOpen"_hash: // LimitNOFILE
        limit_nofile = value->data;
        break;
      case "/Limits/StackSize"_hash: // LimitSTACK
        limit_stack = value->data;
        break;
      case "/Limits/AddressSpaceSize"_hash: // LimitAS
        limit_as = value->data;
        break;

#if defined(__linux__) // Linux
      case "/Linux/Limits/RSS"_hash: // LimitRSS
        limit_rss = value->data;
        break;
      case "/Linux/Limits/NPROC"_hash: // LimitNPROC
        limit_nproc = value->data;
        break;
      case "/Linux/Limits/MEMLOCK"_hash: // LimitMEMLOCK
        limit_memlock = value->data;
        break;
      case "/Linux/Limits/LOCKS"_hash: // LimitLOCKS
        limit_locks = value->data;
        break;
      case "/Linux/Limits/SIGPENDING"_hash: // LimitSIGPENDING
        limit_sigpending = value->data;
        break;
      case "/Linux/Limits/MSGQUEUE"_hash: // LimitMSGQUEUE
        limit_msgqueue = value->data;
        break;
      case "/Linux/Limits/NICE"_hash: // LimitNICE
        limit_nice = value->data;
        break;
      case "/Linux/Limits/RTPRIO"_hash: // LimitRTPRIO
        limit_rtprio = value->data;
        break;
      case "/Linux/Limits/RTTIME"_hash: // LimitRTTIME
        limit_rttime = value->data;
        break;
#endif

      default: // for indeterminate keys
        if(starts_with(key, "/Environment/"))
        {
          if(::setenv(key->data + sizeof("/Environment/") - 1, value->data, 1))
            return PARSE_ERROR_RETURN(1);
        }
      break;
    }
  }

// <limits>
  struct rlimit lim;

  if(limit_core != nullptr && // value is set
     (posix::atoi(limit_core) || limit_core[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_CORE, &(lim = { 0, rlim_t(posix::atoi(limit_core)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(0);

  if(limit_cpu != nullptr && // value is set
     (posix::atoi(limit_cpu) || limit_cpu[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_CPU, &(lim = { 0, rlim_t(posix::atoi(limit_cpu)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(1);

  if(limit_data != nullptr && // value is set
     (posix::atoi(limit_data) || limit_data[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_DATA, &(lim = { 0, rlim_t(posix::atoi(limit_data)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(2);

  if(limit_fsize != nullptr && // value is set
     (posix::atoi(limit_fsize) || limit_fsize[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_FSIZE, &(lim = { 0, rlim_t(posix::atoi(limit_fsize)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(3);

  if(limit_nofile != nullptr && // value is set
     (posix::atoi(limit_nofile) || limit_nofile[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_NOFILE, &(lim = { 0, rlim_t(posix::atoi(limit_nofile)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(4);

  if(limit_stack != nullptr && // value is set
     (posix::atoi(limit_stack) || limit_stack[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_STACK, &(lim = { 0, rlim_t(posix::atoi(limit_stack)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(5);

  if(limit_as != nullptr && // value is set
     (posix::atoi(limit_as) || limit_as[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_AS, &(lim = { 0, rlim_t(posix::atoi(limit_as)) })) == posix::error_response) // ensure limit was set
    return POSIX_LIMIT_RETURN(6);

#if defined(__linux__)
  if(limit_rss != nullptr && // value is set
     (posix::atoi(limit_rss) || limit_rss[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_RSS, &(lim = { 0, rlim_t(posix::atoi(limit_rss)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(0);

  if(limit_nproc != nullptr && // value is set
     (posix::atoi(limit_nproc) || limit_nproc[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_NPROC, &(lim = { 0, rlim_t(posix::atoi(limit_nproc)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(1);

  if(limit_memlock != nullptr && // value is set
     (posix::atoi(limit_memlock) || limit_memlock[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_MEMLOCK, &(lim = { 0, rlim_t(posix::atoi(limit_memlock)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(2);

  if(limit_locks != nullptr && // value is set
     (posix::atoi(limit_locks) || limit_locks[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_LOCKS, &(lim = { 0, rlim_t(posix::atoi(limit_locks)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(3);

  if(limit_sigpending != nullptr && // value is set
     (posix::atoi(limit_sigpending) || limit_sigpending[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_SIGPENDING, &(lim = { 0, rlim_t(posix::atoi(limit_sigpending)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(4);

  if(limit_msgqueue != nullptr && // value is set
     (posix::atoi(limit_msgqueue) || limit_msgqueue[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_MSGQUEUE, &(lim = { 0, rlim_t(posix::atoi(limit_msgqueue)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(5);

  if(limit_nice != nullptr && // value is set
     (posix::atoi(limit_nice) || limit_nice[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_NICE, &(lim = { 0, rlim_t(posix::atoi(limit_nice)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(6);

  if(limit_rtprio != nullptr && // value is set
     (posix::atoi(limit_rtprio) || limit_rtprio[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_RTPRIO, &(lim = { 0, rlim_t(posix::atoi(limit_rtprio)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(7);

  if(limit_rttime != nullptr && // value is set
     (posix::atoi(limit_rttime) || limit_rttime[0] == '0') && // check if value is numeric
     ::setrlimit(RLIMIT_RTTIME, &(lim = { 0, rlim_t(posix::atoi(limit_rttime)) })) == posix::error_response) // ensure limit was set
    return LINUX_LIMIT_RETURN(8);
#endif
// </limits>

  if(priority != nullptr &&
     ::setpriority(PRIO_PROCESS, id_t(posix::getpid()), posix::atoi(priority)) == posix::error_response) // set priority
    return POSIX_PRIORITY_RETURN(0);

  if(workingdir != nullptr &&
     posix::chdir(workingdir) == posix::error_response) // set working director
     return POSIX_PRIORITY_RETURN(1);


  if(egroup != nullptr &&
     (posix::getgroupid(egroup) == gid_t(posix::error_response) ||
      !posix::setegid(posix::getgroupid(egroup)))) // set Effective GID
    return POSIX_ID_RETURN(0);

  if(euser != nullptr &&
     (posix::getuserid(euser) == uid_t(posix::error_response) ||
      !posix::seteuid(posix::getuserid(euser)))) // set Effective UID
    return POSIX_ID_RETURN(1);

  if(group != nullptr &&
     (posix::getgroupid(group) == gid_t(posix::error_response) ||
      !posix::setgid(posix::getgroupid(group)))) // set GID
    return POSIX_ID_RETURN(2);

  if(user != nullptr &&
     (posix::getuserid(user) == uid_t(posix::error_response) ||
      !posix::setuid(posix::getuserid(user)))) // set UID
    return POSIX_ID_RETURN(3);

  if(arguments[0] == NULL)
    return PARSE_ERROR_RETURN(2);

  if(executable == nullptr) // assume the first argument is the executable name
    executable = arguments[0];
/*
  terminal::write("executable : \"%s\"\n", executable);
  for(char** pos = arguments; *pos != NULL; ++pos)
    terminal::write("arg : \"%s\"\n", *pos);
  terminal::write("done\n");
*/
  if(executable == NULL) // shoudn't be possible
    return PARSE_ERROR_RETURN(3);

  if(executable[0] == '/') // if includes path
    return posix::execv(executable, arguments);

  return posix::execvp(executable, arguments); // just the filename
}
