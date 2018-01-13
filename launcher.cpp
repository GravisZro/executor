// PDTK
#include <cxxutils/posix_helpers.h>
#include <cxxutils/misc_helpers.h>
#include <cxxutils/hashing.h>

// C
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// POSIX
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/resource.h>


typedef uint32_t malterminator; // ensure malformed multibyte strings are terminated

int main(int argc, char *argv[])
{
  char* exefile = nullptr;
  char* workingdir = nullptr;
  char* arguments[0x100] = { nullptr };

  struct entry_t
  {
    uint16_t bytewidth;
    uint16_t count;
    const char* data;

    constexpr posix::size_t   size(void) const { return bytewidth * count; }
    constexpr posix::ssize_t ssize(void) const { return bytewidth * count; }
  };

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
        if(entry_pos->bytewidth == 1 && // if the key is a series of characters AND
           entry_pos->count == 6 && // it's six characters long AND
           string_pos[0] == 'L' && // is "Launch"
           string_pos[1] == 'a' &&
           string_pos[2] == 'u' &&
           string_pos[3] == 'n' &&
           string_pos[4] == 'c' &&
           string_pos[5] == 'h')
          done = true; // we are done!
        else
          ok &= string_pos[0] == '/'; // otherwise the key must begin with an absolute path
      }
      iskey ^= true;
    }

    if(ok)
    {
      string_pos += entry_pos->size() + sizeof(malterminator); // add four 0 chars to eliminate malformed multibyte strings from overflowing
      ++entry_pos; // move to next entry

      ok &= string_pos < string_end && // ensure we're still within the data buffer
            entry_pos < entry_end;
    }
  }


/*
  if(::setenv(key, value, 1) == posix::error_response)
    return EXIT_FAILURE;

  if(::setpriority(PRIO_PROCESS, getpid(), priority) == posix::error_response) // set priority
    return EXIT_FAILURE;

  if(::setuid(uid) == posix::error_response)
    return EXIT_FAILURE;

  if(::setgid(gid) == posix::error_response)
    return EXIT_FAILURE;

  if(::seteuid(euid) == posix::error_response)
    return EXIT_FAILURE;

  if(::setegid(egid) == posix::error_response)
    return EXIT_FAILURE;

  if(::setrlimit(limit_id, &val) == posix::error_response)
    return EXIT_FAILURE;

  if(exefile == nullptr) // assume the first argument is the executable name
    exefile = arguments[0];
*/
  return ::execv(exefile, arguments);
}
