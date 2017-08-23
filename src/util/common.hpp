#pragma once

#include <errno.h>
#include <stdlib.h>

//#define VERIFY(expression, format, ...) do{ if(expression == false) throw deepfabric::Exception(SOURCE, format, ##__VA_ARGS__ ); } while(0)
#define ASSERT(expression) do{ if((expression) == false) abort(); } while(0)

struct file_deleter {
  void operator()(FILE* f) const { 
    if (f) ::fclose(f); 
  }
};

typedef std::unique_ptr<FILE, file_deleter> handle_t;
