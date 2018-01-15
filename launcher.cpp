// PDTK
#include <cxxutils/posix_helpers.h>
#include <cxxutils/misc_helpers.h>
#include <cxxutils/hashing.h>

// POSIX++
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>


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

#include <cassert>
// modifies an entry to explode it's string into an array
static void explode(entry_t* entry, char** array, size_t arr_length, const char delim)
{
  char** arg_pos = array;
  char** arg_end = array + arr_length;
  *arg_pos = entry->data; // store first argument;

  // this is a oddly complex loop to modify the a string and fill array "arguments", look very closely
  for(char* pos = nullptr; // pos is always set when checking the condition
      arg_pos != arg_end &&
      (pos = std::strchr(*arg_pos, delim)) != nullptr; // search the current string for a space
      *(++arg_pos) = ++pos) // store start of next string part (next argument)
  {
    for(; *pos == ' '; ++pos) // while space character
      *pos = 0; // terminate string
    assert(pos < entry->data + entry->count); // this should never fail)
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

  for(entry_end = entry_pos, // save new ending location
      entry_pos = entry_data; // return to start
      entry_pos <= entry_end; // exit when you reach the end
      entry_pos += 2) // move toward the end
  {
    entry_t* key = entry_pos;
    entry_t* value = entry_pos + 1;

    switch (hash(key->data, key->count))
    {
      case "/Process/Executable"_hash:
        executable = value->data;
      break;

      case "/Process/WorkingDirectory"_hash:
        workingdir = value->data;
      break;

      case "/Process/Arguments"_hash:
        explode(value, arguments, arraylength(arguments), ' ');
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
