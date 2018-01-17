// PDTK
#include <cxxutils/posix_helpers.h>
#include <cxxutils/misc_helpers.h>
#include <cxxutils/hashing.h>

// POSIX++
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

// POSIX
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/resource.h>


typedef uint32_t malterminator; // ensure malformed multibyte strings are terminated

struct entry_t
{
  uint16_t bytewidth;
  uint16_t count;
  char* data;

  constexpr posix::size_t   size(void) const { return bytewidth * count; }
  constexpr posix::ssize_t ssize(void) const { return bytewidth * count; }
};

static bool starts_with(entry_t* entry, const char* str)
{
  return std::strcmp(entry->data, str) == 0;
}


static void strtoargs(entry_t* entry, char** array, size_t arr_length)
{
  char** arg_pos = array;
  char** arg_end = array + arr_length;
  *arg_pos = entry->data; // store first argument;

  bool quote_open = false;
  for(size_t pos = 0; pos < entry->count && arg_pos != arg_end; ++pos)
  {
    if(::isspace(entry->data[pos]))
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


int main(int argc, char *argv[])
{
  char* arguments[0x100] = { nullptr };
  char* executable  = nullptr;
  char* workingdir  = nullptr;
  char* priority    = nullptr;
  char* user        = nullptr;
  char* group       = nullptr;
  char* euser       = nullptr;
  char* egroup      = nullptr;



  entry_t entry_data[0x0020000] = { { 0, 0, nullptr } }; // 128K possible entries
  entry_t* entry_pos = entry_data;
  entry_t* entry_end = entry_data + arraylength(entry_data);

  char string_data[0x00400000] = { 0 }; // 4MiB string data buffer
  char* string_pos = string_data;
  char* string_end = string_data + arraylength(string_data);

#if 0
  int iopipe[2] = { STDIN_FILENO, STDOUT_FILENO };

  if(pipe(iopipe) == -1)
  {
    printf("error: %s\n", strerror(errno));
    fflush(stdout);
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
    sizeof("Launch") - 1, 0,
    'L', 'a', 'u', 'n', 'c', 'h',
  };
  ::write(iopipe[1], input_data, sizeof(input_data));
#endif

  pollfd pset = { STDIN_FILENO, POLLIN, 0 };
  bool ok = true;
  bool done = false;
  bool iskey = true;

  while(ok && !done)
  {
    if(ok)
      ok &= ::poll(&pset, 1, 1000) > 0;
    if(ok)
      ok &= ::read(STDIN_FILENO, &entry_pos->bytewidth, sizeof(uint16_t)) == sizeof(uint16_t); // reading entry type bytewidth worked
    if(ok)
      ok &= ::read(STDIN_FILENO, &entry_pos->count, sizeof(uint16_t)) == sizeof(uint16_t); // reading entry type count worked
    if(ok)
    {
      entry_pos->data = string_pos;
      ok &= ::read(STDIN_FILENO, string_pos, entry_pos->size()) == entry_pos->ssize();
    }

    if(ok)
    {
      if(iskey) // if even numbered string then it's a key
      {
        if(entry_pos->bytewidth == 1 && // key is a narrow character string AND
           entry_pos->count == 6 && // it's six characters long AND
           string_pos[0] == 'L' && // is "Launch"
           string_pos[1] == 'a' &&
           string_pos[2] == 'u' &&
           string_pos[3] == 'n' &&
           string_pos[4] == 'c' &&
           string_pos[5] == 'h')
          done = true; // we are done!
        else
          ok &= entry_pos->bytewidth == 1 && // key is a narrow character string
                string_pos[0] == '/'; // key must begin with an absolute path
      }
      iskey ^= true;
    }

    if(ok)
    {
      string_pos += entry_pos->size() + sizeof(malterminator); // add four \0 (null terminator) chars to eliminate malformed multibyte strings from overflowing
      ++entry_pos; // move to next entry

      ok &= string_pos < string_end && // ensure we're still within the data buffer
            entry_pos < entry_end;
    }
  }

  if(!ok || !done)
    return EXIT_FAILURE;

  for(entry_end = entry_pos - 1, // save new ending location (minus the "Launch" entry)
      entry_pos = entry_data; // return to start
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
      {
        printf("converting : %s\n", value->data);
        strtoargs(value, arguments, arraylength(arguments));
        for(char** pos = arguments; *pos; ++pos)
        {
          printf("arg : \"%s\"\n", *pos);
        }
        printf("done\n");
        fflush(stdout);
      }
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

      case "/ResourceLimits/CPUTime"_hash:
        break;

      default:
        if(starts_with(key, "/Environment/"))
        {
          if(::setenv(key->data + sizeof("/Environment/") - 1, value->data, 1))
            return EXIT_FAILURE;
        }


      break;
    }
  }

  if(priority != nullptr &&
     ::setpriority(PRIO_PROCESS, id_t(getpid()), std::atoi(priority)) == posix::error_response) // set priority
    return EXIT_FAILURE;

  if(user != nullptr &&
     (posix::getuserid(user) == gid_t(posix::error_response) ||
      ::setuid(posix::getuserid(user)) == posix::error_response))
    return EXIT_FAILURE;

  if(group != nullptr &&
     (posix::getgroupid(group) == uid_t(posix::error_response) ||
      ::setgid(posix::getgroupid(group)) == posix::error_response))
    return EXIT_FAILURE;

  if(euser != nullptr &&
     (posix::getuserid(euser) == gid_t(posix::error_response) ||
      ::seteuid(posix::getuserid(euser)) == posix::error_response))
    return EXIT_FAILURE;

  if(egroup != nullptr &&
     (posix::getgroupid(egroup) == uid_t(posix::error_response) ||
      ::setegid(posix::getgroupid(egroup)) == posix::error_response))
    return EXIT_FAILURE;

//  if(::setrlimit(limit_id, &val) == posix::error_response)
//    return EXIT_FAILURE;




  if(arguments[0] == nullptr)
    return EXIT_FAILURE;

  if(executable == nullptr) // assume the first argument is the executable name
    executable = arguments[0];

  return ::execv(executable, const_cast<char* const*>(arguments));
}
