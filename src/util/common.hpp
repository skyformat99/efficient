#pragma once

#include <errno.h>
#include <stdlib.h>

#include "log.hpp"

#define ASSERT(expression) do{ if((expression) == false) EXCEPTION(); } while(0)

struct file_deleter {
  void operator()(FILE* f) const { 
    if (f) ::fclose(f); 
  }
};

typedef std::unique_ptr<FILE, file_deleter> handle_t;
