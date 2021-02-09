#include <metall/metall.hpp>

int main() {

  {

    metall::manager manager(metall::create_only, "/mnt/ssd/t1");

  }

 

  {

    metall::manager manager(metall::open_only, "/mnt/ssd/t1");

  }

  return 0;

}
